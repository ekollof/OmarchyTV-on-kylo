#ifndef OA_TAB_LEAF_FIXTURE_H
#define OA_TAB_LEAF_FIXTURE_H
#include "openaurora/tab_leaf.h"
#include <assert.h>
struct leaf_fixture {
    uint32_t q[OA_TL_DIGITS+1];unsigned count;
    struct {enum oa_tab_leaf_action action;uint32_t argument;const void *message;} trace[4];
};
static inline uint32_t leaf_query(void *context,enum oa_tab_leaf_query query)
{ return ((struct leaf_fixture *)context)->q[query]; }
static inline void leaf_run(void *context,enum oa_tab_leaf_action action,uint32_t argument,const void *message)
{
    struct leaf_fixture *f=context;assert(f->count<4);
    f->trace[f->count].action=action;f->trace[f->count].argument=argument;f->trace[f->count++].message=message;
    if(action==OA_TL_SET_DIGITS)f->q[OA_TL_DIGITS]=argument;
}
#endif
