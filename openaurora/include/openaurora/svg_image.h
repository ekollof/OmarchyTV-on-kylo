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

#ifdef __cplusplus
}
#endif

#endif
