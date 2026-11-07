/*
 * mod_ir — the remote control, straight off the MCU UART.
 *
 * This is the module that replaces DirectFB's input driver for our purposes.
 * See artifacts/reports/IR-UART-2026-10-30.md for how the protocol was found:
 *
 *   /dev/ttyS2, 115200 8N1 raw
 *   frame: FF FF TT A1 A2 CC ~CC B7 B8
 *          TT = 0x50 key event, 0x5E release/other
 *          A1 A2 = NEC extended address (0x66 0x11), CC = command, ~CC its complement
 *
 * The key id we publish is the NEC command byte: stable, one per button.
 *
 * The port is exclusive: whoever has it open consumes the bytes. When our UI is
 * in front that is us; when the stock UI runs, this module must not be started.
 */
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "openaurora/events.h"
#include "openaurora/ir_blast.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

#define IR_DEV "/dev/ttyS2"

struct ctx {
    const oa_host_api *api;
    int fd;
    int published;
    uint32_t watch;
    unsigned char frame[9];
    int n;
    uint8_t last_cmd;
    char dev[64];

    /* BLAST_RX blocks in the vendor driver while the remote is idle and is
     * not interrupted by closing the v4l2 fd. It runs in oa-ir-blast; this
     * module reads fixed records from the helper's stdout pipe and can kill
     * and reap the child on stop. */
    char blast_helper[256];
    pid_t blast_pid;
    int blast_pipe;
    uint32_t blast_watch;
    unsigned char blast_rxbuf[64];
    size_t blast_rxn;

    /* ABI 3 quiesce: the UART close can stall the vendor driver for tens of
     * seconds. The loop hands that fd to this worker and retries the join. */
    pthread_t cleanup_thread;
    int cleanup_running;
    int cleanup_fd;
    unsigned cleanup_delay_ms; /* diagnostic: exercise the async cleanup gate */
};

static struct ctx *g_ctx;

/* RC codes for this chassis' remote (Vestel RC48), recovered from the device profile via
 * ICE_RemoteInit -> _profileloadkeydefinitions() and joined with /vendor/ice_keymapping_db.xml.
 * Source of truth: artifacts/protocol/ir-keymap-2026-10-30.csv, report IR-KEYMAP-2026-10-30.md.
 * If the MCU puts the RC code in the frame, these names are what the UI should show. */
static const struct {
    uint8_t code;
    const char *name;
} RC_KEYS[] = {
    {  0, "Zero" },        {  1, "One" },          {  2, "Two" },
    {  3, "Three" },       {  4, "Four" },         {  5, "Five" },
    {  6, "Six" },         {  7, "Seven" },        {  8, "Eight" },
    {  9, "Nine" },        { 10, "Exit" },         { 12, "Power" },
    { 13, "Mute" },        { 15, "Lang" },         { 16, "VolumeUp" },
    { 17, "VolumeDown" },  { 18, "Info" },         { 19, "Down" },
    { 20, "Up" },          { 21, "Left" },         { 22, "Right" },
    { 24, "Stop" },        { 25, "Play" },         { 26, "MyButton2" },
    { 27, "Rewind" },      { 28, "Fforward" },     { 32, "ProgramUp" },
    { 33, "ProgramDown" }, { 37, "Exit2" },        { 39, "Netflix" },
    { 47, "Epg" },         { 48, "Menu" },         { 49, "Pause" },
    { 50, "Yellow" },      { 51, "Record" },       { 52, "Blue" },
    { 53, "Select" },      { 54, "Green" },        { 55, "Red" },
    { 56, "Aux" },         { 59, "MyButton" },     { 60, "TextPatMix" },
};

static const char *rc_name(uint8_t code)
{
    for (unsigned i = 0; i < sizeof RC_KEYS / sizeof RC_KEYS[0]; i++)
        if (RC_KEYS[i].code == code)
            return RC_KEYS[i].name;
    return NULL;
}

/* NEC command -> vendor key name, recovered from libdirectfb_nvt_uart_ir.so
 * (nec_keycode_map / custom_keycode_map, 52 entries). */
static const struct {
    uint8_t cmd;
    const char *name;
} KEYS[] = {
    { 0xc0, "0" }, { 0x20, "1" }, { 0xa0, "2" }, { 0xfa, "3" }, { 0xe0, "4" },
    { 0x10, "5" }, { 0x90, "6" }, { 0x50, "7" }, { 0xd0, "8" }, { 0x30, "9" },
    { 0x38, "k_f000" }, { 0x12, "k_f001" }, { 0x22, "k_f002" }, { 0xb8, "k_f003" },
    { 0xa8, "k_f047" }, { 0x28, "k_f046" }, { 0xe8, "k_f04d" }, { 0x68, "k_f04c" },
    { 0x8a, "OK" },     { 0x3a, "k_f012" }, { 0xa2, "k_f00f" }, { 0x82, "k_f04e" },
    { 0xc8, "k_f068" }, { 0x02, "k_f062" }, { 0xf2, "k_f052" }, { 0x88, "k_f011" },
    { 0xca, "k_f043" }, { 0x1a, "k_f044" }, { 0x9a, "k_f042" }, { 0x5a, "k_f045" },
    { 0x6a, "k_f00a" }, { 0x0a, "k_f051" }, { 0x72, "k_f056" }, { 0x58, "k_f059" },
    { 0xd8, "k_f05a" }, { 0x80, "k_f03d" }, { 0xc2, "k_f020" }, { 0xb0, "k_f032" },
    { 0xea, "k_f02a" }, { 0x00, "k_f014" }, { 0x48, "k_f02a_dup" }, { 0x7a, "k_f01b" },
    { 0x08, "k_f102" }, { 0x4a, "k_f101" }, { 0xf8, "k_f03c" }, { 0x98, "k_f01a" },
    { 0x42, "k_f01e" }, { 0xe2, "k_f029" }, { 0x52, "k_f105" }, { 0x62, "k_f05b" },
    { 0x92, "k_f05c" },
};

static const char *key_name(uint8_t cmd)
{
    for (unsigned i = 0; i < sizeof KEYS / sizeof KEYS[0]; i++)
        if (KEYS[i].cmd == cmd)
            return KEYS[i].name;
    return "?";
}

static int open_port(struct ctx *c)
{
    int fd = open(c->dev, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        return -1;

    struct termios t;
    memset(&t, 0, sizeof t);
    t.c_cflag = CS8 | CREAD | CLOCAL | B115200;
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    tcflush(fd, TCIFLUSH);
    if (tcsetattr(fd, TCSANOW, &t) != 0) {
        close(fd);
        return -1;
    }
    c->fd = fd;
    c->n = 0;
    return 0;
}

static void emit_key(struct ctx *c, uint8_t cmd, uint8_t kind)
{
    oa_key_event ev;
    memset(&ev, 0, sizeof ev);
    ev.id = cmd;
    ev.kind = kind;
    ev.ts_ms = 0; /* consumers can use the clock service */
    c->api->emit(c->api->priv, OA_TOPIC_KEY, &ev);
}

static void on_frame(struct ctx *c)
{
    const unsigned char *f = c->frame;
    if (f[0] != 0xff || f[1] != 0xff)
        return;

    if (f[2] == 0x50) {
        uint8_t cmd = f[5];
        uint8_t ncmd = f[6];
        if ((uint8_t)(cmd ^ ncmd) != 0xff) {
            c->api->log(c->api->priv, OA_LOG_WARN, "bad ~cmd (0x%02x/0x%02x), ignoring", cmd, ncmd);
            return;
        }
        c->last_cmd = cmd;
        /* Two encodings reach us depending on the MCU: the profile's RC code (0..63) and the NEC
         * command. Label whichever we got so the name is right either way. */
        const char *name = rc_name(f[3]);
        if (name)
            c->api->log(c->api->priv, OA_LOG_DEBUG, "key rc=%u %s (nec 0x%02x)", f[3], name, cmd);
        else
            c->api->log(c->api->priv, OA_LOG_DEBUG, "key 0x%02x %s (addr %02x%02x)", cmd,
                        key_name(cmd), f[3], f[4]);
        emit_key(c, name ? f[3] : cmd, OA_KEY_PRESS);
    } else if (f[2] == 0x5e) {
        c->api->log(c->api->priv, OA_LOG_DEBUG, "release 0x%02x", c->last_cmd);
        emit_key(c, c->last_cmd, OA_KEY_RELEASE);
    } else {
        c->api->log(c->api->priv, OA_LOG_DEBUG, "frame type 0x%02x", f[2]);
    }
}

static void consume(struct ctx *c, const unsigned char *buf, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        unsigned char b = buf[i];
        /* resync: a frame always starts with FF FF */
        if (c->n == 0 && b != 0xff)
            continue;
        if (c->n == 1 && b != 0xff) {
            c->n = 0;
            continue;
        }
        c->frame[c->n++] = b;
        if (c->n == 9) {
            on_frame(c);
            c->n = 0;
        }
    }
}

static void on_readable(void *ud, int fd, unsigned rev)
{
    struct ctx *c = ud;
    unsigned char buf[64];
    ssize_t n = read(fd, buf, sizeof buf);
    if (n <= 0)
        return;
    consume(c, buf, (size_t)n);
    (void)rev;
}

/* The stock parser has two sources: the MCU UART frames above and the vendor
 * NDAL BLAST V4L2 receiver. DEV_Ir_GetKey polls BLAST_RX and synthesizes the
 * same 9-byte frame: FF FF 50/5E A1 A2 CC ~CC FF FF. Use the same path when the
 * UART is silent (the MCU only forwards frames after _HW_MCU_RcInit).
 *
 * BLAST_RX blocks in the vendor driver while the remote is idle and is not
 * interrupted by closing the v4l2 fd. It therefore runs in the separate
 * oa-ir-blast process; this module reads fixed records from the helper's
 * stdout pipe on the loop, emits keys there, and kills/reaps the helper on
 * stop. No vendor BLAST call runs in the host process. */
/* The helper must not inherit the host's devices/sockets across exec. pipe2()
 * marks the pipe and open_port() uses O_CLOEXEC, but fds opened by other
 * modules or vendor libraries are outside this module's control. Run this in
 * the forked child just before exec: mark every descriptor above stderr
 * close-on-exec. Only async-signal-safe calls are used; ioctl(FIOCLEX) avoids
 * the toolchain's fcntl@GLIBC_2.28, which the TV's glibc 2.23 cannot load. */
static void mark_inherited_fds_cloexec(void)
{
    long max = sysconf(_SC_OPEN_MAX);
    if (max < 0 || max > 65536) max = 65536;
    for (int fd = 3; fd < (int)max; fd++)
        (void)ioctl(fd, FIOCLEX, 0);
}

/* Nonblocking reap with a bounded wait. Returns 1 when the child is gone
 * (reaped now or already reaped) and 0 when it is still running. Never uses a
 * blocking waitpid: a helper stuck in a vendor call must not stall the loop. */
static int reap_child_bounded(pid_t pid, int attempts)
{
    int status = 0;
    for (int i = 0; i < attempts; i++) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid)
            return 1;
        if (r < 0 && errno == ECHILD)
            return 1;
        if (r < 0 && errno != EINTR)
            return 0;
        const struct timespec pause = { .tv_nsec = 5000000 };
        (void)nanosleep(&pause, NULL);
    }
    return 0;
}

static void blast_helper_read(void *ud, int fd, unsigned rev)
{
    struct ctx *c = ud;
    (void)rev;
    /* Read only into free space. An unbounded 64-byte read with a partial
     * record pending used to reset the assembly buffer and drop/desync
     * records; records are 8 bytes and the buffer is a multiple of 8, so a
     * bounded read always drains exactly. */
    size_t room = 0;
    if (c->blast_rxn > sizeof c->blast_rxbuf)
        c->blast_rxn = 0; /* cannot happen; avoid a size_t underflow */
    room = sizeof c->blast_rxbuf - c->blast_rxn;
    if (room == 0) {
        c->blast_rxn = 0; /* impossible with 8-byte records; stay safe */
        room = sizeof c->blast_rxbuf;
    }
    ssize_t n = read(fd, c->blast_rxbuf + c->blast_rxn, room);
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
        return;
    if (n > 0) {
        c->blast_rxn += (size_t)n;
        while (c->blast_rxn >= sizeof(struct oa_ir_blast_record)) {
            struct oa_ir_blast_record r;
            memcpy(&r, c->blast_rxbuf, sizeof r);
            c->blast_rxn -= sizeof r;
            memmove(c->blast_rxbuf, c->blast_rxbuf + sizeof r,
                    c->blast_rxn);
            uint8_t rc = (uint8_t)(r.code & 0x3f);
            const char *name = rc_name(rc);
            c->api->log(c->api->priv, OA_LOG_DEBUG,
                        "ir: BLAST code=%08x aux=%02x flag=%02x -> rc=%u %s",
                        r.code, r.aux, r.flag, rc, name ? name : "?");
            emit_key(c, rc, r.flag ? OA_KEY_RELEASE : OA_KEY_PRESS);
        }
        return;
    }

    /* EOF/error: the helper is gone or closed its pipe. Reap it here if it
     * already exited; a still-running child is killed by blast_close. */
    if (c->blast_watch) {
        c->api->watch_del(c->api->priv, c->blast_watch);
        c->blast_watch = 0;
    }
    if (c->blast_pipe >= 0) {
        close(c->blast_pipe);
        c->blast_pipe = -1;
    }
    if (c->blast_pid > 0) {
        int status = 0;
        pid_t r = waitpid(c->blast_pid, &status, WNOHANG);
        if (r == c->blast_pid) {
            c->api->log(c->api->priv, OA_LOG_WARN,
                        "ir: BLAST helper pid %d exited (%d)",
                        (int)c->blast_pid, status);
            c->blast_pid = -1;
        } else if (r < 0 && errno == ECHILD) {
            c->blast_pid = -1; /* already reaped by a deferred stop */
        }
    }
}

static void blast_helper_stop(struct ctx *c)
{
    if (c->blast_watch) {
        c->api->watch_del(c->api->priv, c->blast_watch);
        c->blast_watch = 0;
    }
    if (c->blast_pipe >= 0) {
        close(c->blast_pipe);
        c->blast_pipe = -1;
    }
    if (c->blast_pid > 0) {
        pid_t pid = c->blast_pid;
        c->blast_pid = -1;
        (void)kill(pid, SIGTERM);
        if (!reap_child_bounded(pid, 20)) {
            (void)kill(pid, SIGKILL);
            if (!reap_child_bounded(pid, 20)) {
                /* A helper stuck in an uninterruptible vendor call cannot be
                 * waited for without stalling the loop. Remember it so a
                 * later stop/destroy retries the nonblocking reap. */
                c->api->log(c->api->priv, OA_LOG_WARN,
                            "ir: BLAST helper pid %d survived SIGKILL; "
                            "deferring reap", (int)pid);
                c->blast_pid = pid;
            }
        }
    }
    c->blast_rxn = 0;
}

static void blast_close(struct ctx *c)
{
    blast_helper_stop(c);
}

static int resolve_helper(struct ctx *c, const char *configured)
{
    if (!configured || !*configured)
        configured = "oa-ir-blast";
    if (strchr(configured, '/')) {
        if (strlen(configured) >= sizeof c->blast_helper)
            return -1;
        snprintf(c->blast_helper, sizeof c->blast_helper, "%s", configured);
        return 0;
    }

    /* A bare helper name is installed next to the core executable. This works
     * for both the stage bundle root and the installed BINDIR. */
    char exe[512];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n <= 0)
        return -1;
    exe[n] = '\0';
    char *slash = strrchr(exe, '/');
    if (!slash)
        return -1;
    slash[1] = '\0';
    int len = snprintf(c->blast_helper, sizeof c->blast_helper,
                       "%s%s", exe, configured);
    return len > 0 && (size_t)len < sizeof c->blast_helper ? 0 : -1;
}

static int blast_open(struct ctx *c)
{
    const char *helper = c->api->config_get(c->api->priv,
                                            "ir.blast_helper",
                                            "oa-ir-blast");
    if (resolve_helper(c, helper) != 0) {
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "ir: cannot resolve ir.blast_helper '%s'",
                    helper ? helper : "");
        return -1;
    }

    const char *video = c->api->config_get(c->api->priv,
                                            "ir.blast_video", "/dev/video28");
    int proto = atoi(c->api->config_get(c->api->priv,
                                        "ir.rc_protocol", "4"));
    if (proto == 0)
        proto = 4;
    int set = atoi(c->api->config_get(c->api->priv,
                                      "ir.blast_set", "1")) != 0;

    int p[2];
    /* Format arguments before fork so the child only uses async-signal-safe
     * calls between fork and exec. */
    char proto_arg[16];
    snprintf(proto_arg, sizeof proto_arg, "%d", proto);
    const char *set_arg = set ? "1" : "0";
    if (pipe2(p, O_NONBLOCK | O_CLOEXEC) != 0) {
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "ir: BLAST helper pipe: %s", strerror(errno));
        return -1;
    }
    pid_t pid = fork();
    if (pid < 0) {
        close(p[0]);
        close(p[1]);
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "ir: BLAST helper fork: %s", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        close(p[0]);
        if (dup2(p[1], STDOUT_FILENO) < 0)
            _exit(127);
        close(p[1]);
        mark_inherited_fds_cloexec();
        execl(c->blast_helper, c->blast_helper,
              "--video", video, "--protocol", proto_arg, "--set", set_arg,
              (char *)NULL);
        _exit(127);
    }
    close(p[1]);
    c->blast_pipe = p[0];
    c->blast_pid = pid;
    c->blast_rxn = 0;
    c->blast_watch = c->api->watch_add(c->api->priv, p[0], OA_WATCH_READ,
                                       blast_helper_read, c);
    if (!c->blast_watch) {
        c->api->log(c->api->priv, OA_LOG_ERROR,
                    "ir: BLAST helper watch failed");
        blast_helper_stop(c);
        return -1;
    }
    c->api->log(c->api->priv, OA_LOG_INFO,
                "ir: BLAST helper pid %d (protocol %d, set %d, video %s)",
                (int)pid, proto, set, video[0] ? video : "<none>");
    return 0;
}

static int input_feed(const uint8_t *bytes, size_t n)
{
    if (!g_ctx || g_ctx->cleanup_running || (!bytes && n))
        return -1;
    if (n)
        consume(g_ctx, bytes, n);
    return 0;
}

static int input_device_open(void)
{
    return g_ctx && g_ctx->fd >= 0;
}

static const oa_input_iface g_input = {
    .feed = input_feed,
    .device_open = input_device_open,
};

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof(oa_host_api) || api->abi != OA_ABI_VERSION)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c)
        return -1;
    c->api = api;
    c->fd = -1;
    c->blast_pid = -1;
    c->blast_pipe = -1;
    c->cleanup_fd = -1;
    g_ctx = c;
    *out = c;
    return 0;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    if (c->blast_pid > 0) {
        /* A stop may have deferred an unkillable child's reap. Clear it here
         * once it is gone instead of refusing to start forever. */
        int status = 0;
        pid_t r = waitpid(c->blast_pid, &status, WNOHANG);
        if (r == c->blast_pid || (r < 0 && errno == ECHILD))
            c->blast_pid = -1;
    }
    if (c->cleanup_running || c->cleanup_fd >= 0 || c->blast_pid > 0)
        return -EBUSY;
    g_ctx = c;
    if (c->api->publish(c->api->priv, OA_SVC_INPUT, 1, &g_input) != 0)
        return -1;
    c->published = 1;
    int blast_ok = 0;
    if (atoi(c->api->config_get(c->api->priv, "ir.blast", "1")) == 1)
        blast_ok = blast_open(c) == 0;
    /* BLAST is the verified physical path. Do not also claim the MCU UART
     * unless the caller asks for it explicitly or BLAST is unavailable: the
     * dual-open reload path can stall for tens of seconds in the vendor
     * driver. An explicit ir.device still enables the UART fallback. */
    long delay = atol(c->api->config_get(c->api->priv, "ir.cleanup_delay_ms", "0"));
    if (delay < 0) delay = 0;
    if (delay > 5000) delay = 5000;
    c->cleanup_delay_ms = (unsigned)delay;
    static const char absent[] = "\1";
    const char *dev = c->api->config_get(c->api->priv, "ir.device", absent);
    if (dev[0] == '\1' && dev[1] == '\0')
        dev = blast_ok ? "" : IR_DEV;   /* absent: fall back only without BLAST */
    /* Explicit ir.device= (empty) always keeps the UART closed. */
    snprintf(c->dev, sizeof c->dev, "%s", dev);
    if (!c->dev[0]) {
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "remote parser ready; BLAST only, UART path empty");
        return 0;
    }
    if (open_port(c) != 0) {
        /* Not fatal: on the build host there is no such device. */
        c->api->log(c->api->priv, OA_LOG_WARN, "%s: %s (no remote input)", c->dev, strerror(errno));
        return 0;
    }
    c->watch = c->api->watch_add(c->api->priv, c->fd, OA_WATCH_READ, on_readable, c);
    if (!c->watch) {
        close(c->fd);
        c->fd = -1;
        c->api->unpublish(c->api->priv, OA_SVC_INPUT);
        c->published = 0;
        return -1;
    }
    c->api->log(c->api->priv, OA_LOG_INFO, "%s open @115200 8N1", c->dev);
    return 0;
}

static void ir_close_sync(struct ctx *c)
{
    blast_close(c);
    if (c->watch) {
        c->api->watch_del(c->api->priv, c->watch);
        c->watch = 0;
    }
    if (c->fd >= 0) {
        close(c->fd);
        c->fd = -1;
        c->api->log(c->api->priv, OA_LOG_INFO, "%s closed", c->dev);
    }
}

/* No host API calls and no access to the module after this returns. */
static void *cleanup_worker(void *v)
{
    struct ctx *c = v;
    if (c->cleanup_delay_ms) {
        const struct timespec pause = {
            .tv_sec = c->cleanup_delay_ms / 1000u,
            .tv_nsec = (long)(c->cleanup_delay_ms % 1000u) * 1000000L
        };
        (void)nanosleep(&pause, NULL);
    }
    if (c->cleanup_fd >= 0) {
        int fd = c->cleanup_fd;
        c->cleanup_fd = -1;
        (void)close(fd);
    }
    return NULL;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    if (c->published) {
        c->api->unpublish(c->api->priv, OA_SVC_INPUT);
        c->published = 0;
    }
}

/* Called before m_stop for stop/unload/reload and after a failed init. The
 * close may take tens of seconds with both BLAST and the UART open, so it runs
 * on a worker; the host retries stop/unload until cleanup_pending clears. */
static int m_quiesce(void *v)
{
    struct ctx *c = v;
    if (c->cleanup_running) {
        for (int i = 0; i < 10; i++) {
            int rc = pthread_tryjoin_np(c->cleanup_thread, NULL);
            if (rc == 0) {
                c->cleanup_running = 0;
                return 0;
            }
            if (rc != EBUSY) {
                c->cleanup_running = 0;
                return -EIO;
            }
            const struct timespec pause = { .tv_nsec = 5000000 };
            (void)nanosleep(&pause, NULL);
        }
        return -EAGAIN;
    }

    blast_helper_stop(c);
    if (c->watch) {
        c->api->watch_del(c->api->priv, c->watch);
        c->watch = 0;
    }
    if (c->fd < 0)
        return 0;

    c->cleanup_fd = c->fd;
    c->fd = -1;
    int rc = pthread_create(&c->cleanup_thread, NULL, cleanup_worker, c);
    if (rc) {
        /* Rare fallback: close on the loop rather than leaking the fd. */
        if (c->cleanup_fd >= 0) {
            (void)close(c->cleanup_fd);
            c->cleanup_fd = -1;
        }
        return -EIO;
    }
    c->cleanup_running = 1;
    return -EAGAIN;
}

static void m_destroy(void *v)
{
    struct ctx *c = v;
    if (c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread, NULL);
        c->cleanup_running = 0;
    }
    if (c->cleanup_fd >= 0) {
        (void)close(c->cleanup_fd);
        c->cleanup_fd = -1;
    }
    ir_close_sync(c);
    if (c->published) {
        c->api->unpublish(c->api->priv, OA_SVC_INPUT);
        c->published = 0;
    }
    if (g_ctx == c)
        g_ctx = NULL;
    free(c);
}

/* standby: give the port up, and take it back on resume */
static int m_suspend(void *v)
{
    struct ctx *c = v;
    if (c->cleanup_running) return -EBUSY;
    ir_close_sync(c);
    if (c->published) {
        c->api->unpublish(c->api->priv, OA_SVC_INPUT);
        c->published = 0;
    }
    return 0;
}

static int m_resume(void *v)
{
    return m_start(v);
}

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC,
    .abi = OA_ABI_VERSION,
    .size = sizeof(oa_module),
    .name = "ir",
    .version = "0.1",
    .requires = { OA_SVC_PLAT, "" },
    .init = m_init,
    .start = m_start,
    .stop = m_stop,
    .quiesce = m_quiesce,
    .destroy = m_destroy,
    .suspend = m_suspend,
    .resume = m_resume,
};

OA_MODULE_EXPORT(g_module)
