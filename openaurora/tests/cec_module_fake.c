/* Host fake for the module-level CEC transport. Installed by a constructor so
 * mod_cec's real acquire/send/receive/release path can run without a TV. */
#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "openaurora/cec_module_transport.h"
#include "openaurora/vendor_cec.h"

#define CEC_OPEN  UINT32_C(0x401c4301)
#define CEC_CLOSE UINT32_C(0x80014302)
#define CEC_RX    UINT32_C(0x80104303)
#define CEC_TX    UINT32_C(0xc0104304)

static uint8_t mirror_page[4096];
static atomic_int rx_delay_once;
static atomic_int tx_count;
static atomic_int open_count;
static atomic_int init_count;
static atomic_int close_count;

static int am_open(void *context, const char *path, int flags)
{
    (void)context; (void)path; (void)flags;
    return 8;
}
static void *am_map(void *context, int fd, uint64_t base, size_t size)
{
    (void)context; (void)fd; (void)base; (void)size;
    return mirror_page;
}
static int am_unmap(void *context, void *address, size_t size)
{
    (void)context; (void)address; (void)size;
    return 0;
}
static int am_close(void *context, int fd)
{
    (void)context; (void)fd;
    return 0;
}
static const oa_cec_authority_mem_io authority_mem = {
    NULL, am_open, am_map, am_unmap, am_close
};

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

#define FAKE_RX_MAX 4
static uint8_t rx_frames[FAKE_RX_MAX][16];
static int rx_lengths[FAKE_RX_MAX];
static int rx_count;
static int rx_at;

static void parse_rx_frame(const char *text)
{
    if (!strncmp(text, "raw:", 4)) text += 4;
    int n = 0;
    while (text[0] && text[1] && n < 16) {
        int hi = hex_value(text[0]);
        int lo = hex_value(text[1]);
        if (hi < 0 || lo < 0) break;
        rx_frames[rx_count][n++] = (uint8_t)((hi << 4) | lo);
        text += 2;
    }
    rx_lengths[rx_count] = n;
    if (n > 0) rx_count++;
}

static void init_rx_frames(void)
{
    if (rx_count || rx_at) return;
    const char *mode = getenv("OA_CEC_FAKE_RX");
    if (mode && !strncmp(mode, "seq:", 4)) {
        const char *p = mode + 4;
        while (*p && rx_count < FAKE_RX_MAX) {
            char item[40];
            size_t n = 0;
            while (*p && *p != ',' && n + 1 < sizeof item)
                item[n++] = *p++;
            item[n] = '\0';
            if (*p == ',') p++;
            parse_rx_frame(item);
        }
        return;
    }
    if (mode && !strcmp(mode, "9d")) {
        parse_rx_frame("5f9d1000");
        return;
    }
    if (mode) {
        parse_rx_frame(mode);
        return;
    }
    parse_rx_frame("507aa8");
}

static int dev_open(void *context, const char *path, int flags)
{
    (void)context; (void)flags;
    fprintf(stderr, "fake cec open %s\n", path);
    atomic_fetch_add(&open_count, 1);
    return 7;
}
static int dev_close(void *context, int fd)
{
    (void)context; (void)fd;
    atomic_fetch_add(&close_count, 1);
    return 0;
}
static int dev_ioctl(void *context, int fd, unsigned long request, void *argument)
{
    (void)context; (void)fd;
    if (!argument) { errno = EINVAL; return -1; }
    if (request == CEC_OPEN) {
        atomic_fetch_add(&init_count, 1);
        return 0;
    }
    if (request == CEC_CLOSE)
        return 0;
    if (request == CEC_TX) {
        oa_cec_raw_packet *p = argument;
        fprintf(stderr, "fake cec tx len=%u", (unsigned)p->length);
        for (unsigned i = 0; i < p->length && i < 16; i++)
            fprintf(stderr, " %02x", p->bytes[i]);
        fputc('\n', stderr);
        atomic_fetch_add(&tx_count, 1);
        return 0;
    }
    if (request == CEC_RX) {
        oa_cec_raw_packet *p = argument;
        if (atomic_exchange(&rx_delay_once, 1) == 0) {
            const char *d = getenv("OA_CEC_FAKE_RX_DELAY_MS");
            int ms = d ? atoi(d) : 0;
            if (ms > 0) {
                fprintf(stderr, "fake cec rx delay %d\n", ms);
                usleep((useconds_t)ms * 1000);
            }
        }
        init_rx_frames();
        if (rx_at < rx_count) {
            p->length = (uint8_t)rx_lengths[rx_at];
            memcpy(p->bytes, rx_frames[rx_at], (size_t)rx_lengths[rx_at]);
            p->status = 1;
            rx_at++;
        } else {
            p->length = 0;
            p->status = 0;
        }
        return 0;
    }
    errno = EINVAL;
    return -1;
}
static const oa_cec_device_io device = {
    NULL, dev_open, dev_ioctl, dev_close
};

static const oa_cec_module_transport transport = {
    &authority_mem, &device
};

__attribute__((constructor)) static void install_fake(void)
{
    mirror_page[0x200] = 0x02; /* authority bit 1 already set */
    oa_cec_module_transport_set(&transport);
}
