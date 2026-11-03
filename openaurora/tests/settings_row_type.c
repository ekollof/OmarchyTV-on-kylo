#include "openaurora/settings_row_type.h"
#include "openaurora/settings_model.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static const uint32_t offsets[]={0x3c4,0x104,0x684,0x13c0,0x1498,0x1ca8,0x1014,0x18d8,0x27a8,0x2cf0,0x2b28,0x2c0c,0x1570,0x1724,0x24e8,0x1f68,0x2228};
struct row {
    struct oa_setting_row_type_state state;
    int visible,force;unsigned rendered,queried,calls;
    int child[17];uint32_t order[17];
};
static void render(void *p)
{
    struct row *r=p;assert(r->state.rebuild==1);++r->rendered;
    if(r->force>=0)r->visible=r->force;
}
static int visible(void *p) { struct row *r=p;++r->queried;return r->visible; }
static void child(void *p,enum oa_setting_child_kind kind,uint32_t offset,int value)
{
    struct row *r=p;assert(r->calls<17 && r->queried && (value==0 || value==1));
    assert(kind==(offset==0x1014?OA_SETTING_SELECTION:offset==0x18d8?OA_SETTING_TEXT_ENTRY:offset==0x27a8?OA_SETTING_CHECKBOX:OA_SETTING_WINDOW));
    r->order[r->calls++]=offset;
    for(unsigned i=0;i<17;++i)if(offsets[i]==offset) { r->child[i]=value;return; }
    assert(0);
}
static struct oa_setting_row_type_ops ops(struct row *r)
{ return (struct oa_setting_row_type_ops){r,render,visible,child}; }
static void reset(struct row *r)
{ memset(r,0,sizeof *r);r->state.type=UINT32_MAX;r->visible=1;r->force=-1;for(unsigned i=0;i<17;++i)r->child[i]=7; }
static void sink(void *p,void *handle,const struct oa_setting_draw_event *e)
{
    struct row *r=p;assert(handle==r);
    if(e->action==OA_SD_TYPE) { struct oa_setting_row_type_ops o=ops(r);assert(!oa_setting_row_set_type(&r->state,&o,e->a)); }
}
int main(void)
{
    struct row r;reset(&r);struct oa_setting_row_type_ops o=ops(&r);
    assert(!oa_setting_row_set_type(&r.state,&o,24));
    assert(r.state.type==24 && r.state.rebuild==1 && r.rendered==1 && r.queried==1 && r.calls==16);
    /* Type24 skips value textbox+1ca8 entirely; preserve existing state. */
    assert(r.child[5]==7 && r.child[13]==1 && r.child[14]==1);
    assert(r.order[5]==0x1f68 && r.order[6]==0x2228 && r.order[7]==0x18d8 && r.order[8]==0x1014);
    r.calls=0;
    assert(!oa_setting_row_set_type(&r.state,&o,24) && !r.state.rebuild && r.rendered==1 && r.queried==2);
    reset(&r);
    assert(!oa_setting_row_set_type(&r.state,&o,27) && r.calls==2);
    assert(!r.child[0] && r.child[1]==1);
    for(unsigned i=2;i<17;++i)assert(r.child[i]==7);
    reset(&r);r.visible=0;r.force=1;
    assert(!oa_setting_row_set_type(&r.state,&o,1) && r.calls==17 && r.child[0]==1 && !r.child[1]);
    reset(&r);r.force=0;
    assert(!oa_setting_row_set_type(&r.state,&o,9) && r.state.type==9 && r.state.rebuild && !r.calls && r.queried==1);
    reset(&r);o.set_visible=NULL;
    assert(oa_setting_row_set_type(&r.state,&o,1)==EINVAL && r.state.type==UINT32_MAX && !r.rendered && !r.queried);
    o=ops(&r);assert(oa_setting_row_set_type(NULL,&o,1)==EINVAL);
    assert(oa_setting_row_set_type(&r.state,NULL,1)==EINVAL);
    /* Actual owned model -> verified parent dispatcher -> real cached row type
     * and visibility fields. Other row setters/rendering stay outside this test. */
    struct oa_settings_item_input item={.id=7,.type=9,.label="Volume",.glyph_path="",.image_url="",
        .image={UINT32_MAX,NULL,0},.additional={UINT32_MAX,NULL,0}};
    struct oa_settings_model *model=NULL;assert(!oa_settings_model_create(&item,1,"OK",NULL,&model));
    reset(&r);struct oa_setting_draw_style style={.progress=1};
    assert(!oa_settings_model_draw(model,0,&r,&style,sink,&r));
    assert(oa_settings_model_item(model,0)->type==23 && r.state.type==23 && r.child[12]==1 && r.child[14]==1 && !r.child[3]);
    assert(!oa_settings_model_destroy(model));
    puts("ok settings row type: ordered visibility, omitted child, unchanged/hidden/invalid gates and owned-model join");
    return 0;
}
