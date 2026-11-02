#include "openaurora/font_box.h"
#include "openaurora/text_origin.h"
#include <errno.h>
#include <limits.h>

static int style(const struct oa_bitmap_font *font,int width,int height,unsigned alignment,
                  int spacing,struct oa_font_line_style *out,struct oa_font_box_layout *result)
{
    if (width<=0 || height<=0) return EINVAL;
    int font_height;
    int rc=oa_bitmap_font_height(font,&font_height);
    if (rc) return rc;
    if (font_height<=0) return EINVAL;
    struct oa_font_box_layout box={0};
    if ((rc=oa_text_box_justification(alignment,&box.horizontal,&box.vertical))) return rc;
    /* TextRenderer1132ab8 reserves the global font X offset on the leading
     * edge (or trailing edge for end alignment). At unit ratio,1134790's
     * logical/physical offset conversions cancel for center/end. Start
     * retains the offset as the initial LineObj X. */
    int offset;
    if ((rc=oa_bitmap_font_x_offset(font,&offset))) return rc;
    int left=box.horizontal==2?0:offset;
    int64_t right=box.horizontal==2?(int64_t)width-offset:width;
    if (right<INT_MIN || right>INT_MAX) return EOVERFLOW;
    if (right<left) return ENOTSUP;
    struct oa_text_vertical positioning={
        .top=0,.bottom=height,.font_height=font_height,.line_step=height,.line_count=1,
        .justification=box.vertical,.clip_mode=3
    };
    struct oa_text_lines lines;
    if ((rc=oa_text_vertical_position(&positioning,&lines))) return rc;
    /* EF41 logical Y offset0, unit ratio and zero surface reference. */
    if ((rc=oa_text_line_y(&lines,0,0,0,1,1,&box.y))) return rc;
    *out=(struct oa_font_line_style){.x=left,.y=box.y,.left=left,.right=(int)right,
                                    .spacing=spacing,.alignment=box.horizontal};
    *result=box;
    return 0;
}
int oa_font_text_layout_box(const struct oa_bitmap_font *font,const char *text,
                            int width,int height,unsigned alignment,int spacing,
                            struct oa_font_box_layout *out)
{
    if (!out) return EINVAL;
    struct oa_font_line_style line;
    struct oa_font_box_layout box;
    int rc=style(font,width,height,alignment,spacing,&line,&box);
    if (!rc) rc=oa_font_text_layout_line(font,text,&line,&box.line);
    if (!rc) *out=box;
    return rc;
}
int oa_font_text_draw_box(oa_canvas *canvas,const struct oa_bitmap_font *font,const char *text,
                          unsigned alignment,int spacing,const uint8_t rgba[4],
                          struct oa_font_box_layout *out)
{
    if (!canvas) return EINVAL;
    struct oa_font_line_style line;
    struct oa_font_box_layout box;
    int rc=style(font,canvas->width,canvas->height,alignment,spacing,&line,&box);
    if (!rc) rc=oa_font_text_draw_line(canvas,font,text,&line,rgba,NULL,&box.line);
    if (!rc && out) *out=box;
    return rc;
}
