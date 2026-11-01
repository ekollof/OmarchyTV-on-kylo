#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "openaurora/ui_anim.h"
#include "openaurora/ui_time.h"

#define OA_ANIM_SLOTS 4

struct oa_anim_entry {
    int used;
    int id;
    int kind;
    uint32_t started;
    uint32_t duration;
    oa_anim_rect from, to, current;
    float eased;
};
struct oa_ui_anim {
    int enabled;
    struct oa_anim_entry slot[OA_ANIM_SLOTS];
    oa_anim_fn fn;
    void *ud;
};

oa_ui_anim *oa_ui_anim_create(void)
{
    return calloc(1, sizeof(oa_ui_anim));
}
void oa_ui_anim_destroy(oa_ui_anim *anim) { free(anim); }

int oa_ui_anim_set_enabled(oa_ui_anim *anim, int on)
{
    if (!anim || (on != 0 && on != 1)) return -1;
    anim->enabled = on;
    return 0;
}
int oa_ui_anim_enabled(const oa_ui_anim *anim) { return anim && anim->enabled; }

static struct oa_anim_entry *find_entry(oa_ui_anim *anim, int id)
{
    for (int i = 0; i < OA_ANIM_SLOTS; i++)
        if (anim->slot[i].used && anim->slot[i].id == id) return &anim->slot[i];
    return NULL;
}
static const struct oa_anim_entry *find_entry_c(const oa_ui_anim *anim, int id)
{
    for (int i = 0; i < OA_ANIM_SLOTS; i++)
        if (anim->slot[i].used && anim->slot[i].id == id) return &anim->slot[i];
    return NULL;
}

static float dialog_ease(float p)
{
    float d = p - 1.f;
    float eased = d * d * d + 1.f;
    if (eased > 1.f) eased = 1.f;
    return eased;
}
static int trunc_blend(int from, int to, float p)
{
    float value = (float)to * p + (float)from * (1.f - p);
    /* INT_MAX rounds to 2^31 as float on ARM. Avoid an out-of-range cast
     * without changing the recovered float/truncation path for UI geometry. */
    if (value >= (float)INT_MAX) return INT_MAX;
    if (value <= (float)INT_MIN) return INT_MIN;
    return (int)value;
}
static void apply(struct oa_anim_entry *e, float p)
{
    float eased = e->kind == OA_ANIM_DIALOG ? dialog_ease(p) : p;
    e->eased = eased;
    float blend = e->kind == OA_ANIM_DIALOG ? eased : p;
    e->current.x = trunc_blend(e->from.x, e->to.x, blend);
    e->current.y = trunc_blend(e->from.y, e->to.y, blend);
    e->current.w = trunc_blend(e->from.w, e->to.w, blend);
    e->current.h = trunc_blend(e->from.h, e->to.h, blend);
}
static void notify(oa_ui_anim *anim, struct oa_anim_entry *e, int done)
{
    /* A callback may cancel/restart this slot. Its event stays stable. */
    struct oa_anim_entry event = *e;
    if (anim->fn) anim->fn(event.id, event.kind, &event.current, event.eased, done, anim->ud);
}

int oa_ui_anim_start(oa_ui_anim *anim, int id, int kind, const oa_anim_rect *from,
                     const oa_anim_rect *to, uint32_t duration, uint32_t now)
{
    if (!anim || !anim->enabled || duration < 1) return -1;
    if (kind != OA_ANIM_LIST && kind != OA_ANIM_DIALOG) return -1;
    if (kind == OA_ANIM_LIST && (!from || !to)) return -1;
    struct oa_anim_entry *e = find_entry(anim, id);
    if (!e) {
        for (int i = 0; i < OA_ANIM_SLOTS; i++)
            if (!anim->slot[i].used) { e = &anim->slot[i]; break; }
    }
    if (!e) return -1;
    memset(e, 0, sizeof *e);
    e->used = 1;
    e->id = id;
    e->kind = kind;
    e->started = now;
    e->duration = duration;
    if (from) e->from = *from;
    if (to) e->to = *to;
    apply(e, 0.f);
    notify(anim, e, 0);
    return 0;
}
int oa_ui_anim_cancel(oa_ui_anim *anim, int id)
{
    struct oa_anim_entry *e = anim ? find_entry(anim, id) : NULL;
    if (!e) return -1;
    e->used = 0;
    return 0;
}
int oa_ui_anim_finalise(oa_ui_anim *anim, int id)
{
    struct oa_anim_entry *e = anim ? find_entry(anim, id) : NULL;
    if (!e) return -1;
    apply(e, 1.f);
    e->used = 0;
    notify(anim, e, 1);
    return 0;
}
int oa_ui_anim_sample(oa_ui_anim *anim, uint32_t now)
{
    if (!anim) return -1;
    for (int i = 0; i < OA_ANIM_SLOTS; i++) {
        struct oa_anim_entry *e = &anim->slot[i];
        if (!e->used) continue;
        uint32_t elapsed = oa_ui_time_elapsed(now, e->started);
        if (elapsed >= e->duration) {
            apply(e, 1.f);
            e->used = 0;
            notify(anim, e, 1);
        } else {
            apply(e, (float)elapsed / (float)e->duration);
            notify(anim, e, 0);
        }
    }
    /* Include entries started by completion callbacks, including in slots
     * already visited. Otherwise the module may disarm their only timer. */
    int running = 0;
    for (int i = 0; i < OA_ANIM_SLOTS; i++) running += anim->slot[i].used != 0;
    return running;
}
int oa_ui_anim_get(const oa_ui_anim *anim, int id, int *kind, oa_anim_rect *rect, float *eased,
                   uint32_t *started, uint32_t *duration)
{
    const struct oa_anim_entry *e = anim ? find_entry_c(anim, id) : NULL;
    if (!e) return -1;
    if (kind) *kind = e->kind;
    if (rect) *rect = e->current;
    if (eased) *eased = e->eased;
    if (started) *started = e->started;
    if (duration) *duration = e->duration;
    return 0;
}
void oa_ui_anim_bind(oa_ui_anim *anim, oa_anim_fn fn, void *ud)
{
    if (!anim) return;
    anim->fn = fn;
    anim->ud = ud;
}

/* AnimationHelper text slide. Both functions follow the vendor's instructions exactly; the only
 * addition is a guard where the vendor would divide by zero (distance < speed makes the movement
 * interval 0, and its float division yields +inf, which it then clamps to 1). */
float oa_anim_slide_duration(int distance)
{
    int steps = distance > 0 ? distance / OA_ANIM_SLIDE_SPEED : 0;
    return (float)OA_ANIM_SLIDE_PAUSE_MS + (float)OA_ANIM_SLIDE_PAUSE_MS +
           (float)steps * 1000.0f;
}

int oa_anim_slide_margin(uint32_t elapsed_ms, float duration_ms, int distance, int reverse)
{
    const float pause = (float)OA_ANIM_SLIDE_PAUSE_MS;
    int margin = 0;

    if ((float)elapsed_ms > pause) {
        float span = duration_ms - pause - pause;   /* vendor: duration - (pause + pause) */
        float frac;
        if (span > 0.0f) {
            frac = ((float)elapsed_ms - pause) / span;
            if (frac > 1.0f) frac = 1.0f;           /* vendor clamps only the upper bound */
        } else {
            frac = 1.0f;                            /* vendor: divide by 0 -> inf -> clamp */
        }
        {
            float value = (float)distance * frac;
            /* INT_MAX rounds to 2^31 as float on ARM; avoid an out-of-range cast while
             * keeping the vendor's float multiply + truncation for ordinary geometry. */
            if (value >= (float)INT_MAX) margin = INT_MAX;
            else if (value <= (float)INT_MIN) margin = INT_MIN;
            else margin = (int)value;
        }
    }
    if (reverse) margin = distance - margin;
    return margin;
}

void oa_slide_start(oa_slide *s, int distance, int reverse, uint32_t now)
{
    if (!s) return;
    if (distance < 0) distance = 0;
    s->distance = distance;
    s->reverse = reverse ? 1 : 0;
    s->started = now;
    s->duration = oa_anim_slide_duration(distance);
    s->active = 1;
}

int oa_slide_offset(const oa_slide *s, uint32_t now)
{
    if (!s || !s->active) return 0;
    return oa_anim_slide_margin(oa_ui_time_elapsed(now, s->started), s->duration,
                                s->distance, s->reverse);
}

int oa_slide_running(const oa_slide *s, uint32_t now)
{
    if (!s || !s->active) return 0;
    return ((float)oa_ui_time_elapsed(now, s->started) < s->duration) ? 1 : 0;
}

float oa_slide_progress(const oa_slide *s, uint32_t now)
{
    float progress;
    if (!s || !s->active || s->duration <= 0.0f) return 1.0f;
    /* vendor onAnimationProgress receives a 0..1 progress; the repeat is armed above 0.99 */
    progress = (float)oa_ui_time_elapsed(now, s->started) / s->duration;
    if (progress > 1.0f) progress = 1.0f;
    return progress;
}

/* Full-screen animation geometry, from hal::OpenGLGraphicsContext::iterateFullScreenAnimation
 * @0xb32910. The constants are the literal pool at 0xb32aa8 (pi, 2/3, 1/3, 1/6); vertex x/y sit
 * at +0x00/+0x04 of each 0x18-byte entry. Only the vertices the vendor touches are modified. */
void oa_fs_apply(int type, float progress, float *vx, float *vy, int vertices)
{
    /* 0xb32aa8 holds a double pi (0x400921fb54442d18); the three fractions are floats. */
    const double pi = 3.14159265358979323846;
    const float two_thirds = 0.6666667f; /* 0x3f2aaaab */
    const float one_third = 0.33333334f; /* 0x3eaaaaab */
    const float one_sixth = 0.16666667f; /* 0x3e2aaaab */
    float p = progress;

    if (!vx || !vy || vertices <= 0) return;
    /* HAL clamp: negative -> 0, above 1 -> 1 (NaN is left alone, as the comparisons are) */
    if (p < 0.0f) p = 0.0f;
    else if (p > 1.0f) p = 1.0f;

    switch (type) {
    case OA_FS_SCALE_AXES: {
        /* Type 1 scales x by 0.5+0.5p and y by 2/3+p/3, but only vertices 1 and 2. */
        float sx = 0.5f + p * 0.5f;
        float sy = two_thirds + p * one_third;
        for (int i = 1; i <= 2 && i < vertices; i++) {
            vx[i] *= sx;
            vy[i] *= sy;
        }
        break;
    }
    case OA_FS_SLIDE_NEG:
    case OA_FS_SLIDE_POS:
    case OA_FS_SLIDE_NEG_SECOND:
    case OA_FS_SLIDE_POS_SECOND: {
        /* Types 2..5 shift x by (1-p)/6; *SECOND works on vertices 4..7, the others on 0..3. */
        float shift = (1.0f - p) * one_sixth;
        int start = (type == OA_FS_SLIDE_NEG_SECOND || type == OA_FS_SLIDE_POS_SECOND) ? 4 : 0;
        if (type == OA_FS_SLIDE_NEG || type == OA_FS_SLIDE_NEG_SECOND) shift = -shift;
        for (int i = start; i < start + 4 && i < vertices; i++)
            vx[i] += shift;
        break;
    }
    case OA_FS_SCALE: {
        /* Type 7 scales both axes of vertices 0..3 by 2/3+p/3. */
        float s = two_thirds + p * one_third;
        for (int i = 0; i < 4 && i < vertices; i++) {
            vx[i] *= s;
            vy[i] *= s;
        }
        break;
    }
    case OA_FS_WOBBLE: {
        /* Type 8: 1 + (1-p)*sin(float(pi * (double)(6p)))*(1/3) on vertices 0..3. The vendor
         * widens 6p to double for the pi multiply and narrows again for sinf; keep that order. */
        float three_p = p * 3.0f;
        float six_p = three_p + three_p;
        float s = 1.0f + (1.0f - p) * sinf((float)((double)six_p * pi)) * one_third;
        for (int i = 0; i < 4 && i < vertices; i++) {
            vx[i] *= s;
            vy[i] *= s;
        }
        break;
    }
    case OA_FS_UNIFORM_ONLY:
    default:
        break;                            /* no vertex change */
    }
}
