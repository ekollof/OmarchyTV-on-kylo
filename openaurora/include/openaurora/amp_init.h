/* Blocking AD82587 Init orchestration from shipped ARM. Injection only; no
 * native backend/default I/O. Run on an owning worker, never the host loop.
 * Caller serializes state and retains profile, callbacks/context and returned
 * GPIO records through return. Adapt errno-returning transports to the explicit
 * vendor boolean callback contract. No device acquisition or paired shutdown. */
#ifndef OPENAURORA_AMP_INIT_H
#define OPENAURORA_AMP_INIT_H
#include "openaurora/audio_amp.h"
typedef struct { uint32_t pin,level_on,level_off; } oa_amp_gpio;
typedef struct {
    uint8_t image[OA_AD82587_REGISTERS];
    uint8_t reset_cached,mute_cached;
} oa_amp_init_state;
typedef struct {
    void *context;
    void (*prelude)(void *);
    const oa_amp_gpio *(*gpio)(void *,unsigned function); /* 0 reset,1 mute; NULL absent */
    void (*direction)(void *,uint32_t pin,uint32_t direction); /* native call uses1 */
    int (*level)(void *,uint32_t pin,uint32_t value); /* vendor boolean:0 fail/nonzero success */
    void (*delay_ms)(void *,unsigned milliseconds);
    int (*write)(void *,unsigned reg,const uint8_t *value); /* one byte;0 fail/nonzero dispatched */
    uint32_t (*subwoofer_enabled)(void *);
    void (*subwoofer_enable)(void *,int enabled);
    void (*subwoofer_init)(void *);
} oa_amp_init_io;
/* Returns the native Init write-loop result (1 complete/0 first write failed),
 * not GPIO/subwoofer success or audible acceptance. As in firmware, GPIO
 * failures do not stop Init; caches update only on nonzero level returns.
 * Reset(1) uses level_off, Reset(0) uses logical !level_off; mute uses on/off.
 * The image is fully updated before the first write and retained on failure.
 * Invalid arguments return -EINVAL before callbacks or state changes. */
int oa_amp_init(oa_amp_init_state *state,const oa_ad82587_profile *profile,const oa_amp_init_io *io);
#endif
