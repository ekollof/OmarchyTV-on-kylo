/* Read-only DVB frontend inventory. Does not tune or write. */
#include <errno.h>
#include <fcntl.h>
#include <linux/dvb/frontend.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int adapter = argc > 1 ? atoi(argv[1]) : 0;
    int frontend = argc > 2 ? atoi(argv[2]) : 0;
    char path[64];
    snprintf(path, sizeof path, "/dev/dvb/adapter%d/frontend%d", adapter,
        frontend);
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "open %s: %s\n", path, strerror(errno));
        return 1;
    }
    struct dvb_frontend_info info;
    memset(&info, 0, sizeof info);
    if (ioctl(fd, FE_GET_INFO, &info) == 0)
        printf("name=%s type=%d freq_min=%u freq_max=%u symbol_min=%u symbol_max=%u\n",
            info.name, info.type, info.frequency_min, info.frequency_max,
            info.symbol_rate_min, info.symbol_rate_max);
    else
        printf("FE_GET_INFO: %s\n", strerror(errno));
    fe_status_t status = 0;
    if (ioctl(fd, FE_READ_STATUS, &status) == 0)
        printf("status=0x%x lock=%d signal=%d carr=%d viterbi=%d sync=%d\n",
            (unsigned)status, !!(status & FE_HAS_LOCK),
            !!(status & FE_HAS_SIGNAL), !!(status & FE_HAS_CARRIER),
            !!(status & FE_HAS_VITERBI), !!(status & FE_HAS_SYNC));
    else
        printf("FE_READ_STATUS: %s\n", strerror(errno));
    struct dvb_frontend_parameters params;
    memset(&params, 0, sizeof params);
    if (ioctl(fd, FE_GET_FRONTEND, &params) == 0)
        printf("frequency=%u bandwidth=%d inversion=%d\n", params.frequency,
            params.u.ofdm.bandwidth, params.inversion);
    else
        printf("FE_GET_FRONTEND: %s\n", strerror(errno));
    close(fd);
    return 0;
}
