#ifndef OPENAURORA_AMP_GPIO_QUERY_H
#define OPENAURORA_AMP_GPIO_QUERY_H
#include <stdint.h>
typedef struct {
    uint8_t level;
    uint8_t direction; /* kernel cached direction:0 output,1 input */
} oa_amp_reset_state;
typedef int (*oa_amp_gpio_request)(void *context,int fd,unsigned long command,void *payload);
/* Read physical reset GPIO114 only: kernel GET_LEVEL670d and GET_DIRECTION670f.
 * Never claim/export/configure a pin or call HAL/STBC. This is not the vendor
 * GetDirection wrapper (which mutates pin state before its GET). fd is borrowed
 * through return; caller serializes on a blocking worker. NULL request selects
 * native ARM32 ioctl after checking fd is char132:0; ENOTSUP elsewhere.
 * Injected request follows POSIX0/-1/errno. No replay. Returns0/-errno; output
 * is unchanged on failure. Two GETs are NOT an atomic snapshot or an exclusive
 * lease. Level reads the input register or output latch according to cached
 * direction; direction is software state, not independent physical validation. */
int oa_amp_reset_query(int fd,oa_amp_reset_state *out,oa_amp_gpio_request request,void *context);
#endif
