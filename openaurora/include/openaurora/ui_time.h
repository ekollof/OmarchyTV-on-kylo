/* FROST_Time/TimeSubtract arithmetic recovered from Aurora 0x744270/0x7443b4.
 * Keep separate from the host's ordinary 64-bit monotonic clock. */
#ifndef OPENAURORA_UI_TIME_H
#define OPENAURORA_UI_TIME_H
#include <stdint.h>
#define OA_UI_TIME_PERIOD UINT32_C(90000000)
#define OA_UI_TIME_QUANTUM UINT32_C(12343)
typedef struct {
    uint32_t previous_raw;
    uint32_t remainder;
    uint32_t correction;
} oa_ui_time_state;

static inline uint32_t oa_ui_time_elapsed(uint32_t now, uint32_t before)
{
    uint32_t elapsed = now - before;
    if (now < before) elapsed += OA_UI_TIME_PERIOD;
    return elapsed;
}

static inline uint32_t oa_ui_time_sample(oa_ui_time_state *s, uint64_t monotonic_ms)
{
    /* The ARM code multiplies/adds in 32-bit registers before reducing. Both
     * comparisons are strictly >; replacing them with >= changes boundaries. */
    uint32_t raw = (uint32_t)monotonic_ms;
    if (raw > OA_UI_TIME_PERIOD) raw %= OA_UI_TIME_PERIOD;
    s->remainder += oa_ui_time_elapsed(raw, s->previous_raw);
    s->previous_raw = raw;
    if (s->remainder > OA_UI_TIME_QUANTUM) {
        s->correction += s->remainder / OA_UI_TIME_QUANTUM;
        s->remainder %= OA_UI_TIME_QUANTUM;
        if (s->correction > OA_UI_TIME_PERIOD) s->correction %= OA_UI_TIME_PERIOD;
    }
    return oa_ui_time_elapsed(raw, s->correction);
}
#endif
