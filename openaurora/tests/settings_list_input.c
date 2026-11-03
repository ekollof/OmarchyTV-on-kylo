#include "settings_list_fixture.h"
#include <errno.h>
#include <stdio.h>
struct map_fixture {enum oa_settings_list_route route;unsigned count;const void *message;uint32_t result;};
static uint32_t map_run(void *context,enum oa_settings_list_route route,const void *message)
{ struct map_fixture *f=context;f->route=route;++f->count;f->message=message;return f->result; }
int main(void)
{
    int event=7,handled=42;struct map_fixture map={0};struct oa_settings_list_dispatch_ops dispatch={&map,map_run,&event};
    assert(!oa_settings_list_dispatch(&dispatch,0x1004,0x20e,&handled) && !handled && map.route==OA_SL_ROUTE_RIGHT_LEFT && map.message==&event);
    assert(!oa_settings_list_dispatch(&dispatch,0x1004,0x209,&handled) && map.route==OA_SL_ROUTE_DIGIT);
    map.result=255;
    assert(!oa_settings_list_dispatch(&dispatch,0x11004,0x209,&handled) && handled==1 && map.route==OA_SL_ROUTE_BASE);
    assert(!oa_settings_list_dispatch(&dispatch,0x1004,0x10209,&handled) && map.route==OA_SL_ROUTE_BASE);
    struct sl_fixture f=sl_initial();struct oa_settings_list_ops ops={&f,sl_query,sl_run,&event};
    assert(!oa_settings_list_input(&ops,OA_SL_DIGIT,0x200,&handled) && !handled && !f.count);
    f.present=1;
    assert(!oa_settings_list_input(&ops,OA_SL_DIGIT,0x200,&handled) && handled==1 && f.count==1 && f.trace[0].subject==1 && f.trace[0].message==&event);
    f=sl_initial();f.present=1;f.values[0]=5;f.values[2]=9;f.mutate=1;f.post_selected=2;f.value_after=77;
    assert(!oa_settings_list_input(&ops,OA_SL_RIGHT_LEFT,0x20d,&handled) && handled==1);
    assert(f.count==2 && f.trace[0].subject==1 && f.trace[1].action==OA_SL_STORE_VALUE && f.trace[1].subject==1 && f.trace[1].value==77 && !f.trace[1].message && f.selected==2 && f.record_values[0]==77 && f.record_values[2]==0x9002);
    f=sl_initial();f.horizontal=1;f.mirror=255;
    assert(!oa_settings_list_input(&ops,OA_SL_RIGHT_LEFT,0x20c,&handled) && handled==1 && f.count==1 && f.trace[0].action==OA_SL_BASE_KEY && f.trace[0].value==0x20d && !f.trace[0].message);
    f.count=0;assert(!oa_settings_list_input(&ops,OA_SL_RIGHT_LEFT,0x2da,&handled) && f.trace[0].value==0x2da && f.trace[0].message==&event);
    f=sl_initial();f.present=1;f.records[0]=0;handled=42;
    assert(oa_settings_list_input(&ops,OA_SL_RIGHT_LEFT,0x20d,&handled)==ENODATA && handled==42 && f.count==1);
    f.count=0;ops.run=NULL;
    assert(oa_settings_list_input(&ops,OA_SL_DIGIT,0,&handled)==EINVAL && handled==42 && !f.count);
    ops.run=sl_run;assert(oa_settings_list_input(&ops,(enum oa_settings_list_kind)-1,0,&handled)==EINVAL && !f.count);
    assert(oa_settings_list_input(&ops,OA_SL_DIGIT,0,NULL)==EINVAL && !f.count);
    assert(oa_settings_list_dispatch(NULL,0,0,&handled)==EINVAL && handled==42);
    puts("ok settings list: row-presence truth, original-row/new-record writeback, mirrored event identity and missing-record bounds");
    return 0;
}
