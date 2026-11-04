#ifndef OPENAURORA_AMP_STBC_H
#define OPENAURORA_AMP_STBC_H
#include <stdint.h>
#include <time.h>
#include <linux/videodev2.h>
typedef struct { uint8_t byte16,byte8; } oa_amp_stbc_state;
/* Blocking synchronous request, POSIX-1/errno on transport failure, otherwise
 * nonnegative. Consume all control/payload pointers before return. Caller
 * retains fd, callback code/context and coordinates V4L2/MCU authority.
 * There is deliberately NO native fallback or device/library initialization. */
typedef int (*oa_amp_stbc_request)(void *context,int fd,unsigned long command,struct v4l2_ext_controls *list);
/* Physical mute GPIO208 -> MCU GPIO8 only. Returns0 or negative errno.
 * Read bytes are raw reply bits23..16 and15..8, NOT a proved direction/level
 * pair or the normal GPIO direction enum. Aurora's GetLevel caller consumes
 * the first output. A read
 * does not establish an exact rollback/ownership snapshot. Output atomic on
 * failure; no EINTR replay. NULL request returns ENOTSUP without any I/O. */
int oa_amp_stbc_query(int fd,oa_amp_stbc_state *out,oa_amp_stbc_request request,void *context);
/* Vendor pin level0/1 -> MCU command3/2. Physical/electrical and audible effects
 * remain unverified. Explicit callback only; no live write acceptance. Failure
 * does not imply the MCU had no effect, and provides no automatic rollback. */
int oa_amp_stbc_set_level(int fd,unsigned level,oa_amp_stbc_request request,void *context);
#endif
