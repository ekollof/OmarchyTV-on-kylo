/* One firmware-understood AD82587 register read on the MB230 target.
 * No discovery scan, GPIO/amp reset, gain/mute write or HAL initialization.
 * See AUDIO-BOARD-2026-11-01 for board identity and PreInit transfer semantics.
 * Native kernel I2C_RDWR serializes this transaction with the stock owner.
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "openaurora/amp_i2c.h"

static void describe(void)
{
    puts("AD82587_D: /dev/i2c-3, slave 0x34 (vendor 0x68), registers 0x03..0x05 read only");
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--describe")) { describe(); return 0; }
    if (argc != 2 || strcmp(argv[1], "--read-volume")) {
        fprintf(stderr, "usage: %s --describe | --read-volume\n", argv[0]); return 2;
    }
    oa_amp_i2c *session=NULL;
    int rc=oa_amp_i2c_open(&session,0,NULL);
    if(rc){errno=-rc;perror("open/validate i2c-3");return 1;}
    uint8_t bytes[3];rc=oa_amp_i2c_read_volume(session,bytes);
    int closed=oa_amp_i2c_close(&session);
    if(rc){errno=-rc;perror("AD82587 read");return 1;}
    if(closed){errno=-closed;perror("close i2c-3");return 1;}
    describe();
    printf("completed=2 register03=%02x register04=%02x register05=%02x\n",
           bytes[0], bytes[1], bytes[2]);
    return 0;
}
