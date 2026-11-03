#include "openaurora/tab_policy.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
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
    puts("ok tab policy: entry/hotel/profile/app gates, raw byte XOR semantics, default IDs, requested focus and atomic invalid inputs");
    return 0;
}
