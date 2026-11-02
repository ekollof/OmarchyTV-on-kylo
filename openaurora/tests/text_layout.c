#include "openaurora/text_layout.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#ifdef __arm__
/* Exact instruction arithmetic from the firmware, independent of C casts. */
static int32_t arm_span(int32_t surface, int32_t mask, int32_t span)
{
    float a,b,c,result;
    memcpy(&a,&surface,4); memcpy(&b,&mask,4); memcpy(&c,&span,4);
    __asm__ volatile (
        "vcvt.f32.s32 %0, %0\n\t"
        "vcvt.f32.s32 %1, %1\n\t"
        "vcvt.f32.s32 %2, %2\n\t"
        "vdiv.f32 %3, %0, %1\n\t"
        "vmul.f32 %3, %3, %2\n\t"
        "vcvt.s32.f32 %3, %3"
        : "+t"(a), "+t"(b), "+t"(c), "=&t"(result));
    int32_t out;
    memcpy(&out,&result,4);
    return out;
}
#endif

int main(void)
{
    struct oa_text_rect r={9,3,49,43};
    assert(!oa_text_rect_scale_height(&r,66,44));
    assert(r.left==9 && r.top==3 && r.right==49 && r.bottom==63);
    r=(struct oa_text_rect){-8,10,99,14};
    assert(!oa_text_rect_scale_height(&r,2,3) && r.bottom==12);
    r.bottom=14;
    assert(!oa_text_rect_scale_height(&r,-2,3) && r.bottom==8);
    struct oa_text_rect before=r;
    assert(oa_text_rect_scale_height(&r,1,0)==EDOM && !memcmp(&r,&before,sizeof r));
    r=(struct oa_text_rect){0,INT32_MIN,1,INT32_MAX}; before=r;
    assert(oa_text_rect_scale_height(&r,1,1)==EOVERFLOW && !memcmp(&r,&before,sizeof r));
    r=(struct oa_text_rect){0,INT32_MAX-4,1,INT32_MAX}; before=r;
    assert(oa_text_rect_scale_height(&r,2,1)==EOVERFLOW && !memcmp(&r,&before,sizeof r));
    r=(struct oa_text_rect){0,0,1,INT32_MAX}; before=r;
    assert(oa_text_rect_scale_height(&r,1,1)==EOVERFLOW && !memcmp(&r,&before,sizeof r));
#ifdef __arm__
    unsigned checked=0;
    for (int surface=-73;surface<=217;surface+=7)
        for (int mask=1;mask<=95;mask+=3)
            for (int span=-51;span<=131;span+=5) {
                r=(struct oa_text_rect){9,3,49,3+span};
                assert(!oa_text_rect_scale_height(&r,surface,mask));
                assert(r.bottom-3==arm_span(surface,mask,span));
                ++checked;
            }
    printf("ok text height: %u ARM VFP instruction comparisons\n",checked);
#endif
    puts("ok text height: axis/origin preservation, truncation, bounded failures");
    return 0;
}
