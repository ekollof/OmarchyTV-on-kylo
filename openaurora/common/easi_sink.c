/* Consumer/dispatcher for replacement EASI effects. No device I/O itself. */
#include "openaurora/easi_sink.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>

void oa_easi_sink_init(oa_easi_sink *s, const oa_easi_sink_io *io)
{
    if (!s)
        return;
    memset(s, 0, sizeof(*s));
    s->state.last_status = OA_EASI_NO_BACKEND;
    if (io)
        s->io = *io;
}

int oa_easi_sink_dispatch(oa_easi_sink *s, enum oa_easi_effect effect, int arg)
{
    int status;
    if (!s || !s->io.apply) {
        status = OA_EASI_NO_BACKEND;
        if (s)
            s->state.no_backend++;
    } else {
        status = s->io.apply(s->io.ud, effect, arg);
        /* Collapse any unexpected value into a failure; never count a bogus
         * backend return as an applied effect. */
        if (status < OA_EASI_APPLIED || status > OA_EASI_NO_BACKEND)
            status = OA_EASI_FAILED;
        switch (status) {
        case OA_EASI_APPLIED: s->state.applied++; break;
        case OA_EASI_UNSUPPORTED: s->state.unsupported++; break;
        case OA_EASI_FAILED: s->state.failed++; break;
        default: s->state.no_backend++; break;
        }
    }
    if (s) {
        s->state.dispatched++;
        s->state.last_status = status;
        s->state.last_effect = effect;
    }
    (void)arg;
    return status;
}

int oa_easi_sink_snapshot(const oa_easi_sink *s, oa_easi_sink_state *out)
{
    if (!s || !out)
        return -EINVAL;
    *out = s->state;
    return 0;
}
