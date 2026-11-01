/* irkey - read the TV's IR receiver directly from /dev/ttyS2 and decode frames.
 *
 * The remote/front-panel path on this set is a UART to the MCU, not evdev:
 *
 *   IR receiver -> MCU -> /dev/ttyS2 (115200 8N1 raw, 9-byte frames)
 *      -> aurora (DEV_Uart_GetKey / _micom_receive)
 *         -> /tmp/myfifo (20-byte records, DEV_UART_SendDFBKey)
 *            -> libdirectfb_nvt_uart_ir.so -> dfb_input_dispatch -> UI
 *
 * A replacement UI does not need DirectFB: it can read the UART itself. This tool
 * does exactly that. Trace the firmware decoder/caller before using a live
 * capture to confirm an understood frame interpretation.
 *
 *   irkey                 # decode frames, print each key, summary on Ctrl-C
 *   irkey --raw           # just hexdump whatever arrives (9-byte frames marked)
 *   irkey --timeout N     # exit after N seconds (default: run until Ctrl-C)
 *
 * NOTE: /dev/ttyS2 is exclusive-ish - every byte this process reads is a byte the
 * stock UI no longer sees, so run it while the set is idle (or before/after
 * aurora). It consumes input and configures the UART; it is not a harmless
 * view-only query alongside an in-use stock UI.
 *
 * Build (static):
 *   arm-linux-musleabihf-gcc -static -Os -s -o irkey irkey.c
 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

#define DEV "/dev/ttyS2"

/* The remote is NEC extended: address 0x66 0x11, then command + ~command.
 * Table recovered from libdirectfb_nvt_uart_ir.so (nec_keycode_map @ .rodata 0x1800,
 * 52 entries, every one satisfies cmd ^ ~cmd == 0xFF). The second column is the key
 * symbol the vendor driver reports (custom_keycode_map @ 0x18d0). */
static const struct { uint8_t cmd; uint32_t sym; const char *name; } KEYS[] = {
    { 0xc0, 0x00000030, "0" },        { 0x20, 0x00000031, "1" },
    { 0xa0, 0x00000032, "2" },        { 0xfa, 0x00000033, "3" },
    { 0xe0, 0x00000034, "4" },        { 0x10, 0x00000035, "5" },
    { 0x90, 0x00000036, "6" },        { 0x50, 0x00000037, "7" },
    { 0xd0, 0x00000038, "8" },        { 0x30, 0x00000039, "9" },
    { 0x38, 0x0000f000, "f000" },     { 0x12, 0x0000f001, "f001" },
    { 0x22, 0x0000f002, "f002" },     { 0xb8, 0x0000f003, "f003" },
    { 0xa8, 0x0000f047, "f047" },     { 0x28, 0x0000f046, "f046" },
    { 0xe8, 0x0000f04d, "f04d" },     { 0x68, 0x0000f04c, "f04c" },
    { 0x8a, 0x0000000d, "OK/enter" }, { 0x3a, 0x0000f012, "f012" },
    { 0xa2, 0x0000f00f, "f00f" },     { 0x82, 0x0000f04e, "f04e" },
    { 0xc8, 0x0000f068, "f068" },     { 0x02, 0x0000f062, "f062" },
    { 0xf2, 0x0000f052, "f052" },     { 0x88, 0x0000f011, "f011" },
    { 0xca, 0x0000f043, "f043" },     { 0x1a, 0x0000f044, "f044" },
    { 0x9a, 0x0000f042, "f042" },     { 0x5a, 0x0000f045, "f045" },
    { 0x6a, 0x0000f00a, "f00a" },     { 0x0a, 0x0000f051, "f051" },
    { 0x72, 0x0000f056, "f056" },     { 0x58, 0x0000f059, "f059" },
    { 0xd8, 0x0000f05a, "f05a" },     { 0x80, 0x0000f03d, "f03d" },
    { 0xc2, 0x0000f020, "f020" },     { 0xb0, 0x0000f032, "f032" },
    { 0xea, 0x0000f02a, "f02a" },     { 0x00, 0x0000f014, "f014" },
    { 0x48, 0x0000f02a, "f02a*" },    { 0x7a, 0x0000f01b, "f01b" },
    { 0x08, 0x0000f102, "f102" },     { 0x4a, 0x0000f101, "f101" },
    { 0xf8, 0x0000f03c, "f03c" },     { 0x98, 0x0000f01a, "f01a" },
    { 0x42, 0x0000f01e, "f01e" },     { 0xe2, 0x0000f029, "f029" },
    { 0x52, 0x0000f105, "f105" },     { 0x62, 0x0000f05b, "f05b" },
    { 0x92, 0x0000f05c, "f05c" },
};

static volatile sig_atomic_t stop;
static unsigned long seen[256];
static void on_sig(int s) { (void)s; stop = 1; }

static const char *keyname(uint8_t cmd, uint32_t *sym)
{
    for (unsigned i = 0; i < sizeof KEYS / sizeof KEYS[0]; i++)
        if (KEYS[i].cmd == cmd) { *sym = KEYS[i].sym; return KEYS[i].name; }
    *sym = 0;
    return "?";
}

static int setup(int fd)
{
    struct termios t;
    memset(&t, 0, sizeof t);
    t.c_cflag = CS8 | CREAD | CLOCAL | B115200;
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    tcflush(fd, TCIFLUSH);
    return tcsetattr(fd, TCSANOW, &t);
}

int main(int argc, char **argv)
{
    int raw = 0, timeout = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--raw")) raw = 1;
        else if (!strcmp(argv[i], "--timeout") && i + 1 < argc) timeout = atoi(argv[++i]);
        else { fprintf(stderr, "usage: %s [--raw] [--timeout SEC]\n", argv[0]); return 2; }
    }

    int fd = open(DEV, O_RDWR | O_NOCTTY);
    if (fd < 0) { perror("open " DEV); return 1; }
    if (setup(fd) != 0) { perror("tcsetattr"); return 1; }
    printf("irkey: %s @115200 8N1 raw%s\n", DEV,
           timeout ? "" : " - reading (Ctrl-C for summary)");
    fflush(stdout);

    signal(SIGINT, on_sig);
    signal(SIGTERM, on_sig);

    unsigned char f[9];
    int n = 0;
    unsigned char b;
    struct timeval t0, now;
    gettimeofday(&t0, NULL);

    while (!stop) {
        ssize_t r = read(fd, &b, 1);
        if (r != 1) {
            if (r < 0 && errno != EINTR) { perror("read"); break; }
            continue;
        }
        if (timeout) {
            gettimeofday(&now, NULL);
            if (now.tv_sec - t0.tv_sec >= timeout) break;
        }

        if (raw) {
            printf("%02x ", b);
            if (++n % 16 == 0) printf("\n");
            fflush(stdout);
            continue;
        }

        /* Frame: FF FF <type> <b3> <b4> <b5> <b6> <b7> <b8> */
        if (n == 0 && b != 0xff) continue;            /* sync on FF */
        if (n == 1 && b != 0xff) { n = 0; continue; } /* need FF FF */
        f[n++] = b;
        if (n < 9) continue;
        n = 0;

        if (f[0] != 0xff || f[1] != 0xff) continue;

        uint8_t type = f[2];
        if (type == 0x50) {
            /* NEC extended: addr 0x66 0x11, cmd, ~cmd */
            uint8_t a1 = f[3], a2 = f[4], cmd = f[5], ncmd = f[6];
            int necolike = ((cmd ^ ncmd) == 0xff);
            uint32_t sym;
            const char *nm = keyname(cmd, &sym);
            seen[cmd]++;
            printf("key  0x50  %02x %02x %02x %02x | addr=%02x%02x cmd=0x%02x%s  sym=0x%08x (%s)"
                   "  tail=%02x %02x\n",
                   a1, a2, cmd, ncmd, a1, a2, cmd, necolike ? "" : " (no ~cmd!)",
                   sym, nm, f[7], f[8]);
        } else if (type == 0x5e) {
            printf("evt  0x5e  %02x %02x %02x %02x  b3=0x%02x  tail=%02x %02x\n",
                   f[3], f[4], f[5], f[6], f[3], f[7], f[8]);
        } else {
            printf("??   type=0x%02x  %02x %02x %02x %02x %02x %02x\n",
                   type, f[3], f[4], f[5], f[6], f[7], f[8]);
        }
        fflush(stdout);
    }

    close(fd);
    int any = 0;
    for (int i = 0; i < 256; i++) if (seen[i]) any = 1;
    if (any) {
        printf("\nsummary (cmd -> count):\n");
        for (int i = 0; i < 256; i++)
            if (seen[i]) {
                uint32_t sym; const char *nm = keyname((uint8_t)i, &sym);
                printf("  0x%02x  x%-4lu  sym=0x%08x  %s\n", i, seen[i], sym, nm);
            }
    }
    return 0;
}
