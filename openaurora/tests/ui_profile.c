#include "host.h"
#include "openaurora/events.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static const char *profile_path;
static const oa_ui_profile_iface *removing;
static unsigned removed_checks;
static const char *config(void *p,const char *key,const char *def)
{ (void)p;return !strcmp(key,"ui_profile.swprofile")?profile_path:def; }
static int command(oa_host *h,const char *text)
{ char line[80],reply[256];snprintf(line,sizeof line,"%s",text);return oa_control_command(h,line,reply,sizeof reply); }
static const oa_ui_profile_iface *service(oa_host *h)
{ const oa_ui_profile_iface *s=h->api.lookup(h,OA_SVC_UI_PROFILE,1);assert(s);return s; }
static struct oa_tab_profile snapshot(oa_host *h)
{ struct oa_tab_profile p;assert(!service(h)->copy(&p));return p; }
static void removed(const void *ev,void *ud)
{
    (void)ud;const oa_service_removed_event *e=ev;
    if(strcmp(e->name,OA_SVC_UI_PROFILE) || !removing)return;
    struct oa_tab_profile p={0},saved=p;
    assert(removing->copy(&p)==ENODEV && !memcmp(&p,&saved,sizeof p));
    removing=NULL;++removed_checks;
}
static void put(unsigned char *p,uint32_t n)
{ for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(n>>(8*i)); }
static void record(unsigned char *b,size_t p,uint32_t hash,unsigned kind,uint32_t n,uint32_t value)
{ put(b+p,hash);b[p+4]=(unsigned char)kind;put(b+p+8,n);put(b+p+12,value); }
static void fixture(const char *path,uint32_t connectivity,uint32_t hotel)
{
    unsigned char b[168]={0};memcpy(b,"EFSL",4);put(b+0x38,5);
    record(b,0x44,UINT32_C(0xe6e0d5c2),1,4,0); /* Config */
    record(b,0x58,UINT32_C(0xcbed0877),1,1,0); /* Connectivity */
    record(b,0x6c,UINT32_C(0xcbed0877),5,0,connectivity);
    record(b,0x80,UINT32_C(0x643f8ec3),1,1,0); /* HotelTv */
    record(b,0x94,UINT32_C(0xa5317464),5,0,hotel); /* HotelMode */
    FILE *f=fopen(path,"wb");assert(f && fwrite(b,1,sizeof b,f)==sizeof b && !fclose(f));
}
int main(int argc,char **argv)
{
    assert(argc==2 || argc==3);
    oa_host *h=calloc(1,sizeof *h);assert(h && !oa_host_init(h,argv[1],NULL));
    h->log_level=-1;h->api.config_get=config;
    uint32_t subscription=h->api.subscribe(h,OA_TOPIC_SERVICE_REMOVED,removed,NULL);assert(subscription);
    oa_mod *m=oa_module_load(h,"ui_profile");assert(m);
    if(argc==3) {
        profile_path=argv[2];assert(!oa_module_start(h,m));
        struct oa_tab_profile p=snapshot(h);
        assert(p.connectivity_raw==1 && p.hotel_raw==1 && p.connectivity_present && p.hotel_present);
        assert(!command(h,"stop ui_profile"));puts("ok ui.profile: saved software profile through real DSO");
    }
    char dir[]="/tmp/oa-profile-module.XXXXXX";assert(mkdtemp(dir));
    char path[256],fifo[256],large[256];
    snprintf(path,sizeof path,"%s/profile",dir);snprintf(fifo,sizeof fifo,"%s/fifo",dir);
    snprintf(large,sizeof large,"%s/large",dir);assert(!mkfifo(fifo,0600));
    FILE *f=fopen(large,"wb");assert(f && !ftruncate(fileno(f),16*1024*1024+1) && !fclose(f));
    fixture(path,1,0);profile_path=path;assert(!command(h,"start ui_profile"));
    struct oa_tab_profile copied=snapshot(h);assert(copied.connectivity_enabled && !copied.hotel_enabled);
    assert(service(h)->copy(NULL)==EINVAL);
    fixture(path,2,1);assert(snapshot(h).connectivity_raw==1);
    uint32_t old=m->id;removing=service(h);assert(!command(h,"reload ui_profile") && removed_checks==1);
    m=oa_host_find_module(h,"ui_profile");assert(m && m->id!=old);
    struct oa_tab_profile p=snapshot(h);assert(p.connectivity_raw==2 && !p.connectivity_enabled && p.hotel_enabled);
    assert(copied.connectivity_raw==1 && !copied.hotel_enabled);
    puts("ok ui.profile: immutable copied facts, exact-one values and CLI reload snapshot replacement");
    oa_mod *consumer=oa_module_load(h,"ticker");assert(consumer);
    h->cur_module=consumer->id;assert(h->api.lookup(h,OA_SVC_UI_PROFILE,1));h->cur_module=0;
    assert(command(h,"stop ui_profile")==-1 && command(h,"unload ui_profile")==-1 && command(h,"reload ui_profile")==-1);
    h->cur_module=consumer->id;h->api.release(h,OA_SVC_UI_PROFILE);h->cur_module=0;
    assert(!oa_module_unload(h,"ticker",0));
    removing=service(h);assert(!command(h,"stop ui_profile") && removed_checks==2);
    int peer=42;assert(!h->api.publish(h,OA_SVC_UI_PROFILE,1,&peer));
    assert(command(h,"start ui_profile")==-1 && h->api.lookup(h,OA_SVC_UI_PROFILE,1)==&peer);
    assert(!h->api.unpublish(h,OA_SVC_UI_PROFILE) && !command(h,"start ui_profile"));
    puts("ok ui.profile: consumer holds, removal callback unavailability, publication collision and retry");
    assert(!oa_module_suspend_all(h));fixture(path,0,2);assert(snapshot(h).connectivity_raw==2);
    assert(!oa_module_resume_all(h) && snapshot(h).connectivity_raw==2);
    assert(!oa_module_suspend_all(h) && !command(h,"reload ui_profile"));
    m=oa_host_find_module(h,"ui_profile");assert(m && m->state==OA_ST_SUSPENDED && !oa_host_service_exists(h,OA_SVC_UI_PROFILE));
    profile_path=fifo;assert(oa_module_resume_all(h)==-1 && m->state==OA_ST_SUSPENDED);
    profile_path=path;assert(!oa_module_resume_all(h));p=snapshot(h);
    assert(!p.connectivity_enabled && !p.hotel_enabled && p.hotel_raw==2);
    assert(!command(h,"stop ui_profile"));
    puts("ok ui.profile: standby keeps facts, suspended reload defers I/O, failed resume activation retries");
    const char *bad[]={NULL,"","/nonexistent/oa-profile",dir,fifo,large};
    for(size_t i=0;i<sizeof bad/sizeof *bad;++i) {
        profile_path=bad[i];assert(oa_module_start(h,m)==-1 && !oa_host_service_exists(h,OA_SVC_UI_PROFILE));
    }
    f=fopen(path,"wb");assert(f && fputc('X',f)!=EOF && !fclose(f));profile_path=path;
    assert(oa_module_start(h,m)==-1 && !oa_host_service_exists(h,OA_SVC_UI_PROFILE));
    fixture(path,1,1);assert(!oa_module_start(h,m));
    assert(!unlink(path));assert(snapshot(h).connectivity_enabled && snapshot(h).hotel_enabled);
    assert(!command(h,"stop ui_profile") && !oa_module_unload(h,"ui_profile",0));
    assert(!unlink(fifo) && !unlink(large) && !rmdir(dir));
    puts("ok ui.profile: invalid/special/oversized/malformed inputs reject, service never reads deleted file");
    h->api.unsubscribe(h,subscription);
    close(h->selfpipe[0]);close(h->selfpipe[1]);pthread_mutex_destroy(&h->post_mu);free(h);return 0;
}
