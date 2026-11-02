#ifndef OPENAURORA_TEXT_LAYOUT_H
#define OPENAURORA_TEXT_LAYOUT_H
#include <stdint.h>
/* EASIRect corner order verified through RectHandler::handleSVGElement.
 * Widget-local coordinates, not final scanout coordinates. */
struct oa_text_rect { int32_t left, top, right, bottom; };
/* SVGTextBox::renderBackground 10c624e..10c6296, lines==1 path:
 * rescale textPos height by surface-height/mask-height, keeping top/left/right.
 * Float32 intermediates and truncation toward zero; default FP environment.
 * Reject zero denominator/range errors without changing rect. Does not apply
 * mask translation, row offsets, alignment or a baseline. Returns errno value. */
int oa_text_rect_scale_height(struct oa_text_rect *, int32_t surface, int32_t mask);
#endif
