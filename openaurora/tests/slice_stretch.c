#include "openaurora/text_layout.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef __arm__
static int32_t arm_scaled(int32_t surface, int32_t mask, int32_t span)
{
    float a,b,c,bits;
    double x,y,z;
    memcpy(&a,&surface,4); memcpy(&b,&mask,4); memcpy(&c,&span,4);
    __asm__ volatile (
        "vcvt.f64.s32 %P0, %4\n\t"
        "vcvt.f64.s32 %P1, %5\n\t"
        "vcvt.f64.s32 %P2, %6\n\t"
        "vdiv.f64 %P0, %P0, %P1\n\t"
        "vmul.f64 %P0, %P2, %P0\n\t"
        "vcvt.s32.f64 %3, %P0"
        : "=&w"(x), "=&w"(y), "=&w"(z), "=&t"(bits)
        : "t"(a), "t"(b), "t"(c));
    int32_t result;
    memcpy(&result,&bits,4);
    return result;
}
#endif

int main(void)
{
    struct oa_horizontal_scale s=OA_HORIZONTAL_SCALE_INITIALIZER;
    assert(s.ratio==1.0);
    const struct oa_text_rect mask={10,20,12,50}, surface={100,200,101,260};
    assert(!oa_horizontal_scale_measure(&s,&surface,&mask) && s.ratio==0.5);
    struct oa_text_rect r={11,17,12,22};
    assert(!oa_text_rect_stretch_mask(&r,&surface,&mask,&s));
    /* Separate truncated relative X/width: independently scaled right=101
     * would be incorrect here. Height5 and relative top-3 are preserved. */
    assert(!memcmp(&r,&(struct oa_text_rect){100,197,100,202},sizeof r));
    const struct oa_text_rect zero_mask={10,20,10,50};
    assert(!oa_horizontal_scale_measure(&s,&surface,&zero_mask) && s.ratio==0.5);
    assert(!oa_horizontal_scale_measure(&s,NULL,NULL) && s.ratio==0.5);
    const struct oa_text_rect zero_surface={100,200,99,260};
    assert(!oa_horizontal_scale_measure(&s,&zero_surface,&mask) && s.ratio==0.0);
    r=(struct oa_text_rect){-100,17,500,22};
    assert(!oa_text_rect_stretch_mask(&r,&zero_surface,&mask,&s) && r.left==100 && r.right==100);
    struct oa_text_rect before=r;
    assert(!oa_text_rect_stretch_mask(&r,NULL,NULL,NULL) && !memcmp(&r,&before,sizeof r));
    s.ratio=NAN;
    assert(oa_text_rect_stretch_mask(&r,&surface,&mask,&s)==EINVAL && !memcmp(&r,&before,sizeof r));
    s.ratio=2147483648.0;
    assert(oa_text_rect_stretch_mask(&r,&surface,&mask,&s)==EOVERFLOW && !memcmp(&r,&before,sizeof r));
    /* Double precision must not lose the low bit as float32 would. */
    s.ratio=1.0; r=(struct oa_text_rect){0,0,16777217,1};
    const struct oa_text_rect origin={0,0,1,1};
    assert(!oa_text_rect_stretch_mask(&r,&origin,&origin,&s) && r.right==16777217);
    /* Compose the real477 raw slices through measure/stretch/painted fields
     * and getRowPosition. No fitted screenshot coordinates supply this path. */
    const struct oa_text_rect tl={225,153,235,169}, ml={235,176,235,246};
    const struct oa_text_rect mm={244,178,319,248}, mr={328,178,329,248};
    const struct oa_text_rect bl={228,251,228,261};
    const struct oa_list_slices slices={&tl,&ml,&mm,&mr,&bl};
    struct oa_list_metrics metrics={.window={0,0,750,826}};
    assert(!oa_list_measure_slices(&metrics,&slices,0));
    const struct oa_text_rect row_surface={0,16,750,86};
    struct oa_text_rect middle;
    assert(!oa_horizontal_middle_surface(&row_surface,&ml,&mr,&middle));
    assert(!memcmp(&middle,&(struct oa_text_rect){0,16,749,86},sizeof middle));
    assert(!oa_horizontal_scale_measure(&s,&middle,&mm));
    r=mm; /* raw rowPos is identical to raw mRowM for477 */
    assert(!oa_text_rect_stretch_mask(&r,&middle,&mm,&s));
    assert(!memcmp(&r,&middle,sizeof r));
    assert(!oa_list_measure_painted_row(&metrics,&r,&middle,&ml,&mr));
    assert(metrics.row_height==70 && metrics.item_width==749 && metrics.left==0 && metrics.right==1);
    assert(!oa_list_row_rect(&metrics,0,&r) && !memcmp(&r,&middle,sizeof r));
#ifdef __arm__
    unsigned checked=0;
    for (int sw=1;sw<=233;sw+=7) for (int mw=1;mw<=91;mw+=3) {
        struct oa_text_rect target={0,0,sw,100}, source={0,0,mw,100};
        assert(!oa_horizontal_scale_measure(&s,&target,&source));
        for (int span=-57;span<=173;span+=5) {
            r=(struct oa_text_rect){span,0,span+7,5};
            assert(!oa_text_rect_stretch_mask(&r,&target,&source,&s));
            assert(r.left==arm_scaled(sw,mw,span));
            assert(r.right-r.left==arm_scaled(sw,mw,7));
            ++checked;
        }
    }
    printf("ok horizontal stretch: %u ARM VFP rectangle comparisons\n",checked);
#endif
    puts("ok horizontal stretch: separate offset/width, double precision, retained ratio, missing mask, bounds");
    return 0;
}
