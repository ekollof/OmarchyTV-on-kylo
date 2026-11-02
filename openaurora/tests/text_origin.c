#include "openaurora/text_origin.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    struct oa_text_vertical p={10,100,12,20,2,1,0};
    struct oa_text_lines out;
    assert(!oa_text_vertical_position(&p,&out) && out.first==10 && out.step==20);
    p.justification=2;
    assert(!oa_text_vertical_position(&p,&out) && out.first==68);
    p.justification=3;
    assert(!oa_text_vertical_position(&p,&out) && out.first==39);
    int32_t y;
    assert(!oa_text_line_y(&out,0,1,0,3,2,&y) && y==59);
    assert(!oa_text_line_y(&out,1,1,0,3,2,&y) && y==89);
    p.justification=4;
    assert(!oa_text_vertical_position(&p,&out) && out.first==10 && out.step==78);
    p=(struct oa_text_vertical){10,60,10,20,5,3,0};
    assert(!oa_text_vertical_position(&p,&out) && out.first==10);
    p.clip_mode=2;
    assert(!oa_text_vertical_position(&p,&out) && out.first==-10);
    p=(struct oa_text_vertical){0,5,10,20,1,3,2};
    assert(!oa_text_vertical_position(&p,&out) && out.first==-2); /* C center truncation */
    p=(struct oa_text_vertical){10,60,10,0,5,3,0};
    assert(!oa_text_vertical_position(&p,&out) && out.first==30 && !out.step);
    out=(struct oa_text_lines){1,1};
    assert(!oa_text_line_y(&out,0,1,0,1,2,&y) && y==1); /* reference phase matters */
    out.first=-1;
    assert(!oa_text_line_y(&out,0,0,0,1,2,&y) && y==-1); /* true floor, not truncation */
    assert(!oa_text_line_y(&out,0,-1,0,1,2,&y) && y==0);
    assert(!oa_text_line_y(&out,0,0,3,1,1,&y) && y==2);
    struct oa_text_lines before=out;
    p.justification=99;
    assert(oa_text_vertical_position(&p,&out)==ENOTSUP && !memcmp(&out,&before,sizeof out));
    p.justification=1; p.line_count=0;
    assert(oa_text_vertical_position(&p,&out)==EINVAL && !memcmp(&out,&before,sizeof out));
    p=(struct oa_text_vertical){0,1,INT32_MAX,INT32_MAX,INT32_MAX,2,0};
    assert(oa_text_vertical_position(&p,&out)==EOVERFLOW && !memcmp(&out,&before,sizeof out));
    y=123;
    assert(oa_text_line_y(&out,0,0,0,1,0,&y)==EINVAL && y==123);
    out=(struct oa_text_lines){INT32_MAX,1};
    assert(oa_text_line_y(&out,1,0,0,1,1,&y)==EOVERFLOW && y==123);
    assert(oa_text_line_y(&out,0,1,0,1,1,&y)==EOVERFLOW && y==123);
    assert(oa_text_line_y(&out,0,0,0,2,1,&y)==EOVERFLOW && y==123);
    puts("ok text origin: vertical policies, clipping capacity, ratio phase/floor, zero step, bounds");
    return 0;
}
