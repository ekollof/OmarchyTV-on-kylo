#ifndef OPENAURORA_EFFECT_WORKER_H
#define OPENAURORA_EFFECT_WORKER_H

#include <stdint.h>

#include "openaurora/easi_effects.h"
#include "openaurora/video_mute.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Single-slot worker for blocking replacement-effect HAL calls.
 *
 * The host service API mutates hold/bookkeeping state and must only be used on
 * the control loop, so the caller resolves service pointers there and copies
 * raw pointers into the job. The worker thread performs no host API lookup;
 * it calls only the copied set_mute function or the injected video-mute
 * backend. A provider reload between submit and execution can invalidate the
 * copied pointer, so jobs are short-lived and this is a temporary bridge, not
 * an ownership/claim proof.
 *
 * One pending job OR one untaken result. A second submit before take() returns
 * -EBUSY, preserving effect order. No backend close happens here.
 */
typedef struct oa_effect_worker oa_effect_worker;

typedef struct {
    enum oa_easi_effect effect; /* MUTE_ON/OFF or HIDE/SHOW only */
    int arg;                    /* unused; the effect encodes the state */
    int (*set_mute)(int);       /* resolved from OA_SVC_AUDIO on the loop */
    oa_video_mute *vm;          /* injected backend, called directly */
    uint64_t ticket;            /* filled by submit(); callers ignore it */
} oa_effect_job;

typedef struct {
    uint64_t ticket;
    enum oa_easi_effect effect;
    int status;  /* oa_easi_sink_status: APPLIED or FAILED */
    int rc;      /* raw backend return (0 or negative errno) */
} oa_effect_completion;

/* All APIs return 0 or negative errno, except take (1 copied / 0 not ready).
 * create copies the job template's static fields; submit copies all job data.
 * quiesce refuses new work, drains accepted work without replay, then tries a
 * nonblocking join and returns -EAGAIN until joined. join blocks. destroy
 * requires a joined worker and discards an untaken result. */
int oa_effect_worker_create(oa_effect_worker **out);
int oa_effect_worker_submit(oa_effect_worker *w, const oa_effect_job *job,
                            uint64_t *ticket);
int oa_effect_worker_take(oa_effect_worker *w, oa_effect_completion *out);
int oa_effect_worker_quiesce(oa_effect_worker *w);
int oa_effect_worker_join(oa_effect_worker *w);
int oa_effect_worker_destroy(oa_effect_worker *w);

#ifdef __cplusplus
}
#endif

#endif
