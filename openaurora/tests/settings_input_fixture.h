#ifndef OA_SETTINGS_INPUT_FIXTURE_H
#define OA_SETTINGS_INPUT_FIXTURE_H
#include "openaurora/settings_input.h"
#include <assert.h>
struct settings_fixture {
    uint32_t q[OA_SI_TEXT_EMPTY+1],selected_after,empty_after;
    unsigned mutate,count;
    struct {enum oa_settings_action action;uint32_t argument;const void *message;} trace[8];
};
static inline uint32_t settings_query(void *p,enum oa_settings_query query,uint32_t argument)
{
    struct settings_fixture *f=p;
    if(query==OA_SI_ITEM_INDEX)assert(argument==f->q[OA_SI_SELECTED_ID]);
    return f->q[query];
}
static inline void settings_run(void *p,enum oa_settings_action action,uint32_t argument,const void *message)
{
    struct settings_fixture *f=p;assert(f->count<8);
    f->trace[f->count].action=action;f->trace[f->count].argument=argument;f->trace[f->count++].message=message;
    if(f->mutate && action==OA_SI_LIST_KEY)f->q[OA_SI_SELECTED_ID]=f->selected_after;
    if(f->mutate && action==OA_SI_ASSIGN_DESCRIPTION)f->q[OA_SI_TEXT_EMPTY]=f->empty_after;
}
#endif
