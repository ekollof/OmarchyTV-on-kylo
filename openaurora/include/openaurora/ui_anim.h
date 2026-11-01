/* UIAnimationController rules recovered in UI-ANIMATIONS.
 * List rectangles use SVGListBoxBase's truncating blend. Dialog progress uses
 * SVGDialog's 1+(p-1)^3 cap. Enablement starts off. Duration below 1 is rejected.
 * Cancel drops an entry; finalise applies the end state, then drops it. */
#ifndef OPENAURORA_UI_ANIM_H
#define OPENAURORA_UI_ANIM_H
#include <stdint.h>
enum { OA_ANIM_LIST = 0, OA_ANIM_DIALOG = 1 };
typedef struct {
    int x, y, w, h;
} oa_anim_rect;
typedef struct oa_ui_anim oa_ui_anim;
typedef void (*oa_anim_fn)(int id, int kind, const oa_anim_rect *rect, float eased, int done,
                           void *ud);
oa_ui_anim *oa_ui_anim_create(void);
void oa_ui_anim_destroy(oa_ui_anim *anim);
int oa_ui_anim_set_enabled(oa_ui_anim *anim, int on);
int oa_ui_anim_enabled(const oa_ui_anim *anim);
int oa_ui_anim_start(oa_ui_anim *anim, int id, int kind, const oa_anim_rect *from,
                     const oa_anim_rect *to, uint32_t duration, uint32_t now);
int oa_ui_anim_cancel(oa_ui_anim *anim, int id);
int oa_ui_anim_finalise(oa_ui_anim *anim, int id);
int oa_ui_anim_sample(oa_ui_anim *anim, uint32_t now);
int oa_ui_anim_get(const oa_ui_anim *anim, int id, int *kind, oa_anim_rect *rect, float *eased,
                   uint32_t *started, uint32_t *duration);
void oa_ui_anim_bind(oa_ui_anim *anim, oa_anim_fn fn, void *ud);

/* AnimationHelper text slide — recovered from this TV's aurora.elf:
 *   0xef0210 calculateSlidingDuration(int distance)
 *       duration = pause + pause + (distance / speed) * 1000.0f     (integer division first)
 *   0xef0264 calculateSlidingMargin(uint elapsed, float duration, int distance, bool reverse)
 *       elapsed <= pause            -> 0
 *       otherwise frac = (elapsed - pause) / (duration - 2*pause), clamped to 1;
 *       margin = (int)(distance * frac); reverse returns distance - margin.
 * The helper constructor (0xeefc54) sets pause = 800.0f (0xeefc8c) and speed = 50; the 1000.0f
 * multiplier is the constant at 0xef0260. Note the vendor result therefore ends at `distance`
 * (0 when reversed) — the earlier "returns to zero" note only holds for the reverse direction. */
#define OA_ANIM_SLIDE_PAUSE_MS 800
#define OA_ANIM_SLIDE_SPEED    50
/* SVGTextBox::onAnimationProgress @0x10c8e48: when the slide progress exceeds 0.99 (constant at
 * 0x10c8ee8) and the sliding state is 3, it kills any pending timer and arms one with
 * UIFrameworkTask::setTimer(target, 2000, 200) — a single 2000 ms shot, not a repeating timer. */
#define OA_ANIM_SLIDE_REPEAT_MS 2000
float oa_anim_slide_duration(int distance);
int oa_anim_slide_margin(uint32_t elapsed_ms, float duration_ms, int distance, int reverse);

/* One text-slide run. distance is the overflow in pixels (isTextSlidingNeeded @0x10c76b0 computes
 * it as text_width - box_width); reverse mirrors the vendor flag. The caller samples offset() and
 * arms OA_ANIM_SLIDE_REPEAT_MS after running() drops. */
typedef struct {
    int distance;
    int reverse;
    int active;
    uint32_t started;
    float duration;
} oa_slide;
void oa_slide_start(oa_slide *s, int distance, int reverse, uint32_t now);
int oa_slide_offset(const oa_slide *s, uint32_t now);   /* px, 0 when not active */
int oa_slide_running(const oa_slide *s, uint32_t now);  /* 0 once the duration elapsed */
float oa_slide_progress(const oa_slide *s, uint32_t now); /* vendor 0..1 progress; repeat above 0.99 */

/* Full-screen animation, recovered from hal::OpenGLGraphicsContext (0xb32910) and its caller
 * SVGDialog::startAnimation (0x10c5009):
 *
 *   UIAnimationController progress p is eased by SVGDialog::onAnimationProgress (0x10c4dac) to
 *   min(1, 1+(p-1)^3) and pushed through UIFramework::setFullScreenAnimationProgress. destroy()
 *   forces 1.0 when the dialog target is still below 1.
 *
 *   SVGDialog::startAnimation maps dialog modes 57..64 through the byte table at 0x153b80c
 *   (01 02 03 04 05 06 07 08) to hal types 1..8, and starts the dialog controller for 1000 ms
 *   (float 0x447a0000 in the literal pool at 0x10c5094).
 *
 *   iterateFullScreenAnimation clamps p to 0..1 and rewrites the vertex positions:
 *     type 1            vertices 1,2: x *= 0.5 + 0.5p; y *= 2/3 + p/3
 *     types 2/3         all 4 vertices: x += -(1-p)/6 or +(1-p)/6
 *     types 4/5         all 4 vertices: y += -(1-p)/6 or +(1-p)/6
 *     type 6            no geometry
 *     type 7            vertices 0..3: x,y *= 2/3 + p/3
 *     type 8            vertices 0..3: x,y *= 1 + (1-p)*sin(pi*(6p))*(1/3)
 *   The pool at 0xb32aa8 holds a **double** pi (bytes 18 2d 44 54 fb 21 09 40 =
 *   0x400921fb54442d18); the float 2/3 (0x3f2aaaab), 1/3 (0x3eaaaaab) and 1/6
 *   (0x3e2aaaab) follow at 0xb32ab4/0xb32ab8/0xb32abc. The 6p argument is widened
 *   to double for the pi multiply, then narrowed to float for sinf, so that
 *   ordering is kept below. Vertex x/y live at +0x00/+0x04 of a 0x18-byte
 *   TextureVertex; the caller owns the layout. */
enum {
    OA_FS_NONE = 0, OA_FS_SCALE_AXES = 1, OA_FS_SLIDE_NEG = 2, OA_FS_SLIDE_POS = 3,
    OA_FS_SLIDE_NEG_SECOND = 4, OA_FS_SLIDE_POS_SECOND = 5, OA_FS_UNIFORM_ONLY = 6,
    OA_FS_SCALE = 7, OA_FS_WOBBLE = 8
};
void oa_fs_apply(int type, float progress, float *vx, float *vy, int vertices);

/* Scene-level form of the same transform, in the vendor vertex space (clip/normalized, not pixels):
 *   x' = dx + sx*x,  y' = dy + sy*y
 * affine=0 means the type is a keystone (type 1 moves only vertices 1 and 2) and cannot be
 * expressed as one affine scene transform; the composite stage needs a mesh warp for it. Types 2/3
 * shift x and types 4/5 shift y. Cross-checked against oa_fs_apply in tests/ui_anim_review.c. */
typedef struct {
    float sx, sy, dx, dy;
    int affine;
} oa_fs_scene;
void oa_fs_scene_transform(int type, float progress, oa_fs_scene *out);
#endif
