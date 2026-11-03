/* MB230 private EGL/GLES owner; EGL-SCENE records the initial pbuffer contract.
 * open_window is an explicit hardware-changing extension; open stays offscreen.
 * Neither path initializes platform HAL. All calls
 * must stay on the creating process/thread. No shared contexts or callbacks.
 */
#ifndef OPENAURORA_SCENE_GLES_H
#define OPENAURORA_SCENE_GLES_H
#include <stddef.h>
#include <stdint.h>
#include "openaurora/font_draw.h"
typedef struct oa_scene_gles oa_scene_gles;
/* *scene must be NULL. Failure can retain a partially initialized owner there;
 * call close until successful before releasing its code/library dependencies. */
int oa_scene_gles_open(oa_scene_gles **scene, unsigned width, unsigned height);
/* Native fbdev display1 window; affects real scanout. Caller must establish the
 * FRAMEBUFFER_2 and MALI_MAU_SWITCH profile from firmware and preserve/restore
 * hardware state. Not a harmless query. Dimensions are retained internally. */
int oa_scene_gles_open_window(oa_scene_gles **scene, unsigned width, unsigned height);
/* Input: tightly packed, top-down straight RGBA scene. Output: tightly packed
 * bottom-up premultiplied RGBA from glReadPixels, not framebuffer BGRA.
 * Pbuffer output needs width*height*4 bytes. A window requires NULL output/size0
 * and commits through eglSwapBuffers instead. Nonnull input needs full size;
 * NULL input/size0 reuses a previously uploaded texture. Input is borrowed only through this
 * synchronous call. Types0..8 use the recovered quad;0 is identity, others fade
 * by progress0..1. A return0 proves readback, not scanout or audible/TV parity. */
int oa_scene_gles_render(oa_scene_gles *scene, const uint8_t *rgba, size_t input_size,
                         int type, float progress, uint8_t *bottom_up, size_t output_size);
/* Direct coverage textures, not CPU-precomposited glyph RGBA. Input is borrowed
 * through the synchronous render/readback/swap. Caller owns text layout and
 * supplies absolute top-down glyph rectangles (bearings already included).
 * Tight width*height coverage bytes; optional clip is copied, never followed.
 * This lower API does not yet add glyph commands to the broker/UI service ABI. */
struct oa_scene_glyph {
    int x,y,width,height;
    const uint8_t *coverage;
    size_t coverage_size;
    uint8_t rgba[4];
    int clipped;
    struct oa_font_clip clip;
};
/* Identity base scene plus ordered glyphs, then the vendor's RGBA FBO sampling
 * pass. Its second SRC_ALPHA blend attenuates translucent RGB again; output
 * retains bottom-up RGBA storage, but is not ordinary single-pass premultiplication.
 * Existing output sizes and owner checks apply. NULL base reuses the cached
 * RGBA image. Maximum4096 glyphs,64MiB total coverage per call (project limits).
 * Validation precedes drawing.
 * Generic full-scene effects remain in render(); text/effects integration in
 * the broker/service pipeline is still pending. */
int oa_scene_gles_render_glyphs(oa_scene_gles *scene,const uint8_t *rgba,size_t input_size,
                               const struct oa_scene_glyph *glyphs,size_t count,
                               uint8_t *bottom_up,size_t output_size);
/* Blocking completion of preceding work on this owned context. Does not prove
 * physical scanout. Separate process uses this before releasing frame input. */
int oa_scene_gles_finish(oa_scene_gles *scene);
/* NULL is idempotent. Failure retains the owner and unfinished EGL handles;
 * retry on the same thread. Success releases all handles and sets *scene=NULL. */
int oa_scene_gles_close(oa_scene_gles **scene);
void oa_scene_gles_error(const oa_scene_gles *scene, char *out, size_t size);
#endif
