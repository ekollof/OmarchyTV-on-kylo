/*
 * wdtctl — operate the MCU watchdog the way pcbconfig does.
 *
 * Read out of `pcbconfig` (Ghidra; /usr/sbin/pcbconfig, 16 KB, links only libc):
 *
 *   open("/proc/nvtstbcipc", O_RDWR) + fcntl(F_SETLK, F_WRLCK)
 *   write(fd, packet, 6) where packet = { 0, cmd, arg0, arg1, arg2, cmd }
 *   (read back 6 bytes for an ack when the first byte is 1)
 *
 *   FIPC_set_watch_dog(sec)  -> cmd 0xae, arg0 = (sec >> 8) & 0xff, arg1 = sec & 0xff, arg2 = 1
 *   FIPC_disable_watch_dog() -> cmd 0xae, args 0, 0, 0
 *
 * This is the lever that lets openaurora own the panel: aurora is what feeds the watchdog, so
 * stopping it resets the SoC. Disabling the watchdog first removes that dependency.
 *
 *   wdtctl disable
 *   wdtctl set <seconds>
 *   wdtctl kick            (set again with the current timeout)
 */
#include <fcntl.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define IPC_NODE "/proc/nvtstbcipc"
#define CMD_WDT  0xae

static int ipc_open(void)
{
    int fd = open(IPC_NODE, O_RDWR);
    if (fd < 0) {
        perror(IPC_NODE);
        return -1;
    }
    struct flock fl;
    memset(&fl, 0, sizeof fl);
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    /* fcntl is fcntl@GLIBC_2.28 in this toolchain and the set has 2.23: go through syscall */
    if (syscall(SYS_fcntl, fd, F_SETLK, &fl) < 0)
        fprintf(stderr, "wdtctl: lock failed (continuing anyway)\n");
    return fd;
}

static int ipc_xfer(int fd, unsigned char cmd, unsigned char a, unsigned char b, unsigned char c,
                    int ack)
{
    unsigned char pkt[6] = { ack ? 1 : 0, cmd, a, b, c, cmd };
    if (write(fd, pkt, sizeof pkt) != (ssize_t)sizeof pkt)
        return -1;
    if (ack) {
        if (read(fd, pkt, sizeof pkt) != (ssize_t)sizeof pkt)
            return -1;
        if (pkt[1] != cmd)
            return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *what = argc > 1 ? argv[1] : "";
    int fd = ipc_open();
    if (fd < 0)
        return 1;

    int rc;
    if (!strcmp(what, "disable")) {
        rc = ipc_xfer(fd, CMD_WDT, 0, 0, 0, 0);
        printf("watchdog disable -> %s\n", rc == 0 ? "sent" : "failed");
    } else if (!strcmp(what, "set") && argc > 2) {
        int sec = atoi(argv[2]);
        rc = ipc_xfer(fd, CMD_WDT, (sec >> 8) & 0xff, sec & 0xff, 1, 0);
        printf("watchdog set %ds -> %s\n", sec, rc == 0 ? "sent" : "failed");
    } else if (!strcmp(what, "get")) {
        rc = ipc_xfer(fd, CMD_WDT, 0, 0, 3, 1);
        printf("watchdog get -> %s\n", rc == 0 ? "ok" : "failed");
    } else {
        fprintf(stderr, "usage: wdtctl disable | set <sec> | get\n");
        close(fd);
        return 2;
    }
    close(fd);
    return rc == 0 ? 0 : 1;
}
