#include "openaurora/widget_layout.h"
#include <errno.h>
#include <limits.h>
#include <string.h>

static int group_rect(const struct oa_binsvg *v, const char *group,
                         const char *mask_name, const struct oa_text_rect *surface,
                         const char *name, struct oa_binsvg_rect *out, int *used_mask)
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
        if (!strcmp(e.id,name)) {
            if (used_mask) *used_mask=mask_available;
            *out=rect; return 0;
        }
    }
    return ENOENT;
}
int oa_widget_group_rect(const struct oa_binsvg *v, const char *group,
                         const char *mask_name, const struct oa_text_rect *surface,
                         const char *name, struct oa_binsvg_rect *out)
{ return group_rect(v,group,mask_name,surface,name,out,NULL); }
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

static int optional_rect(const struct oa_binsvg *v, const char *group, const char *name,
                          struct oa_binsvg_rect *r, int *found)
{
    int rc=oa_binsvg_find_rect(v,group,name,r);
    if (rc && rc!=ENOENT) return rc;
    *found=rc==0;
    return 0;
}
int oa_widget_single_line_textbox(const struct oa_binsvg *v, const struct oa_text_rect *window,
                                  struct oa_widget_textbox *out)
{
    if (!v || !window || !out) return EINVAL;
    int64_t width=(int64_t)window->right-window->left;
    int64_t height=(int64_t)window->bottom-window->top;
    if (!fits(width) || !fits(height)) return EOVERFLOW;
    struct oa_text_rect surface={0,0,(int32_t)width,(int32_t)height}, middle;
    struct oa_binsvg_rect left={0},right={0},mask,marker;
    int has_left,has_right;
    int rc=optional_rect(v,"gRowL","mRowL",&left,&has_left);
    if (rc) return rc;
    if ((rc=optional_rect(v,"gRowR","mRowR",&right,&has_right))) return rc;
    if ((rc=oa_binsvg_find_rect(v,"gRowM","mRowM",&mask))) return rc;
    if ((rc=oa_horizontal_middle_surface(&surface,has_left?&left.bounds:NULL,
                                         has_right?&right.bounds:NULL,&middle))) return rc;
    struct oa_widget_textbox result={.alignment=4,.font_type=0};
    int used_mask;
    if ((rc=group_rect(v,"gRowM","mRowM",&middle,"textPos",&result.text,&used_mask))) return rc;
    /* A textPos preceding its mask needs the complete second-pass/cache state
     * machine. Do not silently treat that different layout like the shipped
     * mask-first textbox styles supported here. */
    if (!used_mask) return ENOTSUP;
    int64_t mask_height=(int64_t)mask.bounds.bottom-mask.bounds.top;
    if (!fits(mask_height)) return EOVERFLOW;
    if ((rc=oa_text_rect_scale_height(&result.text.bounds,(int32_t)height,(int32_t)mask_height))) return rc;
    /* SVGTextBox10c5ffc..10c621a: left marker wins, then right, then any
     * marker found by the full-record description pass. */
    int found;
    if ((rc=optional_rect(v,"gRowL","alignment",&marker,&found))) return rc;
    if (!found) {
        if ((rc=optional_rect(v,"gRowR","alignment",&marker,&found))) return rc;
        if (found) result.alignment=6;
        else {
            if ((rc=optional_rect(v,NULL,"alignment",&marker,&found))) return rc;
            if (found) result.alignment=5;
        }
    }
    static const char *const fonts[]={"fontRegular","fontBold","fontLight",
        "fontItalic","fontBoldItalic","fontLightItalic"};
    for (unsigned i=0;i<sizeof fonts/sizeof *fonts;++i) {
        if ((rc=optional_rect(v,NULL,fonts[i],&marker,&found))) return rc;
        if (found) { result.font_type=i;break; }
    }
    struct oa_text_rect *r=&result.text.bounds;
    int64_t l=(int64_t)r->left+window->left,t=(int64_t)r->top+window->top;
    int64_t right_edge=(int64_t)r->right+window->left,b=(int64_t)r->bottom+window->top;
    if (!fits(l) || !fits(t) || !fits(right_edge) || !fits(b)) return EOVERFLOW;
    *r=(struct oa_text_rect){(int32_t)l,(int32_t)t,(int32_t)right_edge,(int32_t)b};
    *out=result;
    return 0;
}
