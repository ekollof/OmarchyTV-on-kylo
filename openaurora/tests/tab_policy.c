#include "openaurora/tab_policy.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    struct oa_tab_context c={0};uint8_t out=0xff;
    assert(!oa_tab_available_mask(&c,&out) && out==3); /* QuickMenu entry */
    c.from_launcher=1;c.network_profile_flag=1;
    assert(!oa_tab_available_mask(&c,&out) && out==31);
    c.network_profile_flag=0;
    assert(!oa_tab_available_mask(&c,&out) && out==27);
    c.network_profile_flag=1;c.hotel_active=1;c.hotel_picture_flag=1;
    assert(!oa_tab_available_mask(&c,&out) && out==18); /* sound/system */
    c.hotel_osd_mode=2;
    assert(!oa_tab_available_mask(&c,&out) && out==30);
    c.in_media_browser=1;
    assert(!oa_tab_available_mask(&c,&out) && out==2);
    c.in_media_browser=0;c.in_app=1;
    assert(!oa_tab_available_mask(&c,&out) && out==2);
    c.in_app=2;c.hotel_picture_flag=255;
    assert(!oa_tab_available(&c,0,&out) && out==254);
    assert(!oa_tab_available(&c,4,&out) && out==3);
    assert(!oa_tab_available(&c,-1,&out) && out==1);
    assert(!oa_tab_available(&c,INT32_MAX,&out) && out==1);
    out=0xa5;assert(oa_tab_available(NULL,0,&out)==EINVAL && out==0xa5);
    assert(oa_tab_available_mask(NULL,&out)==EINVAL && out==0xa5);
    assert(oa_tab_available(&c,0,NULL)==EINVAL && oa_tab_available_mask(&c,NULL)==EINVAL);
    assert(!oa_tab_requests_content_focus(0,INT32_MIN));
    assert(!oa_tab_requests_content_focus(0,-1) && !oa_tab_requests_content_focus(0,0));
    assert(oa_tab_requests_content_focus(0,1) && oa_tab_requests_content_focus(0,INT32_MAX));
    assert(oa_tab_requests_content_focus(1,0) && oa_tab_requests_content_focus(255,-1));
    struct oa_tab_selection selected;
    assert(!oa_tab_initial_selection(31,31,4,&selected));
    assert(selected.count==5 && selected.selected_index==4 && selected.focused_tab==4);
    assert(!oa_tab_initial_selection(30,31,0,&selected));
    assert(selected.count==4 && selected.selected_index==0 && selected.focused_tab==1);
    assert(!oa_tab_initial_selection(31,18,4,&selected));
    assert(selected.created_mask==18 && selected.count==2 && selected.selected_index==1 && selected.content_tab==4);
    assert(!oa_tab_initial_selection(31,18,2,&selected));
    assert(selected.selected_index==0 && selected.focused_tab==1 && selected.content_tab==1);
    assert(!oa_tab_initial_selection(0,31,4,&selected));
    assert(!selected.count && !selected.selected_index && selected.focused_tab==-1 && selected.content_tab==0);
    struct oa_tab_selection saved=selected;
    assert(oa_tab_initial_selection(32,31,0,&selected)==EINVAL && !memcmp(&selected,&saved,sizeof saved));
    assert(oa_tab_initial_selection(31,128,0,&selected)==EINVAL && !memcmp(&selected,&saved,sizeof saved));
    assert(oa_tab_initial_selection(31,31,0,NULL)==EINVAL);
    struct oa_tab_profile profile={.connectivity_raw=1,.hotel_raw=1};
    struct oa_tab_hotel hotel={.mode_available=255,.picture_flag=1,.osd_menu_mode=2};
    c=(struct oa_tab_context){.from_launcher=1,.in_app=2,.in_media_browser=3};
    assert(!oa_tab_apply_settings(&c,&profile,&hotel));
    assert(c.network_profile_flag==1 && c.hotel_active==1 && c.hotel_picture_flag==1 && c.hotel_osd_mode==2);
    assert(c.from_launcher==1 && c.in_app==2 && c.in_media_browser==3);
    profile.hotel_raw=2;profile.connectivity_raw=UINT32_MAX;
    assert(!oa_tab_apply_settings(&c,&profile,&hotel) && !c.hotel_active && !c.network_profile_flag);
    profile.hotel_raw=1;hotel.mode_available=0;
    assert(!oa_tab_apply_settings(&c,&profile,&hotel) && !c.hotel_active);
    struct oa_tab_context previous=c;
    assert(oa_tab_apply_settings(&c,NULL,&hotel)==EINVAL && !memcmp(&c,&previous,sizeof c));
    assert(oa_tab_apply_settings(&c,&profile,NULL)==EINVAL && !memcmp(&c,&previous,sizeof c));
    assert(oa_tab_apply_settings(NULL,&profile,&hotel)==EINVAL);
    puts("ok tab policy: availability/raw bytes, initial selection/fallback/empty group, requested focus and atomic invalid inputs");
    return 0;
}
