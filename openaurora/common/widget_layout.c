#include "openaurora/widget_layout.h"
#include <errno.h>
#include <limits.h>
#include <string.h>

int oa_widget_group_rect(const struct oa_binsvg *v, const char *group,
                         const char *mask_name, const struct oa_text_rect *surface,
                         const char *name, struct oa_binsvg_rect *out)
{
    if (!v || !v->data || !group || !*group || !mask_name || !*mask_name ||
        !surface || !name || !*name || !out) return EINVAL;
    size_t at=0, depth=0;
    int mask_available=0, measured=0;
    struct oa_text_rect mask={0};
    struct oa_horizontal_scale scale=OA_HORIZONTAL_SCALE_INITIALIZER;
    while (at<v->size) {
        struct oa_binsvg_element e;
        int rc=oa_binsvg_next(v,&at,&e);
        if (rc) return rc;
        if (e.tag==8) {
            if (depth || !strcmp(e.id,group)) ++depth;
        } else if (e.tag==9 && depth) {
            if (!--depth) return ENOENT;
        }
        if (!depth) continue;
        if (e.tag>=10) return ENOTSUP;
        if (e.tag!=1) continue;
        struct oa_binsvg_rect rect;
        if ((rc=oa_binsvg_rectangle(v,e.offset,&rect))) return rc;
        /* RectHandler transforms before addRectangle. The mask's own first
         * encounter therefore cannot see itself in the empty container. */
        if (mask_available) {
            if (!measured) {
                if ((rc=oa_horizontal_scale_measure(&scale,surface,&mask))) return rc;
                measured=1;
            }
            if ((rc=oa_text_rect_stretch_mask(&rect.bounds,surface,&mask,&scale))) return rc;
        }
        if (!mask_available && !strcmp(e.id,mask_name)) {
            mask=rect.bounds; mask_available=1;
        }
        if (!strcmp(e.id,name)) { *out=rect; return 0; }
    }
    return ENOENT;
}
static int fits(int64_t n) { return n>=INT32_MIN && n<=INT32_MAX; }
int oa_widget_row_item_window(const struct oa_binsvg *v, const struct oa_text_rect *client,
                              const char *name, struct oa_text_rect *out)
{
    if (!client || !out) return EINVAL;
    int64_t width=(int64_t)client->right-client->left;
    int64_t height=(int64_t)client->bottom-client->top;
    if (!fits(width) || !fits(height)) return EOVERFLOW;
    struct oa_text_rect surface={0,0,(int32_t)width,(int32_t)height};
    struct oa_binsvg_rect r;
    int rc=oa_widget_group_rect(v,"rowItems","mRowM",&surface,name,&r);
    if (rc) return rc;
    int64_t left=(int64_t)r.bounds.left+client->left, top=(int64_t)r.bounds.top+client->top;
    int64_t right=(int64_t)r.bounds.right+client->left, bottom=(int64_t)r.bounds.bottom+client->top;
    if (!fits(left) || !fits(top) || !fits(right) || !fits(bottom)) return EOVERFLOW;
    *out=(struct oa_text_rect){(int32_t)left,(int32_t)top,(int32_t)right,(int32_t)bottom};
    return 0;
}
