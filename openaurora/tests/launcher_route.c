#include "openaurora/launcher_route.h"
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
static void action(const struct oa_launcher_plan *p,size_t i,enum oa_launcher_action_kind kind,int32_t value)
{ assert(i<p->count && p->actions[i].kind==kind && p->actions[i].value==value); }
int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--dump")) {
        for(uint32_t i=0;i<62;++i)printf("id %" PRIu32 " %" PRId32 "\n",i,oa_launcher_operation_result(i));
        for(int32_t i=0;i<512;++i) {
            struct oa_launcher_plan p;
            if(oa_launcher_settings_plan(i,&p))continue;
            printf("plan %" PRId32,i);
            for(size_t j=0;j<p.count;++j)printf(" %d:%" PRId32,(int)p.actions[j].kind,p.actions[j].value);
            putchar('\n');
        }
        return ferror(stdout)?1:0;
    }
    assert(argc==1);
    assert(oa_launcher_operation_result(2)==0x6c && oa_launcher_operation_result(3)==0x6e);
    assert(oa_launcher_operation_result(30)==-1 && oa_launcher_operation_result(61)==295);
    assert(oa_launcher_operation_result(62)==-1 && oa_launcher_operation_result(UINT32_MAX)==-1);
    struct oa_launcher_plan p;
    assert(!oa_launcher_settings_plan(0x6c,&p) && p.count==3);
    action(&p,0,OA_LAUNCHER_PUSH_FRONT,0x13);action(&p,1,OA_LAUNCHER_SET_TAB,0);
    action(&p,2,OA_LAUNCHER_SET_ITEM,0);
    assert(!oa_launcher_settings_plan(0x5b,&p) && p.count==2);
    action(&p,0,OA_LAUNCHER_PUSH_FRONT,0x13);action(&p,1,OA_LAUNCHER_SET_TAB,2);
    assert(!oa_launcher_settings_plan(0x6d,&p) && p.count==4);
    action(&p,0,OA_LAUNCHER_OPEN_SUBMENU,1);action(&p,1,OA_LAUNCHER_SET_TAB,0);
    action(&p,2,OA_LAUNCHER_SET_ITEM,0x6d);action(&p,3,OA_LAUNCHER_PUSH_FRONT,0x13);
    struct oa_launcher_plan saved=p;
    for(int32_t i=-4;i<512;++i) {
        p=saved;int rc=oa_launcher_settings_plan(i,&p);
        if(rc)assert(rc==ENOTSUP && !memcmp(&p,&saved,sizeof p));
        else assert(p.count>=2 && p.count<=4);
    }
    assert(oa_launcher_settings_plan(0x4b,&p)==ENOTSUP);
    assert(oa_launcher_settings_plan(0xa6,&p)==ENOTSUP);
    assert(oa_launcher_settings_plan(0x6c,NULL)==EINVAL);
    puts("ok launcher routing: ID bounds/holes, ordered settings plans, retained fields, atomic unsupported/invalid cases");
    return 0;
}
