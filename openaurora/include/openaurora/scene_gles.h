/* MB230 private offscreen EGL/GLES owner; EGL-SCENE report records firmware
 * authority. No native window, swap, HAL init or framebuffer access. All calls
 * must stay on the creating process/thread. No shared contexts or callbacks.
 */
#ifndef OPENAURORA_SCENE_GLES_H
#define OPENAURORA_SCENE_GLES_H
#include <stddef.h>
#include <stdint.h>
typedef struct oa_scene_gles oa_scene_gles;
/* *scene must be NULL. Failure can retain a partially initialized owner there;
 * call close until successful before releasing its code/library dependencies. */
int oa_scene_gles_open(oa_scene_gles **scene, unsigned width, unsigned height);
/* Input: tightly packed, top-down straight RGBA scene. Output: tightly packed
 * bottom-up premultiplied RGBA from glReadPixels, not framebuffer BGRA.
 * Both buffers need width*height*4 bytes. Input is borrowed only through this
 * synchronous call. Types0..8 use the recovered quad;0 is identity, others fade
 * by progress0..1. A return0 proves readback, not scanout or audible/TV parity. */
int oa_scene_gles_render(oa_scene_gles *scene, const uint8_t *rgba, size_t input_size,
                         int type, float progress, uint8_t *bottom_up, size_t output_size);
/* NULL is idempotent. Failure retains the owner and unfinished EGL handles;
 * retry on the same thread. Success releases all handles and sets *scene=NULL. */
int oa_scene_gles_close(oa_scene_gles **scene);
void oa_scene_gles_error(const oa_scene_gles *scene, char *out, size_t size);
#endif
