/* Fake libNDAL_EX BLAST receiver for the IR helper lifecycle test.
 *
 * The real NDAL_VideoExt_BLAST_RX blocks in blast_io_get_key while the remote
 * is idle. This fake blocks for 150 ms per call, then returns one press and one
 * release before settling into the no-key path. oa-ir-blast calls it in the
 * helper process and writes the records to its stdout pipe.
 */
#include <stdint.h>
#include <unistd.h>

static int calls;

int NDAL_VideoExt_BLAST_SetRcProtocol(unsigned protocol)
{
    (void)protocol;
    return 0;
}

int NDAL_VideoExt_BLAST_RX(uint32_t *code, uint8_t *aux, uint8_t *flag)
{
    usleep(150000);
    int n = __sync_fetch_and_add(&calls, 1);
    if (n == 0) {
        *code = 0x3056;
        *aux = 0;
        *flag = 0;
        return 0;
    }
    if (n == 1) {
        *code = 0x3056;
        *aux = 0;
        *flag = 1;
        return 0;
    }
    return 1;
}

void ndcommon_v4l2_set_fd(int fd)
{
    (void)fd;
}
