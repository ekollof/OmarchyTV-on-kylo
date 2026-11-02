#include "openaurora/text_layout.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    /* SettingListBoxUFA477: exact untransformed resource slice coordinates. */
    const struct oa_text_rect top={225,153,235,169}, left={235,176,235,246};
    const struct oa_text_rect mid={244,178,319,248}, right={328,178,329,248};
    const struct oa_text_rect bottom={228,251,228,261};
    const struct oa_list_slices slices={&top,&left,&mid,&right,&bottom};
    struct oa_list_metrics m={.window={0,0,750,826},.row_height=99,.item_width=88,.left=7,.right=8};
    assert(!oa_list_measure_slices(&m,&slices,0));
    assert(m.top==16 && m.bottom==10 && m.vertical_step==70 && m.horizontal_step==0);
    assert(m.row_height==99 && m.item_width==88 && m.left==7 && m.right==8);
    int32_t count=123;
    assert(!oa_list_update_visible_count(&m,&count) && count==11);
    assert(!oa_list_measure_slices(&m,&slices,1) && m.top==86);
    assert(!oa_list_update_visible_count(&m,&count) && count==10);
    m.horizontal=1;
    assert(!oa_list_measure_slices(&m,&slices,0) && m.horizontal_step==76);
    assert(!oa_list_update_visible_count(&m,&count) && count==9);
    /* Transformed rowPos in the middle surface, not the raw75px resource. */
    const struct oa_text_rect middle_surface={0,16,749,86}, painted={2,16,747,86};
    assert(!oa_list_measure_painted_row(&m,&painted,&middle_surface,&left,&right));
    assert(m.row_height==70 && m.item_width==745 && m.left==2 && m.right==3);
    struct oa_list_metrics saved=m;
    assert(!oa_list_measure_painted_row(&m,NULL,NULL,NULL,NULL) && !memcmp(&m,&saved,sizeof m));
    assert(oa_list_measure_painted_row(&m,&painted,NULL,NULL,NULL)==EINVAL);
    const struct oa_text_rect next={5,86,740,156};
    assert(!oa_list_measure_painted_row(&m,&next,&middle_surface,NULL,NULL));
    assert(m.left==5 && m.right==9 && m.item_width==735); /* last-painted state wins */
    const struct oa_text_rect narrowed={100,16,650,86};
    assert(!oa_list_measure_painted_row(&m,&narrowed,&middle_surface,NULL,NULL));
    assert(count==9); /* count was computed before painting, not changed by it */
    assert(!oa_list_update_visible_count(&m,&count) && count==7); /* next paint */
    const struct oa_list_slices missing={0};
    assert(!oa_list_measure_slices(&m,&missing,0));
    assert(m.top==0 && m.bottom==0 && m.vertical_step==0 && m.horizontal_step==0);
    assert(!oa_list_update_visible_count(&m,&count) && count==7); /* stale count retained */
    const struct oa_text_rect huge={INT32_MIN,0,INT32_MAX,1};
    saved=m;
    assert(oa_list_measure_painted_row(&m,&huge,&middle_surface,NULL,NULL)==EOVERFLOW);
    assert(!memcmp(&m,&saved,sizeof m));
    const struct oa_text_rect huge_height={0,INT32_MIN,1,INT32_MAX};
    const struct oa_list_slices invalid_slices={.top_left=&huge_height};
    assert(oa_list_measure_slices(&m,&invalid_slices,0)==EOVERFLOW);
    assert(!memcmp(&m,&saved,sizeof m));
    puts("ok list metrics: theme slices, horizontal/title count, painted state, absence, bounds");
    return 0;
}
