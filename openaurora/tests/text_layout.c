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
    const struct oa_text_rect surface={100,200,500,800}, mask={10,20,70,100};
    const struct oa_text_rect positioned[] = {
        {103,204,113,224}, {103,204,113,224}, {443,204,453,224},
        {103,204,113,224}, {103,724,113,744}, {103,204,113,224},
        {443,204,453,224}, {103,724,113,744}, {443,724,453,744},
        {3,4,13,24}
    };
    for (unsigned a=0;a<10;++a) {
        struct oa_text_rect actual={13,24,23,44};
        assert(!oa_text_rect_position_mask(&actual,&surface,&mask,a));
        assert(!memcmp(&actual,&positioned[a],sizeof actual));
    }
    struct oa_text_rect r={9,3,49,43};
    struct oa_list_metrics list={
        .window={100,200,500,800}, .vertical_step=70,.row_height=66,
        .top=13,.bottom=17,.left=11,.right=19,.horizontal_step=90,.item_width=80
    };
    assert(!oa_list_row_rect(&list,2,&r));
    assert(!memcmp(&r,&(struct oa_text_rect){11,153,381,219},sizeof r));
    list.horizontal=1;
    assert(!oa_list_row_rect(&list,2,&r));
    assert(!memcmp(&r,&(struct oa_text_rect){191,13,271,583},sizeof r));
    list.mirrored=1;
    assert(!oa_list_row_rect(&list,2,&r));
    assert(!memcmp(&r,&(struct oa_text_rect){219,13,299,583},sizeof r));
    assert(!oa_list_row_rect(&list,-1,&r) && r.left==489 && r.right==569);
    struct oa_text_rect row_saved=r;
    assert(oa_list_row_rect(&list,INT32_MAX,&r)==EOVERFLOW && !memcmp(&r,&row_saved,sizeof r));
    assert(oa_list_row_rect(NULL,0,&r)==EINVAL);
    r=(struct oa_text_rect){9,3,49,43};
    struct oa_text_rect saved=r;
    assert(!oa_text_rect_position_mask(&r,NULL,NULL,0) && !memcmp(&r,&saved,sizeof r));
    assert(oa_text_rect_position_mask(&r,NULL,&mask,0)==EINVAL && !memcmp(&r,&saved,sizeof r));
    r=(struct oa_text_rect){INT32_MAX,3,INT32_MAX,43}; saved=r;
    assert(oa_text_rect_position_mask(&r,&surface,&mask,0)==EOVERFLOW && !memcmp(&r,&saved,sizeof r));
    r=(struct oa_text_rect){9,3,49,43};
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
    puts("ok text layout: mask alignments, vertical/horizontal/mirrored rows, truncation, bounds");
    return 0;
}
