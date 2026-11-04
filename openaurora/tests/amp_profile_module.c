#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "host.h"
#include "openaurora/events.h"
#include "openaurora/profile.h"
static const char *source;
static unsigned fd_count(void)
{
    DIR *dir=opendir("/proc/self/fd");assert(dir);unsigned count=0;struct dirent *entry;
    while((entry=readdir(dir)))if(entry->d_name[0]!='.')++count;
    assert(!closedir(dir));return count;
}
static const char *config(void *p,const char *key,const char *def)
{ (void)p;return !strcmp(key,"amp_profile.hwprofile")?source:def; }
static void put(unsigned char *p,uint32_t n)
{ for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(n>>(8*i)); }
static void write_profile(const char *path,unsigned value)
{
    unsigned char b[168]={0};memcpy(b,"EFSL",4);put(b+0x38,5);
    const char *names[]={"AudioSettings","Prescales","DefaultSpeaker","12Watt","Amp_Prescale"};
    const unsigned types[]={1,1,5,1,5},counts[]={4,3,0,1,0};
    for(unsigned i=0;i<5;++i) {
        unsigned char *r=b+0x44+i*20;put(r,oa_profile_hash(names[i]));r[4]=(unsigned char)types[i];
        put(r+8,counts[i]);put(r+12,i==2?3:i==4?value:0);
    }
    FILE *f=fopen(path,"wb");assert(f && fwrite(b,1,sizeof b,f)==sizeof b && !fclose(f));
}
static int command(oa_host *h,const char *text,char out[512])
{ char line[128];snprintf(line,sizeof line,"%s",text);return oa_control_command(h,line,out,512); }
static const oa_audio_amp_profile_iface *service(oa_host *h)
{ const void *s=h->api.lookup(h,OA_SVC_AUDIO_AMP_PROFILE,1);assert(s);return s; }
static void value(const oa_audio_amp_profile_iface *s,unsigned expected)
{
    oa_ad82587_profile p;char power[32];assert(!s->copy(&p,power));
    assert(p.speaker_power==power && !strcmp(power,"12Watt") && p.prescale==expected);
    assert(p.channel1==0xc0 && p.speaker_drc==UINT32_MAX && p.state_control4==-1);
}
static const oa_audio_amp_profile_iface *removing;
static unsigned notifications;
static void removed(const void *ev,void *p)
{
    (void)p;const oa_service_removed_event *e=ev;
    if(!removing || strcmp(e->name,OA_SVC_AUDIO_AMP_PROFILE))return;
    oa_ad82587_profile out,saved;memset(&out,0xa5,sizeof out);memcpy(&saved,&out,sizeof out);
    char power[32],saved_power[32];memset(power,0xa5,32);memcpy(saved_power,power,32);
    assert(removing->copy(&out,power)==-ENODEV);
    assert(!memcmp(&out,&saved,sizeof out) && !memcmp(power,saved_power,32));++notifications;
}
int main(int argc,char **argv)
{
    assert(argc==2);
    char dir[]="/tmp/oa-amp-provider.XXXXXX";assert(mkdtemp(dir));
    char file[256],fifo[256];snprintf(file,sizeof file,"%s/profile",dir);snprintf(fifo,sizeof fifo,"%s/fifo",dir);
    assert(!mkfifo(fifo,0600));write_profile(file,17);source=file;
    oa_host *h=calloc(1,sizeof *h);assert(h && !oa_host_init(h,argv[1],NULL));
    h->log_level=-1;h->api.config_get=config;
    oa_mod *m=oa_module_load(h,"audio_amp_profile");assert(m && !oa_module_start(h,m));
    const oa_audio_amp_profile_iface *s=service(h);value(s,17);
    oa_ad82587_profile copy;char name[32];assert(!s->copy(&copy,name));
    assert(s->copy(NULL,name)==-EINVAL && s->copy(&copy,NULL)==-EINVAL);
    name[0]='X';value(s,17);name[0]='1';write_profile(file,31);value(s,17);
    char reply[512];assert(!command(h,"amp-profile",reply) && strstr(reply,"power '12Watt' prescale 17"));
    assert(command(h,"amp-profile extra",reply)==-1 && strstr(reply,"errno 22"));
    assert(command(h,"amp-profile --force",reply)==-1);
    uint32_t old=m->id;assert(!command(h,"reload audio_amp_profile",reply));
    m=oa_host_find_module(h,"audio_amp_profile");assert(m && m->id!=old);s=service(h);value(s,31);
    assert(copy.prescale==17 && copy.speaker_power==name && !strcmp(name,"12Watt"));
    puts("ok amp provider: DSO/query/reload, file snapshot and caller-owned name/value lifetime");

    oa_mod *consumer=oa_module_load(h,"ticker");assert(consumer);
    h->cur_module=consumer->id;assert(h->api.lookup(h,OA_SVC_AUDIO_AMP_PROFILE,1)==s);h->cur_module=0;
    assert(command(h,"stop audio_amp_profile",reply)==-1);
    assert(command(h,"unload audio_amp_profile",reply)==-1);
    assert(command(h,"reload audio_amp_profile",reply)==-1);value(s,31);
    h->cur_module=consumer->id;h->api.release(h,OA_SVC_AUDIO_AMP_PROFILE);h->cur_module=0;
    assert(!oa_module_unload(h,"ticker",0));
    uint32_t sub=h->api.subscribe(h,OA_TOPIC_SERVICE_REMOVED,removed,NULL);assert(sub);
    removing=s;assert(!command(h,"stop audio_amp_profile",reply));assert(notifications==1);removing=NULL;
    assert(!h->api.lookup(h,OA_SVC_AUDIO_AMP_PROFILE,1));
    assert(command(h,"amp-profile",reply)==-1 && strstr(reply,"errno 19"));
    puts("ok amp provider: holds block lifecycle and removal callbacks cannot read a stopped snapshot");

    int peer=1;assert(!h->api.publish(h,OA_SVC_AUDIO_AMP_PROFILE,1,&peer));
    assert(oa_module_start(h,m)==-1 && h->api.lookup(h,OA_SVC_AUDIO_AMP_PROFILE,1)==&peer);
    assert(!h->api.unpublish(h,OA_SVC_AUDIO_AMP_PROFILE));assert(!oa_module_start(h,m));s=service(h);value(s,31);
    assert(!oa_module_stop(h,m));
    unsigned descriptors=fd_count();
    const char *bad[]={NULL,"","/no-such-oa-profile",dir,fifo};
    for(unsigned i=0;i<5;++i){source=bad[i];assert(oa_module_start(h,m)==-1 && !oa_host_service_exists(h,OA_SVC_AUDIO_AMP_PROFILE));}
    source=file;int fd=open(file,O_WRONLY|O_TRUNC);assert(fd>=0);
    assert(!ftruncate(fd,17*1024*1024) && !close(fd));assert(oa_module_start(h,m)==-1);
    FILE *f=fopen(file,"wb");assert(f && fputs("broken",f)>=0 && !fclose(f));assert(oa_module_start(h,m)==-1);
    unsigned char malformed[168]={0};f=fopen(file,"wb");
    assert(f && fwrite(malformed,1,sizeof malformed,f)==sizeof malformed && !fclose(f));
    assert(oa_module_start(h,m)==-1); /* regular/full-size file reaches the decoder */
    assert(fd_count()==descriptors);
    write_profile(file,47);assert(!oa_module_start(h,m));s=service(h);value(s,47);
    puts("ok amp provider: publication collision, invalid/FIFO/oversized/malformed files and retry cleanup");

    assert(!oa_module_suspend_all(h));write_profile(file,63);value(s,47);
    assert(!oa_module_resume_all(h));value(s,47);
    assert(!oa_module_suspend_all(h));assert(!command(h,"reload audio_amp_profile",reply));
    m=oa_host_find_module(h,"audio_amp_profile");assert(m && m->state==OA_ST_SUSPENDED);
    assert(!oa_host_service_exists(h,OA_SVC_AUDIO_AMP_PROFILE));
    source=fifo;assert(oa_module_resume_all(h)==-1 && m->state==OA_ST_SUSPENDED);
    source=file;assert(!oa_module_resume_all(h));s=service(h);value(s,63);
    assert(!unlink(file));value(s,63);
    assert(command(h,"reload audio_amp_profile",reply)==-1);
    assert(!oa_host_find_module(h,"audio_amp_profile") && !oa_host_service_exists(h,OA_SVC_AUDIO_AMP_PROFILE));
    write_profile(file,79);m=oa_module_load(h,"audio_amp_profile");assert(m && !oa_module_start(h,m));
    value(service(h),79);assert(!oa_module_unload(h,"audio_amp_profile",0));
    puts("ok amp provider: suspend snapshot, deferred reload/resume retry and failed replacement recovery");
    h->api.unsubscribe(h,sub);close(h->selfpipe[0]);close(h->selfpipe[1]);assert(!pthread_mutex_destroy(&h->post_mu));free(h);
    assert(!unlink(file) && !unlink(fifo) && !rmdir(dir));return 0;
}
