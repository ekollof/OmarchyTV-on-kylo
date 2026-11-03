#include "openaurora/launcher_route.h"
#include <errno.h>

int32_t oa_launcher_operation_result(uint32_t operation)
{
    /* e4ae0c: unsigned bound <=61, ldrsh from14efd84. */
    static const int16_t results[62]={
        71,74,108,110,164,75,91,73,223,77,257,262,263,264,265,266,
        258,259,260,261,267,268,269,270,273,274,126,137,250,131,-1,-1,
        127,128,129,130,-1,-1,105,121,122,124,123,271,272,139,262,265,
        -1,125,127,287,250,251,252,253,243,245,134,135,136,295
    };
    return operation<62?results[operation]:-1;
}
int oa_launcher_settings_plan(int32_t result,struct oa_launcher_plan *out)
{
    if(!out)return EINVAL;
    int tab,deep=1,item=result;
    switch(result) {
    case 0x49:tab=4;deep=0;item=0;break;
    case 0x5b:tab=2;deep=0;break; /* no item write */
    case 0x6c:tab=0;deep=0;item=0;break;
    case 0x6e:tab=1;deep=0;item=0;break;
    case 0x4f:case 0xa2:case 0xa3:case 0xad:case 0xae:case 0xef:
    case 0x121:case 0x129:tab=4;break;
    case 0x50:case 0x55:case 0x5a:case 0x5f:case 0x60:case 0x61:
    case 0x62:case 0x63:case 0x64:case 0xa1:case 0xd2:case 0xd4:
    case 0xe6:tab=3;break;
    case 0x5c:tab=2;break;
    case 0x6d:tab=0;break;
    case 0x6f:tab=1;break;
    /* 4b has portal prework; a6 depends on tuner/runtime state. Neither can
     * be reduced to an unconditional selection plan. */
    default:return ENOTSUP;
    }
    struct oa_launcher_plan plan={0};
    if(deep)plan.actions[plan.count++]=(struct oa_launcher_action){OA_LAUNCHER_OPEN_SUBMENU,1};
    else plan.actions[plan.count++]=(struct oa_launcher_action){OA_LAUNCHER_PUSH_FRONT,0x13};
    plan.actions[plan.count++]=(struct oa_launcher_action){OA_LAUNCHER_SET_TAB,tab};
    if(result!=0x5b)
        plan.actions[plan.count++]=(struct oa_launcher_action){OA_LAUNCHER_SET_ITEM,item};
    if(deep)plan.actions[plan.count++]=(struct oa_launcher_action){OA_LAUNCHER_PUSH_FRONT,0x13};
    *out=plan;return 0;
}
