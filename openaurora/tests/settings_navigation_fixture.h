#ifndef OA_SETTINGS_NAVIGATION_FIXTURE_H
#define OA_SETTINGS_NAVIGATION_FIXTURE_H
#include "openaurora/settings_navigation.h"
#include <assert.h>
struct nav_fixture {
    uint32_t count,cursor,ids[8],indices[8],biggest,rows,item,row,mutate;
    unsigned count_queries,actions;
    struct {enum oa_settings_nav_action action;uint32_t argument;} trace[4];
};
static inline uint32_t nav_query(void *p,enum oa_settings_nav_query query,uint32_t argument)
{
    struct nav_fixture *f=p;++f->count_queries;
    switch(query) {
    case OA_SN_COUNT:return f->count;
    case OA_SN_CURSOR:return f->cursor;
    case OA_SN_ACTIVE_ID:assert(argument<f->count && argument<8);return f->ids[argument];
    case OA_SN_ITEM_INDEX:
        for(unsigned i=0;i<8;++i)if(f->ids[i]==argument)return f->indices[i];
        assert(0);return 0;
    case OA_SN_BIGGEST_VISIBLE:return f->biggest;
    case OA_SN_ROWS:return f->rows;
    }
    assert(0);return 0;
}
static inline void nav_run(void *p,enum oa_settings_nav_action action,uint32_t argument)
{
    struct nav_fixture *f=p;assert(f->actions<4);
    f->trace[f->actions].action=action;f->trace[f->actions++].argument=argument;
    switch(action) {
    case OA_SN_SET_CURSOR:f->cursor=argument;break;
    case OA_SN_SET_ITEM:f->item=argument;if(f->mutate) { f->rows=1;f->biggest=UINT32_MAX; }break;
    case OA_SN_SET_ROW:f->row=argument;break;
    }
}
static inline struct nav_fixture nav_initial(void)
{
    struct nav_fixture f={.item=0xdada,.row=0xbebe};
    for(unsigned i=0;i<8;++i){f.ids[i]=257+i*11;f.indices[i]=i;}
    return f;
}
#endif
