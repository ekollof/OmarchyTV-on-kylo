#include "settings_draw_fixture.h"
#include <stdio.h>
int main(void)
{
    struct sd_text label={1,0},path={2,0},url={3,0},text={4,0};
    struct oa_setting_value values[]={{31,&text},{17,&url},{UINT32_MAX,&label}};
    struct oa_setting_item item={.id=42,.label=&label,.glyph_path=&path,.image_url=&url,
        .values=values,.value_count=3,.service_logo=UINT32_MAX,.image={UINT32_MAX,NULL},
        .additional={UINT32_MAX,NULL},.minimum=UINT32_MAX,.maximum=INT32_MAX};
    struct oa_setting_item *items[]={&item};struct oa_setting_draw_style style={0};
    unsigned cases=0;int row;
    for(unsigned type=0;type<32;++type)for(unsigned flags=0;flags<16;++flags)for(unsigned n=0;n<4;++n) {
        struct sd_fixture f={.row=&row};struct oa_setting_draw_ops ops=sd_ops(&f);
        style=(struct oa_setting_draw_style){flags&1,(flags>>1)&1,(flags>>2)&1,(flags>>3)&1};
        item.type=type;item.value_count=n;
        assert(!oa_settings_draw_row(&ops,items,1,0,&row,&style));sd_no_leak(&f);
        uint32_t expected=type;
        if(style.progress && type>=9 && type<=11)expected+=14;
        else if(!style.progress && style.mixed && type==9)expected=23;
        assert(item.type==expected && f.trace[0].id==OA_SD_ROW_ID && f.trace[0].a==42);
        ++cases;
    }
    /* Constructor/copy failures unwind temporaries and snapshots; earlier
     * actions/type writeback remain visible. No TV or vendor code involved. */
    for(unsigned type=2;type<=26;++type)for(unsigned fail=1;fail<=7;++fail) {
        struct sd_fixture f={.row=&row,.fail_at=fail};struct oa_setting_draw_ops ops=sd_ops(&f);
        item.type=type;item.value_count=type==10?0:3;style=(struct oa_setting_draw_style){0};
        int e=oa_settings_draw_row(&ops,items,1,0,&row,&style);
        assert(e==0 || e==ENOMEM);sd_no_leak(&f);
        assert(e==(f.allocated>=fail?ENOMEM:0));
    }
    struct sd_fixture f={.row=&row,.copy_error=EIO};struct oa_setting_draw_ops ops=sd_ops(&f);
    item.type=2;
    assert(oa_settings_draw_row(&ops,items,1,0,&row,&style)==EIO);sd_no_leak(&f);
    memset(&f,0,sizeof f);f.row=&row;f.fail_at=1;item.type=10;style.progress=1;
    assert(oa_settings_draw_row(&ops,items,1,0,&row,&style)==ENOMEM && item.type==24);
    assert(f.count==5 && f.trace[1].id==OA_SD_PROGRESS_MODE && f.trace[3].id==OA_SD_TYPE && f.trace[3].a==24);
    sd_no_leak(&f);
    memset(&f,0,sizeof f);f.row=&row;
    assert(!oa_settings_draw_row(&ops,items,0,0,&row,&style) && !f.count);
    assert(!oa_settings_draw_row(&ops,items,1,-1,&row,&style) && !f.count);
    assert(!oa_settings_draw_row(&ops,items,1,1,&row,&style) && !f.count);
    assert(oa_settings_draw_row(&ops,NULL,1,0,&row,&style)==ENODATA && !f.count);
    ops.run=NULL;
    assert(oa_settings_draw_row(&ops,items,1,0,&row,&style)==EINVAL && !f.count);
    printf("ok settings draw: %u type/style/value cases, owned temporaries/failure unwind and bounds\n",cases);
    return 0;
}
