#include "openaurora/tab_policy.h"
#include <errno.h>
int oa_tab_available(const struct oa_tab_context *c,int32_t tab,uint8_t *out)
{
    if(!c || !out)return EINVAL;
    uint8_t available=1;
    switch(tab) {
    case 0:
        if(c->hotel_active)available=(uint8_t)(c->hotel_picture_flag^1u);
        break;
    case 2:
        if(!c->network_profile_flag){available=0;break;}
        /* fall through */
    case 3:
        if(c->hotel_active && c->hotel_osd_mode!=2){available=0;break;}
        /* fall through */
    case 4:
        available=!c->in_media_browser && c->from_launcher?(uint8_t)(c->in_app^1u):0;
        break;
    default:break;
    }
    *out=available;return 0;
}
int oa_tab_available_mask(const struct oa_tab_context *c,uint8_t *out)
{
    if(!c || !out)return EINVAL;
    uint8_t mask=0;
    for(int32_t i=0;i<5;++i) {
        uint8_t available;int rc=oa_tab_available(c,i,&available);if(rc)return rc;
        if(available)mask=(uint8_t)(mask|(1u<<(unsigned)i));
    }
    *out=mask;return 0;
}
int oa_tab_requests_content_focus(uint8_t open_submenu,int32_t item)
{ return open_submenu!=0 || item>0; }
int oa_tab_initial_selection(uint8_t available,uint8_t described,int32_t requested,
                             struct oa_tab_selection *out)
{
    if(!out || (available&0xe0u) || (described&0xe0u))return EINVAL;
    struct oa_tab_selection next={.created_mask=(uint8_t)(available&described),
                                  .focused_tab=-1,.content_tab=0};
    for(int32_t tab=0;tab<5;++tab)if(next.created_mask&(1u<<(unsigned)tab)) {
        if(tab==requested)next.selected_index=next.count;
        ++next.count;
    }
    uint8_t index=0;
    for(int32_t tab=0;tab<5;++tab)if(next.created_mask&(1u<<(unsigned)tab)) {
        if(index==next.selected_index) {next.focused_tab=tab;next.content_tab=tab;break;}
        ++index;
    }
    *out=next;return 0;
}
