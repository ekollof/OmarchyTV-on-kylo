/* Host fixture for mod_source: implement the decoded Hal_Vin ABI without a
 * TV. Every open/select/start/close is appended to FAKE_VIN_LOG so the socket
 * test can assert the exact VIN input ids selected for each source type. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static void vin_log(const char *fmt, ...)
{
    const char *path = getenv("FAKE_VIN_LOG");
    if (!path || !path[0]) return;
    FILE *f = fopen(path, "a");
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

int Hal_Vin_Open(int idx)
{
    vin_log("open idx=%d", idx);
    return 100 + idx;
}

void Hal_Vin_Close(int fd)
{
    vin_log("close fd=%d", fd);
}

void Hal_Vin_SelectInput(int fd, int id)
{
    vin_log("select fd=%d id=%d", fd, id);
}

void Hal_Vin_SetStreamStart(int fd)
{
    vin_log("start fd=%d", fd);
}
