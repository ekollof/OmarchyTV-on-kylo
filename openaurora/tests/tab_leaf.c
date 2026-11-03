#include "tab_leaf_fixture.h"
#include <errno.h>
#include <stdio.h>
int main(void)
{
    struct leaf_fixture f={0};int event=0,handled=42;
    struct oa_tab_leaf_ops ops={&f,leaf_query,leaf_run,&event};
    assert(oa_tab_leaf(&ops,OA_TL_VOLUME,0x220,&handled)==ENODATA && handled==42 && !f.count);
    f.q[OA_TL_CONTENT_PRESENT]=1;
    assert(!oa_tab_leaf(&ops,OA_TL_VOLUME,0x220,&handled) && handled==1 && !f.count);
    f.q[OA_TL_CONTENT_SHOWING]=255;
    assert(!oa_tab_leaf(&ops,OA_TL_VOLUME,0x220,&handled) && f.count==1 && f.trace[0].message==&event && f.trace[0].argument==0x220);
    f=(struct leaf_fixture){0};
    const char *code="1104";
    for(unsigned i=0;i<4;++i) { f.count=0;assert(!oa_tab_leaf(&ops,OA_TL_DIGIT,0x200+(unsigned)(code[i]-'0'),&handled)); }
    assert(f.q[OA_TL_DIGITS]==1104 && f.count==2 && f.trace[0].action==OA_TL_SET_DIGITS && f.trace[1].action==OA_TL_SVG_END_DIALOG && f.trace[1].argument==0xd1);
    f.q[OA_TL_DIGITS]=472;f.count=0;
    assert(!oa_tab_leaf(&ops,OA_TL_DIGIT,0x205,&handled) && f.count==2 && f.trace[1].argument==0xb5);
    f.q[OA_TL_DIGITS]=UINT32_MAX;f.count=0;
    assert(!oa_tab_leaf(&ops,OA_TL_DIGIT,0x209,&handled) && f.q[OA_TL_DIGITS]==UINT32_MAX && f.count==1);
    f.count=0;assert(!oa_tab_leaf(&ops,OA_TL_MENU,0x22d,&handled));
    assert(f.count==2 && f.trace[0].action==OA_TL_RESET_PICTURE_INDEX && f.trace[1].action==OA_TL_END_DIALOG && !f.trace[1].argument);
    f.count=0;assert(!oa_tab_leaf(&ops,OA_TL_MENU,0x210,&handled));
    assert(f.count==1 && f.trace[0].argument==0x29a);
    f.count=0;assert(!oa_tab_leaf(&ops,OA_TL_QUIT,0x28b,&handled));
    assert(f.count==2 && f.trace[0].action==OA_TL_END_DIALOG && f.trace[1].action==OA_TL_PUSH_KEY && f.trace[1].argument==0x28b && !f.trace[1].message);
    f.count=0;assert(!oa_tab_leaf(&ops,OA_TL_GUIDE,0x230,&handled));
    assert(f.count==2 && f.trace[1].action==OA_TL_PUSH_NAVIGATION && f.trace[1].argument==0xb);
    f.count=0;assert(!oa_tab_leaf(&ops,OA_TL_REMOVED,0x2a7,&handled) && !handled && !f.count);
    f.q[OA_TL_IN_MEDIA]=255;
    assert(!oa_tab_leaf(&ops,OA_TL_REMOVED,0,&handled) && handled==1 && f.count==2 && f.trace[1].action==OA_TL_PUSH_KEY && f.trace[1].argument==0x2a7);
    f.count=0;handled=42;
    assert(oa_tab_leaf(&ops,(enum oa_tab_leaf_kind)-1,0,&handled)==EINVAL && handled==42 && !f.count);
    assert(oa_tab_leaf(NULL,OA_TL_MENU,0,&handled)==EINVAL && handled==42);
    ops.run=NULL;assert(oa_tab_leaf(&ops,OA_TL_MENU,0,&handled)==EINVAL && handled==42 && !f.count);
    ops.run=leaf_run;ops.query=NULL;assert(oa_tab_leaf(&ops,OA_TL_MENU,0,&handled)==EINVAL && !f.count);
    ops.query=leaf_query;assert(oa_tab_leaf(&ops,OA_TL_MENU,0,NULL)==EINVAL && !f.count);
    puts("ok tab leaves: showing-gated volume, signed rolling digits, menu/exit distinctions and owned repost ordering");
    return 0;
}
