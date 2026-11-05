/* dfbkey — inject a key record through the vendor DFB input FIFO.
 *
 *   dfbkey SYMBOL [TYPE]     write one 20-byte record to /tmp/myfifo
 *                            TYPE 1=press (default), 2=release
 *   dfbkey --kp CODE [TYPE]  write a 12-byte /tmp/kp_fifo record
 *                            TYPE 4=press, 1=release, 8=repeat
 *
 * /tmp/myfifo is the vendor's normal input path (DEV_UART_SendDFBKey writes
 * 20-byte records, Dev_Uart_GetKey4 reads them). This is an authorized input
 * test tool, not a view-only probe.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int parse(const char *text, unsigned long *out)
{
    char *end;
    errno = 0;
    unsigned long value = strtoul(text, &end, 0);
    if (errno || *end) return -1;
    *out = value;
    return 0;
}

int main(int argc, char **argv)
{
    int kp = 0;
    const char *arg = NULL, *type_arg = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--kp")) kp = 1;
        else if (!arg) arg = argv[i];
        else if (!type_arg) type_arg = argv[i];
        else { fprintf(stderr, "too many arguments\n"); return 2; }
    }
    if (!arg) {
        fprintf(stderr, "usage: %s [--kp] CODE [TYPE]\n", argv[0]);
        return 2;
    }
    unsigned long code = 0, type = kp ? 4 : 1;
    if (parse(arg, &code) || (type_arg && parse(type_arg, &type))) {
        fprintf(stderr, "bad number\n");
        return 2;
    }
    if (kp) {
        if (code > UINT32_MAX || type > UINT32_MAX) return 2;
        uint8_t record[12];
        memset(record, 0, sizeof record);
        record[0] = 1;
        record[4] = (uint8_t)code;
        record[5] = (uint8_t)(code >> 8);
        record[6] = (uint8_t)(code >> 16);
        record[7] = (uint8_t)(code >> 24);
        record[8] = (uint8_t)type;
        int fd = open("/tmp/kp_fifo", O_RDWR | O_CLOEXEC);
        if (fd < 0) { perror("open /tmp/kp_fifo"); return 1; }
        ssize_t n = write(fd, record, sizeof record);
        close(fd);
        if (n != (ssize_t)sizeof record) { perror("write"); return 1; }
        printf("kp type=%lu code=0x%lx\n", type, code);
        return 0;
    }
    if (code > UINT32_MAX || type > UINT32_MAX) return 2;
    uint8_t record[20];
    memset(record, 0, sizeof record);
    record[0] = 1;                  /* status */
    record[4] = (uint8_t)type;      /* 1 press, 2 release */
    record[8] = (uint8_t)code;
    record[9] = (uint8_t)(code >> 8);
    record[10] = (uint8_t)(code >> 16);
    record[11] = (uint8_t)(code >> 24);
    record[16] = 1;                 /* repeat/long flag */
    int fd = open("/tmp/myfifo", O_WRONLY | O_CLOEXEC);
    if (fd < 0) { perror("open /tmp/myfifo"); return 1; }
    ssize_t n = write(fd, record, sizeof record);
    close(fd);
    if (n != (ssize_t)sizeof record) { perror("write"); return 1; }
    printf("dfb type=%lu code=0x%lx\n", type, code);
    return 0;
}
