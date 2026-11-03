#include "openaurora/svg_list_input.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
struct fixture {uint32_t fields[3],result;unsigned count;enum oa_svg_list_action action;const void *message;};
static uint32_t query(void *p,enum oa_svg_list_query field) { return ((struct fixture *)p)->fields[field]; }
static uint32_t run(void *p,enum oa_svg_list_action action,const void *message)
{ struct fixture *f=p;f->action=action;f->message=message;++f->count;return f->result; }
struct row_fixture {uint32_t values[5];unsigned count;enum oa_settings_row_query query;};
static uint32_t row_query(void *p,enum oa_settings_row_query field)
{ struct row_fixture *f=p;f->query=field;++f->count;return f->values[field]; }
int main(void)
{
    int event=7,handled=42;struct fixture f={.fields={0,0x20e,0x20e}};
    struct oa_svg_list_ops ops={&f,query,run,&event};
    assert(!oa_svg_list_key(&ops,0x20e,&handled) && !handled && f.action==OA_SVG_LIST_SELECT && !f.message);
    f.fields[OA_SVG_LIST_PAGE_UP_KEY]=0x20b;f.result=255;
    assert(!oa_svg_list_key(&ops,0x20b,&handled) && handled==1 && f.action==OA_SVG_LIST_DOWN);
    f.fields[OA_SVG_LIST_HORIZONTAL]=1;f.fields[OA_SVG_LIST_PAGE_DOWN_KEY]=0x20a;
    assert(!oa_svg_list_key(&ops,0x20a,&handled) && f.action==OA_SVG_LIST_PAGE_DOWN);
    f.fields[OA_SVG_LIST_PAGE_UP_KEY]=0x20a;
    assert(!oa_svg_list_key(&ops,0x20a,&handled) && f.action==OA_SVG_LIST_PAGE_DOWN);
    assert(!oa_svg_list_key(&ops,0x2db,&handled) && f.action==OA_SVG_LIST_FIRST && f.message==&event);
    f.fields[OA_SVG_LIST_PAGE_DOWN_KEY]=0x2db;
    assert(!oa_svg_list_key(&ops,0x2db,&handled) && f.action==OA_SVG_LIST_PAGE_DOWN && !f.message);
    f.count=0;
    assert(!oa_svg_list_handle(&ops,0x1004,0x1020e,&handled) && !handled && !f.count);
    assert(!oa_svg_list_handle(&ops,0x11004,0x20e,&handled) && handled==1 && f.action==OA_SVG_LIST_WINDOW && f.message==&event);
    struct row_fixture row={.values={0x10001,UINT32_MAX,255,17,19}};
    struct oa_settings_row_ops rops={&row,row_query};uint32_t value=42;
    assert(!oa_settings_row_value(&rops,2,&value) && value==0x10001 && row.query==OA_ROW_SELECTION_ID);
    assert(!oa_settings_row_value(&rops,13,&value) && value==UINT32_MAX && row.query==OA_ROW_NUMERIC);
    assert(!oa_settings_row_value(&rops,22,&value) && value==255 && row.query==OA_ROW_CHECKED);
    assert(!oa_settings_row_value(&rops,9,&value) && value==17 && row.query==OA_ROW_CACHE_PRIMARY);
    assert(!oa_settings_row_value(&rops,25,&value) && value==19 && row.query==OA_ROW_CACHE_SECONDARY);
    row.count=0;assert(!oa_settings_row_value(&rops,0x10002,&value) && !value && !row.count);
    handled=42;f.count=0;ops.run=NULL;
    assert(oa_svg_list_key(&ops,0,&handled)==EINVAL && handled==42 && !f.count);
    assert(oa_svg_list_handle(&ops,0,0,&handled)==EINVAL && handled==42 && !f.count);
    ops.run=run;assert(oa_svg_list_key(&ops,0,NULL)==EINVAL && !f.count);
    value=42;rops.query=NULL;
    assert(oa_settings_row_value(&rops,0,&value)==EINVAL && value==42 && !row.count);
    assert(oa_settings_row_value(NULL,0,&value)==EINVAL && value==42);
    puts("ok SVG list input: routing precedence/aliases, message operands, no implicit fallback and raw typed row values");
    return 0;
}
