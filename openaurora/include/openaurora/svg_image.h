#ifndef OA_SVG_IMAGE_H
#define OA_SVG_IMAGE_H

#include <stdint.h>
#define OA_SVG_IMAGE_RESOURCE_BYTES 32u

#ifdef __cplusplus
extern "C" {
#endif

/*
 * SVGImage::create / SVGImage::destroy orchestration, recovered from the
 * shipped ARM bodies at 0x10c9638 and 0x10c8ef4.
 *
 * The ops are the vendor calls the shipped bodies make. imageCreate itself
 * (0x10c9430, the SVG surface/draw subsystem) remains a callback; the
 * surrounding create logic (UIWindow::setId, SVGContext::getImageSupplier +
 * supplier resource lookup, optional parent client-rect offset) and destroy
 * (SVGWindow::destroy + SVGSurface::setCleanUISurface) are implemented here.
 * img points to backing storage containing its surface at+0x9c, not an integer
 * handle. Missing required callbacks make the operation a no-op, before any
 * callback runs. create requires get_client_rect only with a non-NULL parent.
 */
struct oa_svg_image_ops {
    void (*set_id)(void *img,int32_t id);
    void *(*get_image_supplier)(void *img);
    /* Supplier virtual call: out has32 bytes aligned to max_align_t. The
     * resource and rectangle are borrowed only until image_create returns;
     * copy anything retained. Success is nonzero. */
    int (*supplier_get_resource)(void *supplier,uint32_t id,void *out);
    /* UIWindow::getClientRect, returned through a hidden pointer. */
    void (*get_client_rect)(void *parent,uint32_t out[4]);
    void (*image_create)(void *img,const void *resource,
        const uint32_t rect[4],void *parent);
    void (*window_destroy)(void *img);
    void (*set_clean_ui_surface)(void *img);
};

void oa_svg_image_create(void *img,uint32_t id,const uint32_t rect[4],
    void *parent,const struct oa_svg_image_ops *o);
void oa_svg_image_destroy(void *img,const struct oa_svg_image_ops *o);

/*
 * SVGImage::imageCreate(UIImageResource const&, EASIRect const&, UIWindow*)
 * (0x10c9430): set parent/window rect, then either resize+draw on the created
 * window or create the surface, draw and UIWindow::create. The vendor calls
 * are behind this second ops table; floats are raw IEEE words so the module
 * has no float ABI dependency at its own boundary.
 */
struct oa_svg_image_surface_ops {
    void (*set_parent_window)(void *img,void *parent);
    void (*set_window_rect)(void *img,const uint32_t rect[4]);
    int (*is_created)(void *img);
    void (*resize_ui_surface)(void *img,const uint32_t rect[4]);
    void *(*target_handle)(void *img);
    void (*release_target)(void *img,void *target);
    void (*draw_image_resource)(void *img,const void *resource,uint32_t x,
        uint32_t y,uint32_t flag,void *target,uint32_t tiling,int32_t f0,
        int32_t f1,uint32_t scaling);
    void (*draw_image_resource_scaled)(void *img,const void *resource,
        uint32_t x,uint32_t y,void *target);
    void *(*create_surface)(void *img,const uint32_t rect[4],int a,int b);
    void (*set_ui_surface)(void *img,void *surface);
    void (*create_window)(void *img);
    uint32_t (*image_field)(void *img,uint32_t offset);
};
void oa_svg_image_image_create(void *img,const void *resource,
    const uint32_t rect[4],void *parent,
    const struct oa_svg_image_surface_ops *o);

#ifdef __cplusplus
}
#endif

#endif
