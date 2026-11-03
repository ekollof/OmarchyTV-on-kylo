#ifndef OA_SETTINGS_LIST_FIXTURE_H
#define OA_SETTINGS_LIST_FIXTURE_H
#include "openaurora/settings_list_input.h"
#include <assert.h>
struct sl_fixture {
    uint32_t horizontal,mirror,selected,present,post_selected,mutate,result,value_after;
    uint32_t values[3],records[3],record_values[3];unsigned count;
    struct {enum oa_settings_list_action action;uint32_t subject,value;const void *message;} trace[4];
};
static inline uint32_t sl_query(void *context,enum oa_settings_list_query query,uint32_t argument)
{
    struct sl_fixture *f=context;
    switch(query) {
    case OA_SL_HORIZONTAL:return f->horizontal;
    case OA_SL_MIRROR:return f->mirror;
    case OA_SL_SELECTED:return f->selected;
    case OA_SL_ROW:return f->present && argument<3?argument+1:0;
    case OA_SL_RECORD:return argument<3?f->records[argument]:0;
    case OA_SL_ROW_VALUE:assert(argument && argument<=3);return f->values[argument-1];
    }
    assert(0);return 0;
}
static inline uint32_t sl_run(void *context,enum oa_settings_list_action action,uint32_t subject,uint32_t value,const void *message)
{
    struct sl_fixture *f=context;assert(f->count<4);
    f->trace[f->count].action=action;f->trace[f->count].subject=subject;f->trace[f->count].value=value;f->trace[f->count++].message=message;
    if(action==OA_SL_ROW_KEY && f->mutate) {
        assert(subject && subject<=3);f->selected=f->post_selected;f->values[subject-1]=f->value_after;
        f->records[0]=3;f->records[1]=2;f->records[2]=1;
    } else if(action==OA_SL_STORE_VALUE) { assert(subject && subject<=3);f->record_values[subject-1]=value; }
    return f->result;
}
static inline struct sl_fixture sl_initial(void)
{ return (struct sl_fixture){.records={1,2,3},.record_values={0x9000,0x9001,0x9002}}; }
#endif
