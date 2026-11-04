#ifndef OA_SVG_WIDGET_STATE_H
#define OA_SVG_WIDGET_STATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * SVGWindow/SVGTextBox widget-state qualification used by renderItems.
 * Recovered from the shipped Thumb bodies:
 *   SVGWindow::setFocus   0xeeebd4 (state at +0x6c: 0/1/2/3)
 *   SVGWindow::setActive  0xeee4ec
 *   SVGTextBox::setFocus  0x10c8a28 (redraw when the state changes)
 *   SVGTextBox::setElipses 0x10c6f48 (byte +0x1b1, mode +0x220)
 */
struct oa_svg_widget_state_ops {
    void (*base_set_focus)(void *window,int focus); /* UIMessageTarget::setFocus */
    int (*is_created)(void *window);
    void (*draw_text_box)(void *text_box);
    void (*perform_text_slide)(void *text_box);
};

void oa_svg_window_set_focus(void *window,int focus,
    const struct oa_svg_widget_state_ops *o);
void oa_svg_window_set_active(void *window,int active);
void oa_svg_text_box_set_focus(void *text_box,int focus,
    const struct oa_svg_widget_state_ops *o);
void oa_svg_text_box_set_elipses(void *text_box,int on);

#ifdef __cplusplus
}
#endif

#endif
