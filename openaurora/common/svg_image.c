#include "openaurora/svg_image.h"
#include <string.h>
#include <stddef.h>

/* SVGImage::create (0x10c9638): setId, fetch the UIImageResource through the
 * context image supplier, copy the rectangle, offset it by the parent's
 * client rect when a parent exists, then imageCreate. The supplier's return
 * value gates imageCreate entirely. */
void oa_svg_image_create(void *img,uint32_t id,const uint32_t rect[4],
    void *parent,const struct oa_svg_image_ops *o)
{
    _Alignas(max_align_t) uint8_t resource[OA_SVG_IMAGE_RESOURCE_BYTES];
    uint32_t local[4];
    if(!img || !rect || !o || !o->set_id || !o->get_image_supplier ||
       !o->supplier_get_resource || !o->image_create || (parent && !o->get_client_rect))return;
    memset(resource,0,sizeof resource);
    o->set_id(img,(int32_t)id);
    {
        void *supplier=o->get_image_supplier(img);
        if(!o->supplier_get_resource(supplier,id,resource))return;
    }
    memcpy(local,rect,sizeof local);
    if(parent) {
        uint32_t client[4];
        o->get_client_rect(parent,client);
        /* EASIRect::offset: x adds to words 0 and 2, y to words 1 and 3. */
        local[0]+=client[0];local[1]+=client[1];
        local[2]+=client[0];local[3]+=client[1];
    }
    o->image_create(img,resource,local,parent);
}

/* SVGImage::destroy (0x10c8ef4): SVGWindow::destroy(this) followed by
 * SVGSurface::setCleanUISurface(this+0x9c). */
void oa_svg_image_destroy(void *img,const struct oa_svg_image_ops *o)
{
    if(!img || !o || !o->window_destroy || !o->set_clean_ui_surface)return;
    o->window_destroy(img);
    o->set_clean_ui_surface((void *)((char *)img+0x9c));
}
