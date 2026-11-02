/* Policy/lifetime test, not a visual acceptance: real theme/engine/vendor DSOs,
 * recorder display and injected animation progress. Never opens TV devices. */
#include "host.h"
#include "openaurora/events.h"
#include "openaurora/ui_engine.h"
#include "ui_nav_trace.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *index_path, *binary_path, *override="";
static oa_anim_fn callback;
static void *callback_ud;
static int starts;
static int enable(int on) { (void)on; return 0; }
static int enabled(void) { return 1; }
static int start(int id,int kind,const oa_anim_rect *a,const oa_anim_rect *b,uint32_t ms,uint32_t *at)
{ (void)id;(void)a;(void)b;(void)at;assert(kind==OA_ANIM_DIALOG && ms==1000);++starts;return 0; }
static int cancel(int id) { (void)id; return 0; }
static int bind(oa_anim_fn fn,void *ud) { callback=fn;callback_ud=ud;return 0; }
static const oa_ui_anim_iface anim={.set_enabled=enable,.enabled=enabled,.start=start,.cancel=cancel,.bind=bind};
static const char *config(void *priv,const char *key,const char *def)
{
    (void)priv;
    if (!strcmp(key,"ui_theme.index")) return index_path;
    if (!strcmp(key,"ui_theme.binary")) return binary_path;
    if (!strcmp(key,"ui.fs_type")) return override;
    if (!strcmp(key,"ui.launch_anim")) return "1";
    if (!strcmp(key,"ui.paint") || !strcmp(key,"ui.rail_hl")) return "0";
    if (!strcmp(key,"ui.dir")) return "/nonexistent/oa-theme-policy-assets";
    if (!strcmp(key,"ui.cards")) return "card";
    return def;
}
static void fixture(const char *index,const char *binary,unsigned mask,int correct_name)
{
    static const char *const names[]={"animation_flip_fade_in","animation_slide_right_fade_in",
        "animation_slide_left_fade_in","animation_slide_up_fade_in","animation_slide_down_fade_in",
        "animation_fade_in","animation_grow_fade_in","animation_bounce_fade_in"};
    FILE *f=fopen(binary,"wb"); assert(f);
    /* Reverse stream order: numeric mode priority must win. Background keeps
     * the zero-marker record nonempty without inventing animation semantics. */
    for (int i=8;i>=0;--i) if (i==8 || (mask&(1u<<(unsigned)i))) {
        const char *name=i==8?"background":names[i];
        uint8_t payload[17]={0}; payload[10]=10;
        assert(fputc(1,f)!=EOF && fwrite(name,1,strlen(name)+1,f)==strlen(name)+1);
        assert(fwrite(payload,1,sizeof payload,f)==sizeof payload);
    }
    long size=ftell(f); assert(size>0 && !fclose(f));
    f=fopen(index,"w"); assert(f);
    assert(fprintf(f,"1\n0x%08x %ld\n",correct_name?0x5c4ba740u:0u,size)>0 && !fclose(f));
}
static int command(oa_host *h,const char *text)
{ char line[100],reply[256];snprintf(line,sizeof line,"%s",text);return oa_control_command(h,line,reply,sizeof reply); }
static void start_module(oa_host *h,const char *name)
{ oa_mod *m=oa_module_load(h,name);assert(m && !oa_module_start(h,m)); }
static void drain_rebind(oa_host *h)
{
    oa_mod *m=oa_host_find_module(h,"ui_vendor"); assert(m);
    for (unsigned round=0;round<4;++round) {
        for (unsigned i=0;i<MAX_TIMERS;++i) {
            oa_timer *t=&h->timers[i];
            if (!t->active || t->owner!=m->id || t->repeat_ms) continue;
            void (*cb)(void *)=t->cb;void *ud=t->ud;t->active=0;
            uint32_t prev=h->cur_module;h->cur_module=m->id;cb(ud);h->cur_module=prev;
        }
        oa_host_dispatch_posts(h);
    }
}
static void key(oa_host *h,int id)
{
    oa_key_event ev={.id=(uint8_t)id,.kind=OA_KEY_PRESS};
    h->api.emit(h,OA_TOPIC_KEY,&ev); oa_host_dispatch_posts(h);
}
static void progress(oa_host *h,int done)
{
    oa_mod *m=oa_host_find_module(h,"ui_vendor"); assert(m && callback);
    uint32_t prev=h->cur_module;h->cur_module=m->id;
    callback(1,OA_ANIM_DIALOG,NULL,done?1.0f:0.5f,done,callback_ud);h->cur_module=prev;
}
static void expect(oa_host *h,int type)
{
    key(h,10); /* return to portal, clear previous transform */
    const oa_portal_iface *p=h->api.lookup(h,OA_SVC_PORTAL,1);assert(p && p->activate_at(220,490));
    int before=starts;
    key(h,53);
    assert(starts==before+(type!=0));
    if (type) {
        progress(h,0);
        const oa_nav_trace *trace=h->api.lookup(h,OA_SVC_NAV_TRACE,1);assert(trace);
        int actual;float amount;assert(!trace->transform(&actual,&amount,NULL));
        assert(actual==type && amount==0.5f);
    }
}
int main(int argc,char **argv)
{
    assert(argc==2 || argc==4);
    char dir[]="/tmp/oa-theme-skin.XXXXXX";assert(mkdtemp(dir));
    char index[256],binary[256];snprintf(index,sizeof index,"%s/index",dir);snprintf(binary,sizeof binary,"%s/binary",dir);
    fixture(index,binary,(1u<<7)|(1u<<1),1);index_path=index;binary_path=binary;
    oa_host *h=calloc(1,sizeof *h);assert(h && !oa_host_init(h,argv[1],NULL));
    h->log_level=-1;h->api.config_get=config;
    assert(!h->api.publish(h,OA_SVC_UI_ANIM,2,&anim));
    start_module(h,"nav_display");start_module(h,"ui_engine");start_module(h,"ui_vendor");
    expect(h,4); /* no theme: retain the previously verified stock fallback */
    start_module(h,"ui_theme");drain_rebind(h);expect(h,2);
    assert(command(h,"reload ui_theme")==-1); /* actual ui_vendor lookup hold */
    assert(!command(h,"unload ui_theme --force"));
    progress(h,0); /* still type2: active animation owns a scalar, no view */
    const oa_nav_trace *trace=h->api.lookup(h,OA_SVC_NAV_TRACE,1);int type;
    assert(trace && !trace->transform(&type,NULL,NULL) && type==2);
    drain_rebind(h);expect(h,4);
    fixture(index,binary,1,1);start_module(h,"ui_theme");drain_rebind(h);expect(h,1);
    puts("  ok   skin discovers late theme, pins provider, drops forced removal, preserves active mode and rebinds");
    for (unsigned i=0;i<8;++i) {
        assert(!command(h,"stop ui_vendor"));fixture(index,binary,1u<<i,1);
        assert(!command(h,"reload ui_theme") && !command(h,"start ui_vendor"));expect(h,(int)i+1);
    }
    override="7";expect(h,7);override="0";expect(h,0);override="9";expect(h,0);override="";
    for (unsigned missing=0;missing<2;++missing) {
        assert(!command(h,"stop ui_vendor"));fixture(index,binary,missing?255:0,!missing);
        assert(!command(h,"reload ui_theme") && !command(h,"start ui_vendor"));expect(h,0);
    }
    puts("  ok   all8 modes, numeric priority over stream order, missing resource/markers and explicit overrides");
    if (argc==4) {
        assert(!command(h,"stop ui_vendor"));index_path=argv[2];binary_path=argv[3];
        assert(!command(h,"reload ui_theme") && !command(h,"start ui_vendor"));expect(h,4);
        puts("  ok   shipped DialogLauncher.svg selects actual marker mode60/type4 through provider and skin");
    }
    assert(!oa_module_suspend_all(h));
    assert(!command(h,"unload ui_theme --force"));
    index_path=index;binary_path=binary;fixture(index,binary,1u<<6,1);
    start_module(h,"ui_theme"); /* host defers activation while suspended */
    assert(!oa_module_resume_all(h));drain_rebind(h);expect(h,7);
    assert(!command(h,"stop ui_vendor"));
    int peer=1;assert(!h->api.publish(h,OA_SVC_PORTAL,1,&peer));
    assert(command(h,"start ui_vendor")==-1 && !callback);
    assert(h->api.lookup(h,OA_SVC_PORTAL,1)==&peer);
    assert(!command(h,"unload ui_theme")); /* failed start released new hold */
    assert(!h->api.unpublish(h,OA_SVC_PORTAL));
    assert(!command(h,"unload ui_vendor") && !callback);
    assert(!command(h,"unload ui_engine") && !command(h,"unload nav_display"));
    assert(!h->api.unpublish(h,OA_SVC_UI_ANIM));
    assert(!unlink(index) && !unlink(binary) && !rmdir(dir));
    close(h->selfpipe[0]);close(h->selfpipe[1]);pthread_mutex_destroy(&h->post_mu);free(h);
    puts("  ok   suspended replacement defers I/O/rebind; resume reacquires; failed skin start releases theme and callback without removing peer");
    return 0;
}
