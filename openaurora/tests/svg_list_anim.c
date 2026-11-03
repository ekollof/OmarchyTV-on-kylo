#include "svg_list_anim_fixture.h"
#include <errno.h>
#include <stdio.h>
static void reset(struct anim_fixture *f,void *list,void *manager,void *row)
{
    *f=(struct anim_fixture){.enabled=1,.has_row=1,.owner=0x99aa,.stdname="M",
        .nanims=1,.names={"xM"},.durs={"2.5"},.present={1},.list=list,.manager=manager,.row=row};
    f->rect[3]=1;
}
int main(void)
{
    unsigned char list[0x400],manager[8],row[8];struct anim_fixture f;
    reset(&f,list,manager,row);struct oa_svg_list_anim_ops ops=anim_ops(&f);float started=-1;
    assert(!oa_svg_list_start_animation(&ops,list,manager,&started) && started==2500 && f.started);
    af_clean(&f);
    unsigned parsed=96,free_sub=96,free_name=96,start=96;
    for(unsigned i=0;i<f.count;++i) {
        if(f.trace[i].id==A_PARSE)parsed=i;
        if(f.trace[i].id==A_FREE && f.trace[i].a==2)free_sub=i;
        if(f.trace[i].id==A_FREE && f.trace[i].a==1)free_name=i;
        if(f.trace[i].id==A_START)start=i;
    }
    assert(parsed<free_sub && free_sub<free_name && free_name<start);
    /* Codepoint prefix, not first byte. Input remains borrowed and untouched. */
    reset(&f,list,manager,row);f.names[0]="éM";
    assert(!oa_svg_list_start_animation(&ops,list,manager,&started) && f.started && started==2500);af_clean(&f);
    /* Empty name's wrapped length-1 must not read beyond its allocation. */
    reset(&f,list,manager,row);f.names[0]="";started=-1;
    assert(!oa_svg_list_start_animation(&ops,list,manager,&started) && !f.started && started==-1);af_clean(&f);
    reset(&f,list,manager,row);f.names[0]="";f.stdname="";
    assert(!oa_svg_list_start_animation(&ops,list,manager,&started) && f.started);af_clean(&f);
    /* Negative raw container size is a signed-loop early exit. */
    reset(&f,list,manager,row);f.nanims=UINT32_MAX;started=-1;
    assert(!oa_svg_list_start_animation(&ops,list,manager,&started) && !f.started && started==-1);af_clean(&f);
    for(unsigned fail=1;fail<=3;++fail) {
        reset(&f,list,manager,row);f.fail_on=fail;started=-1;
        assert(oa_svg_list_start_animation(&ops,list,manager,&started)==ENOMEM && !f.started && started==-1);af_clean(&f);
    }
    reset(&f,list,manager,row);f.names[0]=NULL;started=-1;
    assert(!oa_svg_list_start_animation(&ops,list,manager,&started) && !f.started && started==-1);af_clean(&f);
    reset(&f,list,manager,row);f.durs[0]=NULL;
    assert(oa_svg_list_start_animation(&ops,list,manager,&started)==ENODATA && started==-1);af_clean(&f);
    reset(&f,list,manager,row);f.missing_controller=1;
    assert(oa_svg_list_start_animation(&ops,list,manager,&started)==ENODATA && started==-1);af_clean(&f);
    reset(&f,list,manager,row);f.enabled=0;
    assert(!oa_svg_list_start_animation(&ops,list,manager,&started) && f.count==1 && started==-1);
    reset(&f,list,manager,row);f.has_row=0;
    assert(!oa_svg_list_start_animation(&ops,list,manager,&started) && f.count==3 && started==-1);
    reset(&f,list,manager,row);memcpy(f.cur,f.rect,16);
    assert(!oa_svg_list_start_animation(&ops,list,manager,&started) && f.count==4 && started==-1);
    reset(&f,list,manager,row);ops.parse_duration=NULL;
    assert(oa_svg_list_start_animation(&ops,list,manager,&started)==EINVAL && !f.count && started==-1);
    assert(oa_svg_list_start_animation(NULL,list,manager,&started)==EINVAL);
    ops=anim_ops(&f);assert(oa_svg_list_start_animation(&ops,list,manager,NULL)==EINVAL && !f.count);
    puts("ok animation review: owned/context-bound strings, UTF-8/empty bounds, parse/free order, signed size and cleanup");
    return 0;
}
