#include "settings_input_fixture.h"
#include <errno.h>
#include <stdio.h>
struct dispatch_fixture {unsigned count;enum oa_settings_route route;const void *message;uint32_t result;};
static uint32_t dispatch_run(void *p,enum oa_settings_route route,const void *message)
{ struct dispatch_fixture *f=p;++f->count;f->route=route;f->message=message;return f->result; }
int main(void)
{
    int event=7,handled=42;struct dispatch_fixture d={0};
    struct oa_settings_dispatch_ops dispatch={&d,dispatch_run,&event};
    assert(!oa_settings_dispatch(&dispatch,0x1004,0x222,&handled) && handled==1 && !d.message && d.route==OA_SD_MUTE);
    assert(!oa_settings_dispatch(&dispatch,0x1004,0x255,&handled) && !handled && d.message==&event && d.route==OA_SD_MENU);
    assert(!oa_settings_dispatch(&dispatch,0x1004,0x2bb,&handled) && d.route==OA_SD_BASE);
    assert(!oa_settings_dispatch(&dispatch,0x11004,0x222,&handled) && d.route==OA_SD_BASE && d.message==&event);
    struct settings_fixture f={0};struct oa_settings_input_ops ops={&f,settings_query,settings_run,&event};
    assert(!oa_settings_input(&ops,OA_SI_UP_DOWN,0x20b,&handled) && handled==1);
    assert(f.count==2 && f.trace[0].action==OA_SI_LIST_KEY && f.trace[0].message==&event && f.trace[1].action==OA_SI_UPDATE_DISPLAY);
    f=(struct settings_fixture){0};f.q[OA_SI_HORIZONTAL]=1;f.q[OA_SI_ACTIVE_COUNT]=2;f.q[OA_SI_MIRROR]=255;
    assert(!oa_settings_input(&ops,OA_SI_RIGHT_LEFT,0x20c,&handled));
    assert(f.count==2 && f.trace[0].action==OA_SI_NAVIGATE && f.trace[0].argument==0x20b && !f.trace[0].message);
    f=(struct settings_fixture){.mutate=1,.selected_after=33,.empty_after=1};
    f.q[OA_SI_ITEM_INDEX]=2;f.q[OA_SI_ITEM_PRESENT]=1;
    assert(!oa_settings_input(&ops,OA_SI_DIGIT,0x205,&handled));
    assert(f.count==5 && f.q[OA_SI_SELECTED_ID]==33 && f.trace[1].action==OA_SI_ASSIGN_DESCRIPTION && f.trace[1].argument==2 && f.trace[2].action==OA_SI_ASSIGN_LABEL && f.trace[2].argument==2 && f.trace[3].action==OA_SI_ADD_MENU_INFO && f.trace[4].action==OA_SI_UPDATE_SETTINGS);
    f=(struct settings_fixture){0};f.q[OA_SI_ITEM_INDEX]=UINT32_MAX;
    assert(!oa_settings_input(&ops,OA_SI_DIGIT,0x205,&handled) && f.count==3 && f.trace[1].action==OA_SI_ADD_MENU_INFO);
    f.count=0;handled=42;ops.run=NULL;
    assert(oa_settings_input(&ops,OA_SI_SELECT,0,&handled)==EINVAL && handled==42 && !f.count);
    ops.run=settings_run;assert(oa_settings_input(&ops,(enum oa_settings_input_kind)-1,0,&handled)==EINVAL && !f.count);
    assert(oa_settings_input(&ops,OA_SI_SELECT,0,NULL)==EINVAL && !f.count);
    assert(oa_settings_dispatch(NULL,0,0,&handled)==EINVAL && handled==42);
    puts("ok settings input: base-map distinctions, list/update order, mirror translation and dynamic description fallback");
    return 0;
}
