#ifndef OPENAURORA_EASI_SINK_H
#define OPENAURORA_EASI_SINK_H

#include "openaurora/easi_effects.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Consumer for the effects emitted by the replacement EASI backend.
 *
 * The sink is the seam between "a firmware-ordered effect was requested" and
 * "a replacement effect actually happened". It holds an injected apply
 * callback and counts how each effect was handled. A backend that has no real
 * replacement must return OA_EASI_UNSUPPORTED; the sink never fakes success.
 *
 * This is a replacement effect dispatcher, not vendor parity: the shipped EASI
 * methods are C++ methods on live vendor objects and cannot be called here.
 */
enum oa_easi_sink_status {
    OA_EASI_APPLIED = 0,     /* the injected backend accepted the effect */
    OA_EASI_UNSUPPORTED,     /* no replacement backend for this effect */
    OA_EASI_FAILED,          /* a backend was called and reported failure */
    OA_EASI_NO_BACKEND,      /* no apply callback was configured */
    OA_EASI_QUEUED           /* accepted by a worker; result reported later */
};

typedef struct {
    void *ud;
    /* Return one of oa_easi_sink_status. effect/arg come from the ordered
     * EASI emit stream; the callback must not perform blocking device I/O on
     * the host loop. */
    int (*apply)(void *ud, enum oa_easi_effect effect, int arg);
} oa_easi_sink_io;

typedef struct {
    unsigned applied;
    unsigned unsupported;
    unsigned failed;
    unsigned no_backend;
    unsigned queued;
    unsigned completed;        /* worker results collected on the loop */
    unsigned completion_failed;
    unsigned dispatched;
    int last_status;
    enum oa_easi_effect last_effect;
} oa_easi_sink_state;

typedef struct {
    oa_easi_sink_io io;
    oa_easi_sink_state state;
} oa_easi_sink;

void oa_easi_sink_init(oa_easi_sink *s, const oa_easi_sink_io *io);
/* Dispatch one effect; returns the oa_easi_sink_status value and updates the
 * counters. A null sink is OA_EASI_NO_BACKEND. */
int oa_easi_sink_dispatch(oa_easi_sink *s, enum oa_easi_effect effect, int arg);
int oa_easi_sink_snapshot(const oa_easi_sink *s, oa_easi_sink_state *out);
/* Record one collected worker result on the loop. Does not touch a backend. */
void oa_easi_sink_note_completion(oa_easi_sink *s, int status);

#ifdef __cplusplus
}
#endif

#endif
