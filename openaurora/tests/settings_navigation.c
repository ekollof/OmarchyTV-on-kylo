#include "settings_navigation_fixture.h"
#include <errno.h>
#include <stdio.h>
int main(void)
{
    struct nav_fixture f=nav_initial();struct oa_settings_nav_ops ops={&f,nav_query,nav_run};
    assert(!oa_settings_navigate(&ops,0x20b) && f.count_queries==1 && !f.actions);
    f=nav_initial();f.count=3;f.cursor=2;f.biggest=2;f.rows=3;
    assert(!oa_settings_navigate(&ops,0x20b) && f.cursor==0 && f.row==0 && f.actions==2 && f.trace[0].action==OA_SN_SET_CURSOR && f.trace[1].action==OA_SN_SET_ROW);
    f=nav_initial();f.count=3;f.biggest=2;f.rows=3;f.mutate=1;f.indices[1]=8;
    assert(!oa_settings_navigate(&ops,0x20b) && f.cursor==1 && f.item==6 && f.row==2 && f.rows==1 && f.actions==3);
    f=nav_initial();f.count=3;f.biggest=2;f.rows=3;
    assert(!oa_settings_navigate(&ops,0x20a) && f.cursor==2 && f.row==2);
    f.actions=0;assert(!oa_settings_navigate(&ops,0x2dc) && f.actions==1 && f.trace[0].action==OA_SN_SET_ROW);
    f=nav_initial();f.count=3;f.cursor=UINT32_MAX;f.rows=3;f.biggest=2;
    assert(!oa_settings_navigate(&ops,0x20b) && f.cursor==0 && f.row==0);
    f=nav_initial();f.count=3;f.cursor=3;
    assert(oa_settings_navigate(&ops,0)==ERANGE && !f.actions);
    assert(oa_settings_navigate(&ops,0x20b)==ERANGE && f.actions==1 && f.cursor==4 && f.trace[0].action==OA_SN_SET_CURSOR);
    f=nav_initial();f.count=0x20000000;
    assert(oa_settings_navigate(&ops,0)==EOVERFLOW && f.count_queries==1 && !f.actions);
    f=nav_initial();ops.run=NULL;
    assert(oa_settings_navigate(&ops,0)==EINVAL && !f.count_queries && !f.actions);
    ops.run=nav_run;ops.query=NULL;assert(oa_settings_navigate(&ops,0)==EINVAL && !f.count_queries);
    assert(oa_settings_navigate(NULL,0)==EINVAL);
    puts("ok settings navigation: active-ID cursor wrap/jumps, viewport updates, captured geometry and invalid-read bounds");
    return 0;
}
