#include "host.h"
#include "openaurora/events.h"
#include <sqlite3.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static const char *db_path;
static const char *readonly_mode="1";
static const oa_settings_hotel_iface *removing_hotel;
static const oa_settings_iface *removing_settings;
static unsigned removals;
static const char *config(void *p,const char *key,const char *def)
{ (void)p;return !strcmp(key,"settings.db")?db_path:!strcmp(key,"settings.readonly")?readonly_mode:def; }
static int command(oa_host *h,const char *text)
{ char line[80],reply[256];snprintf(line,sizeof line,"%s",text);return oa_control_command(h,line,reply,sizeof reply); }
static const oa_settings_hotel_iface *hotel(oa_host *h)
{ const oa_settings_hotel_iface *s=h->api.lookup(h,OA_SVC_SETTINGS_HOTEL,1);assert(s);return s; }
static const oa_settings_iface *settings(oa_host *h)
{ const oa_settings_iface *s=h->api.lookup(h,OA_SVC_SETTINGS,1);assert(s);return s; }
static void execute(sqlite3 *db,const char *sql)
{ char *error=NULL;int rc=sqlite3_exec(db,sql,NULL,NULL,&error);if(rc)fprintf(stderr,"SQL fixture: %s\n",error);sqlite3_free(error);assert(rc==SQLITE_OK); }
static void check(oa_host *h,uint8_t mode,uint32_t osd,uint8_t picture)
{ struct oa_tab_hotel state;assert(!hotel(h)->read(&state));assert(state.mode_available==mode && state.osd_menu_mode==osd && state.picture_flag==picture); }
static void failure(oa_host *h,int error)
{ struct oa_tab_hotel state={.osd_menu_mode=42,.mode_available=43,.picture_flag=44},saved=state;assert(hotel(h)->read(&state)==error && !memcmp(&state,&saved,sizeof state)); }
static void removed(const void *ev,void *ud)
{
    (void)ud;const oa_service_removed_event *e=ev;
    if(!strcmp(e->name,OA_SVC_SETTINGS_HOTEL) && removing_hotel) {
        struct oa_tab_hotel state={0},saved=state;
        assert(removing_hotel->read(&state)==ENODEV && !memcmp(&state,&saved,sizeof state));
        removing_hotel=NULL;++removals;
    }
    if(!strcmp(e->name,OA_SVC_SETTINGS) && removing_settings) {
        int value=99;assert(removing_settings->get_int("m_cec_enabled",&value)==-1 && value==99);
        assert(!removing_settings->is_writable());removing_settings=NULL;++removals;
    }
}
int main(int argc,char **argv)
{
    assert(argc==2 || argc==3);oa_host *h=calloc(1,sizeof *h);assert(h && !oa_host_init(h,argv[1],NULL));
    h->log_level=-1;h->api.config_get=config;
    uint32_t sub=h->api.subscribe(h,OA_TOPIC_SERVICE_REMOVED,removed,NULL);assert(sub);
    oa_mod *m=oa_module_load(h,"settings");assert(m);
    if(argc==3) {
        db_path=argv[2];assert(!oa_module_start(h,m));check(h,0,1,0);
        assert(!command(h,"stop settings"));puts("ok settings.hotel: saved firmware database read-only corpus0/1/0");
    }
    char dir[]="/tmp/oa-hotel-settings.XXXXXX";assert(mkdtemp(dir));
    char path[256];snprintf(path,sizeof path,"%s/settings.db",dir);
    sqlite3 *writer=NULL;assert(sqlite3_open(path,&writer)==SQLITE_OK);
    execute(writer,"CREATE TABLE Settings(m_cec_enabled INTEGER); INSERT INTO Settings VALUES(1);"
        "CREATE TABLE HMCSettings(m_hotel_mode_availability INTEGER,m_osd_menu_mode INTEGER,m_pic_mode_availability INTEGER);"
        "INSERT INTO HMCSettings VALUES(0,1,0)");
    db_path=path;assert(!command(h,"start settings"));check(h,0,1,0);
    int value;assert(!settings(h)->get_int("m_cec_enabled",&value) && value==1);
    assert(!settings(h)->is_writable() && settings(h)->set_int("m_cec_enabled",0)==-2);
    assert(hotel(h)->read(NULL)==EINVAL);
    execute(writer,"UPDATE HMCSettings SET m_hotel_mode_availability=-1,m_osd_menu_mode=4294967298,m_pic_mode_availability=258");
    check(h,255,2,2); /* native sqlite3_column_int then byte/int32 stores */
    execute(writer,"UPDATE HMCSettings SET m_hotel_mode_availability=NULL,m_osd_menu_mode='-1junk',m_pic_mode_availability='255junk'");
    check(h,0,UINT32_MAX,255);
    assert(!command(h,"reload settings"));m=oa_host_find_module(h,"settings");assert(m);check(h,0,UINT32_MAX,255);
    execute(writer,"BEGIN EXCLUSIVE");failure(h,EBUSY);execute(writer,"ROLLBACK");
    check(h,0,UINT32_MAX,255);
    puts("ok settings.hotel: readonly companion, SQL integer narrowing/NULL/text, reload and non-waiting busy error");
    oa_mod *consumer=oa_module_load(h,"ticker");assert(consumer);
    h->cur_module=consumer->id;assert(h->api.lookup(h,OA_SVC_SETTINGS_HOTEL,1));h->cur_module=0;
    assert(command(h,"stop settings")==-1 && command(h,"reload settings")==-1);
    h->cur_module=consumer->id;h->api.release(h,OA_SVC_SETTINGS_HOTEL);
    assert(h->api.lookup(h,OA_SVC_SETTINGS,1));h->cur_module=0;
    assert(command(h,"unload settings")==-1);
    h->cur_module=consumer->id;h->api.release(h,OA_SVC_SETTINGS);h->cur_module=0;
    assert(!oa_module_unload(h,"ticker",0));
    removing_hotel=hotel(h);removing_settings=settings(h);assert(!command(h,"stop settings") && removals==2);
    int peer=42;assert(!h->api.publish(h,OA_SVC_SETTINGS,1,&peer));
    assert(oa_module_start(h,m)==-1 && h->api.lookup(h,OA_SVC_SETTINGS,1)==&peer);
    assert(!oa_host_service_exists(h,OA_SVC_SETTINGS_HOTEL));
    assert(!h->api.unpublish(h,OA_SVC_SETTINGS));
    assert(!h->api.publish(h,OA_SVC_SETTINGS_HOTEL,1,&peer));
    assert(oa_module_start(h,m)==-1 && h->api.lookup(h,OA_SVC_SETTINGS_HOTEL,1)==&peer);
    assert(!oa_host_service_exists(h,OA_SVC_SETTINGS));
    assert(!h->api.unpublish(h,OA_SVC_SETTINGS_HOTEL) && !oa_module_start(h,m));
    puts("ok settings.hotel: either consumer hold pins provider; removal refuses cached reads; both publication collisions preserve peers");
    execute(writer,"DELETE FROM HMCSettings");failure(h,ENODATA);
    execute(writer,"DROP TABLE HMCSettings");failure(h,EIO);
    assert(!command(h,"reload settings"));m=oa_host_find_module(h,"settings");assert(m);failure(h,EIO);
    execute(writer,"CREATE TABLE HMCSettings(m_hotel_mode_availability INTEGER); INSERT INTO HMCSettings VALUES(1)");failure(h,EIO);
    execute(writer,"DROP TABLE HMCSettings; CREATE TABLE HMCSettings(m_hotel_mode_availability INTEGER,m_osd_menu_mode INTEGER,m_pic_mode_availability INTEGER);"
        "INSERT INTO HMCSettings VALUES(1,2,1); INSERT INTO HMCSettings VALUES(0,0,0)");
    check(h,1,2,1);assert(!command(h,"stop settings"));
    readonly_mode="0";assert(!command(h,"start settings") && settings(h)->is_writable());
    assert(!settings(h)->set_int("m_cec_enabled",0));check(h,1,2,1);
    removing_hotel=hotel(h);removing_settings=settings(h);
    assert(!command(h,"stop settings") && removals==4);assert(!oa_module_unload(h,"settings",0));
    assert(sqlite3_close(writer)==SQLITE_OK && !unlink(path) && !rmdir(dir));
    h->api.unsubscribe(h,sub);close(h->selfpipe[0]);close(h->selfpipe[1]);pthread_mutex_destroy(&h->post_mu);free(h);
    puts("ok settings.hotel: empty/schema errors preserve output, no schema creation/sync; first persisted row and cleanup");
    return 0;
}
