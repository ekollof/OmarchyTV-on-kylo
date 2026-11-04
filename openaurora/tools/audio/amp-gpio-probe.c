/* Only firmware-understood GET operations on normal reset GPIO114.
 * Do not replace these with HAL getters: they can claim/export a pin. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "openaurora/amp_gpio_query.h"
int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--describe")) {
        puts("reset GPIO114: /dev/ntgpio GET670d/670f only; direction0=output/1=input; no HAL/export/claim/STBC");return 0;
    }
    if(argc!=2 || strcmp(argv[1],"--read-reset")) {
        fputs("usage: amp-gpio-probe --describe | --read-reset\n",stderr);return 2;
    }
#if defined(__arm__) && UINTPTR_MAX == UINT32_MAX
    int fd=open("/dev/ntgpio",O_RDONLY|O_CLOEXEC);
    if(fd<0){perror("open ntgpio");return 1;}
    oa_amp_reset_state state;int rc=oa_amp_reset_query(fd,&state,NULL,NULL);
    int closed=close(fd),close_error=errno;
    if(rc){errno=-rc;perror("GET reset GPIO114");return 1;}
    if(closed){errno=close_error;perror("close ntgpio");return 1;}
    printf("GPIO114 level=%u cached-direction=%u (%s)\n",state.level,state.direction,
        state.direction?"input":"output");return 0;
#else
    fputs("native query requires ARM32\n",stderr);return 1;
#endif
}
