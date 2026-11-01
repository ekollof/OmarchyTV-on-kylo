/* Controlled TV CEC diagnostics. Ownership mode ONLY opens/closes the raw node;
 * it never initializes the controller, dequeues traffic or transmits a frame.
 * KER_CEC_Open/Release semantics are recovered in CEC-OWNERSHIP-2026-11-01. */
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include "openaurora/vendor_cec.h"

#if defined(__arm__) && UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(struct v4l2_ext_control) == 20, "target control layout");
_Static_assert(sizeof(struct v4l2_ext_controls) == 24, "target control list layout");
_Static_assert(offsetof(struct v4l2_ext_control, ptr) == 12, "target control payload");
_Static_assert(offsetof(struct v4l2_ext_controls, controls) == 20, "target control pointer");
_Static_assert(VIDIOC_S_EXT_CTRLS == UINT32_C(0xc0185648), "target ioctl request");
#endif

static int controls(int fd, struct v4l2_ext_control *items, unsigned count)
{
    struct v4l2_ext_controls request;
    memset(&request, 0, sizeof request);
    request.ctrl_class = UINT32_C(0x0f1a0000);
    request.count = count; request.controls = items;
    int rc = ioctl(fd, VIDIOC_S_EXT_CTRLS, &request);
    if (rc < 0) perror("CEC control ioctl");
    return rc;
}
static void control(struct v4l2_ext_control *c, uint32_t id, void *data, uint32_t size)
{
    memset(c, 0, sizeof *c); c->id = id; c->size = size; c->ptr = data;
}
/* These shared-owner modes never initialize/close CEC or consume receive data.
 * HDMIswitch open/release are no-ops; source and live common fd identify video28.
 * Keep the vendor as the physical owner and inspect its normal cached responses. */
static int shared(const char *path, int destination, int opcode)
{
    if (sizeof(void *) != 4) { fputs("shared controls require target ARM32 ABI\n", stderr); return 1; }
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) { perror(path); return 1; }
    uint8_t logical = 255; uint32_t hardware_id = 0;
    struct v4l2_ext_control items[3];
    control(&items[0], UINT32_C(0x0f1a0f06), &logical, 1);
    control(&items[1], UINT32_C(0x0f1a0f06), &hardware_id, 4);
    int rc = controls(fd, items, 2);
    printf("shared CEC logical address=%u, ioctl_rc=%d\n", logical, rc);
    if (rc < 0 || logical != 0) { close(fd); return 1; }
    if (opcode >= 0) {
        uint8_t operands[16], accepted = 0; uint32_t mode = 0;
        oa_cec_message message = { .initiator = 0, .destination = (uint8_t)destination, .opcode = opcode };
        oa_cec_vpl_packet packet;
        if (oa_cec_vpl_encode(&packet, operands, sizeof operands, &message)) { close(fd); return 1; }
        control(&items[0], UINT32_C(0x0f1a0f04), &accepted, 1);
        control(&items[1], UINT32_C(0x0f1a0f04), &packet, sizeof packet);
        control(&items[2], UINT32_C(0x0f1a0f04), &mode, sizeof mode);
        rc = controls(fd, items, 3);
        printf("query 0>%x opcode=%02x ioctl_rc=%d driver_accepted=%u\n", destination, opcode, rc, accepted);
        if (rc < 0 || accepted != 1) { close(fd); return 1; }
    }
    if (close(fd) < 0) { perror("close video control fd"); return 1; }
    return 0;
}

static int ownership(const char *path)
{
    FILE *f = fopen("/proc/devices", "r");
    if (!f) { perror("/proc/devices"); return 1; }
    char line[160], name[80]; unsigned device_major = 0, candidate;
    while (fgets(line, sizeof line, f))
        if (sscanf(line, "%u %79s", &candidate, name) == 2 && !strcmp(name, "ntkcec")) {
            device_major = candidate; break;
        }
    fclose(f);
    struct stat node;
    if (stat(path, &node) < 0) { perror(path); return 1; }
    if (!device_major || !S_ISCHR(node.st_mode) || major(node.st_rdev) != device_major || minor(node.st_rdev)) {
        fprintf(stderr, "refusing node: expected registered ntkcec major %u minor 0\n", device_major);
        return 1;
    }
    errno = 0;
    int fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        int saved = errno;
        printf("exclusive open: errno=%d (%s)\n", saved, strerror(saved));
        return saved == EBUSY || saved == EPERM ? 0 : 1;
    }
    printf("exclusive open: fd=%d; no initialization or bus I/O requested\n", fd);
    if (close(fd) < 0) { perror("close"); return 1; }
    puts("descriptor released"); return 0;
}
static uint32_t word(const uint8_t *p)
{
    uint32_t value; memcpy(&value, p, sizeof value); return value;
}
static int cached_controller(int memory, unsigned long long address)
{
    uint8_t object[0x13b0];
    if (pread64(memory, object, sizeof object, (off64_t)address) != (ssize_t)sizeof object) return 0;
    /* Aurora 64c25234... constructor @1030d94 uses vtable 152fa68 and a
     * secondary interface +94 at this+4. Getters confirm stride/field offsets.
     * UString::c_str @c870b8 returns this+4; its first word is byte length. */
    if (word(object) != UINT32_C(0x0152fa68) || word(object + 4) != UINT32_C(0x0152fafc)) return 0;
    printf("controller=0x%llx\n", address);
    for (unsigned la = 1; la < 15; la++) {
        size_t offset = la * 0x128;
        uint32_t length = word(object + offset + 0x13c), pointer = word(object + offset + 0x140);
        char name[15] = {0};
        int have_name = length <= 14 && (!length || (pointer &&
            pread64(memory, name, length, pointer) == (ssize_t)length));
        printf("LA=%u seen=%u PA=%04x power=%u name=", la, object[offset + 0x12c],
               word(object + offset + 0x130), word(object + offset + 0x148));
        if (!have_name) printf("<unavailable len=%u>", length);
        else for (uint32_t i = 0; i < length; i++) {
            unsigned ch = (unsigned char)name[i];
            if (ch >= 32 && ch < 127) putchar((int)ch);
            else printf("\\x%02x", ch);
        }
        putchar('\n');
    }
    return 1;
}
static int cached(const char *pid_text)
{
    char *end; long pid = strtol(pid_text, &end, 10);
    if (!*pid_text || *end || pid < 1 || pid > INT32_MAX) return 2;
    char path[80]; snprintf(path, sizeof path, "/proc/%ld/maps", pid);
    FILE *maps = fopen(path, "r");
    if (!maps) { perror(path); return 1; }
    snprintf(path, sizeof path, "/proc/%ld/mem", pid);
    int memory = open(path, O_RDONLY | O_CLOEXEC);
    if (memory < 0) { perror(path); fclose(maps); return 1; }
    uint8_t *buffer = malloc(65536);
    if (!buffer) { close(memory); fclose(maps); return 1; }
    char line[512], mode[5]; unsigned long long first, last; unsigned found = 0;
    while (fgets(line, sizeof line, maps)) {
        if (!strstr(line, "[heap]") || sscanf(line, "%llx-%llx %4s", &first, &last, mode) != 3 ||
            mode[0] != 'r' || mode[1] != 'w' || last <= first || last > UINT32_MAX) continue;
        for (unsigned long long address = first; address < last; address += 65532) {
            size_t count = last - address < 65536 ? (size_t)(last - address) : 65536;
            ssize_t n = pread64(memory, buffer, count, (off64_t)address);
            if (n <= 0) continue;
            for (size_t i = 0; i + 8 <= (size_t)n; i += 4)
                if (word(buffer + i) == UINT32_C(0x0152fa68) && word(buffer + i + 4) == UINT32_C(0x0152fafc))
                    found += (unsigned)cached_controller(memory, address + i);
        }
    }
    printf("matching live controller objects=%u\n", found);
    free(buffer); close(memory); fclose(maps);
    return found == 1 ? 0 : 1;
}
int main(int argc, char **argv)
{
    if (argc == 3 && !strcmp(argv[1], "cached")) return cached(argv[2]);
    if (argc == 3 && !strcmp(argv[1], "ownership")) return ownership(argv[2]);
    if (argc == 3 && !strcmp(argv[1], "shared-status")) return shared(argv[2], 0, -1);
    if (argc == 5 && !strcmp(argv[1], "shared-query")) {
        char *end; long destination = strtol(argv[3], &end, 0);
        if (!*argv[3] || *end || destination < 1 || destination > 14) return 2;
        long opcode = strtol(argv[4], &end, 0);
        if (!*argv[4] || *end || (opcode != 0x46 && opcode != 0x83 && opcode != 0x8f)) return 2;
        return shared(argv[2], (int)destination, (int)opcode);
    }
    fprintf(stderr, "usage: %s ownership RAW_NODE | cached AURORA_PID | shared-status VIDEO_NODE | shared-query VIDEO_NODE LA 0x46|0x83|0x8f\n", argv[0]);
    return 2;
}
