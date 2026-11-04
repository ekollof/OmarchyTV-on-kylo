/* Live LauncherItem cards in ui_vendor: read parent-0 rows, order by
 * m_order_index, load the named thumbnails and verify the card blits. The
 * display is the recorder, so this is layout/selection evidence, not scanout. */
#include "host.h"
#include "openaurora/events.h"
#include "openaurora/services.h"
#include "openaurora/ui_engine.h"
#include "ui_nav_trace.h"
#include <assert.h>
#include <ftw.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_EXPECT 64
struct card { int id,order; char name[64]; };
static char g_ui_dir[256],g_db[512],g_modules[256],g_assets[256],g_tmp[256];
static char g_png_dir[512];
static int g_apps,g_png;
static char g_grid_sel[16]="0";
static const char *g_override="";

static const char *config(void *p,const char *key,const char *def)
{
    (void)p;
    if(!strcmp(key,"ui.dir"))return g_ui_dir;
    if(!strcmp(key,"settings.db"))return g_db;
    if(!strcmp(key,"settings.readonly"))return "1";
    if(!strcmp(key,"ui.paint"))return "1";
    if(!strcmp(key,"ui.fps"))return "0";
    if(!strcmp(key,"ui.live_cards"))return "1";
    if(!strcmp(key,"ui.cards"))return g_override;
    if(g_apps && !strcmp(key,"ui.screen"))return "apps";
    if(g_apps && !strcmp(key,"ui.grid_sel"))return g_grid_sel;
    if(g_png && !strcmp(key,"ui.download_dir"))return g_png_dir;
    return def;
}
static int command(oa_host *h,const char *text,char *out,size_t cap)
{
    char line[128];
    snprintf(line,sizeof line,"%s",text);
    return oa_control_command(h,line,out,cap);
}
static void ensure_blob(const char *name,int fallback_w,int fallback_h,
    int *out_w,int *out_h)
{
    char src[512],path[512];
    if(g_assets[0]) {
        snprintf(src,sizeof src,"%s/%s.rgba",g_assets,name);
        FILE *probe=fopen(src,"rb");
        if(probe) {
            unsigned char hdr[12];
            assert(fread(hdr,1,sizeof hdr,probe)==sizeof hdr);
            assert(fclose(probe)==0);
            assert(!memcmp(hdr,"OAI1",4));
            *out_w=hdr[4]|hdr[5]<<8|hdr[6]<<16|hdr[7]<<24;
            *out_h=hdr[8]|hdr[9]<<8|hdr[10]<<16|hdr[11]<<24;
            snprintf(path,sizeof path,"%s/%s.rgba",g_ui_dir,name);
            FILE *out=fopen(path,"wb");
            assert(out);
            assert(fwrite(hdr,1,sizeof hdr,out)==sizeof hdr);
            unsigned char buf[4096];
            size_t n;
            FILE *in=fopen(src,"rb");
            assert(in);
            assert(fseek(in,12,SEEK_SET)==0);
            while((n=fread(buf,1,sizeof buf,in))>0)
                assert(fwrite(buf,1,n,out)==n);
            assert(fclose(in)==0);
            assert(fclose(out)==0);
            return;
        }
    }
    int w=fallback_w,h=fallback_h;
    *out_w=w;*out_h=h;
    snprintf(path,sizeof path,"%s/%s.rgba",g_ui_dir,name);
    FILE *f=fopen(path,"wb");
    assert(f);
    unsigned char hdr[12]={'O','A','I','1',
        (unsigned char)w,(unsigned char)(w>>8),(unsigned char)(w>>16),(unsigned char)(w>>24),
        (unsigned char)h,(unsigned char)(h>>8),(unsigned char)(h>>16),(unsigned char)(h>>24)};
    assert(fwrite(hdr,1,sizeof hdr,f)==sizeof hdr);
    for(int i=0;i<w*h;i++) {
        unsigned char px[4]={(unsigned char)(i*3),(unsigned char)(i*5),(unsigned char)(i*7),255};
        assert(fwrite(px,1,sizeof px,f)==sizeof px);
    }
    assert(fclose(f)==0);
}
static int remove_entry(const char *path,const struct stat *st,int flag,
    struct FTW *ftw)
{
    (void)st;(void)flag;(void)ftw;
    return remove(path);
}
static void cleanup_tmp(void)
{
    if(g_tmp[0])nftw(g_tmp,remove_entry,16,FTW_DEPTH|FTW_PHYS);
}
static int card_cmp(const void *a,const void *b)
{
    const struct card *x=a,*y=b;
    if(x->order!=y->order)return x->order<y->order?-1:1;
    return x->id<y->id?-1:x->id>y->id?1:0;
}
static void exec(sqlite3 *db,const char *sql)
{
    char *error=NULL;
    if(sqlite3_exec(db,sql,NULL,NULL,&error)!=SQLITE_OK) {
        fprintf(stderr,"sql: %s\n",error?error:"?");exit(1);
    }
}
static void synthetic_db(const char *path,int apps,int png)
{
    sqlite3 *db=NULL;
    assert(sqlite3_open(path,&db)==SQLITE_OK);
    exec(db,
        "CREATE TABLE LauncherItem(m_application_image_crc integer,"
        "m_application_thumb_update_time integer,m_application_update_time integer,"
        "m_channel_handle integer,m_id integer,m_is_delete_disabled integer,"
        "m_is_deleted integer,m_is_featured integer,m_is_moved_by_user integer,"
        "m_is_out_of_sync integer,m_item_type integer,m_menu_return_id integer,"
        "m_order_index integer,m_original_id integer,m_parent_id integer,"
        "m_server_id integer,m_application_ad_image_url blob,"
        "m_application_ad_url blob,m_application_name blob,"
        "m_application_thumb_file_name blob,m_application_thumb_url blob,"
        "m_application_url blob,m_fvp_application_id blob,"
        "m_menu_string_id blob,m_recommendation_url blob,"
        "m_application_ad_tracking_url blob)");
    if (png) {
        static const char *names[] = {
            "launcher_app_thumb61", "launcher_app_thumb336",
            "launcher_app_thumb368", "launcher_app_thumb495",
            "launcher_app_thumb356", "launcher_app_thumb355",
            "launcher_app_thumb354", "launcher_app_thumb385",
            "launcher_app_thumb476",
        };
        for (int i = 0; i < 9; i++) {
            char sql[512];
            snprintf(sql, sizeof sql,
                "INSERT INTO LauncherItem VALUES(0,0,0,-1,%d,0,0,1,0,0,2,-1,%d,0,0,0,"
                "'','','App %d','%s','','','','','','')",
                300 + i, i + 1, i, names[i]);
            exec(db, sql);
        }
    } else if (apps) {
        /* The larger corpus reverses insertion/order, so truncating to64
         * before sorting must lose the globally earliest six items. */
        int many=getenv("OA_TEST_MANY")!=NULL;
        int items=many?70:20;
        for (int i = 0; i < items; i++) {
            char name[32], sql[512];
            snprintf(name, sizeof name, "live_%02d", i);
            snprintf(sql, sizeof sql,
                "INSERT INTO LauncherItem VALUES(0,0,0,-1,%d,0,0,1,0,0,2,-1,%d,0,0,0,"
                "'','','App %d','%s','','','','','','')",
                200 + i, many?items-i:i+1, i, name);
            exec(db, sql);
        }
    } else {
        /* Inserted a,b,c but ordered 3,1,2: the UI must use b,c,a. The
         * deleted, parent-1 and type-1 rows must not become cards. */
        exec(db,"INSERT INTO LauncherItem VALUES(0,0,0,-1,101,0,0,1,0,0,2,-1,3,0,0,0,'','','App A','live_a','','','','','','')");
        exec(db,"INSERT INTO LauncherItem VALUES(0,0,0,-1,102,0,0,1,0,0,2,-1,1,0,0,0,'','','App B','live_b','','','','','','')");
        exec(db,"INSERT INTO LauncherItem VALUES(0,0,0,-1,103,0,0,1,0,0,2,-1,2,0,0,0,'','','App C','live_c','','','','','','')");
    }
    exec(db,"INSERT INTO LauncherItem VALUES(0,0,0,-1,104,0,1,1,0,0,2,-1,0,0,0,0,'','','Deleted','live_deleted','','','','','','')");
    exec(db,"INSERT INTO LauncherItem VALUES(0,0,0,-1,105,0,0,1,0,0,2,-1,4,0,1,0,'','','Other parent','live_other','','','','','','')");
    exec(db,"INSERT INTO LauncherItem VALUES(0,0,0,-1,106,0,0,1,0,0,1,-1,5,0,0,0,'','','Menu','live_menu','','','','','','')");
    assert(sqlite3_close(db)==SQLITE_OK);
}
/* Independent oracle: parent0, non-deleted, type2, non-empty thumbnail name,
 * ordered by m_order_index then m_id. */
static unsigned expected_cards(const char *path,struct card *cards)
{
    sqlite3 *db=NULL;
    sqlite3_stmt *st=NULL;
    assert(sqlite3_open_v2(path,&db,SQLITE_OPEN_READONLY,NULL)==SQLITE_OK);
    assert(sqlite3_prepare_v2(db,
        "SELECT m_id,m_order_index,m_application_thumb_file_name FROM LauncherItem"
        " WHERE m_parent_id=0 AND m_is_deleted=0 AND m_is_out_of_sync=0 AND m_item_type=2"
        " ORDER BY m_order_index,m_id",-1,&st,NULL)==SQLITE_OK);
    unsigned n=0;
    while(sqlite3_step(st)==SQLITE_ROW && n<MAX_EXPECT) {
        const unsigned char *text=sqlite3_column_blob(st,2);
        int bytes=sqlite3_column_bytes(st,2);
        if(!text || bytes<1 || !text[0])continue;
        cards[n].id=sqlite3_column_int(st,0);
        cards[n].order=sqlite3_column_int(st,1);
        size_t length=0;
        while(length<(size_t)bytes && text[length] && length<sizeof cards[n].name-1)
            ++length;
        memcpy(cards[n].name,text,length);cards[n].name[length]='\0';
        if(strchr(cards[n].name,'/'))continue;
        ++n;
    }
    sqlite3_finalize(st);sqlite3_close(db);
    qsort(cards,n,sizeof cards[0],card_cmp);
    return n;
}
static int find_fill(const oa_nav_trace *trace,int x,int y,int w,int h,
    int r,int g,int b)
{
    int count=trace->fill_count();
    for(int i=0;i<count;++i) {
        int fx,fy,fw,fh,fr,fg,fb;
        if(trace->fill_at(i,&fx,&fy,&fw,&fh,&fr,&fg,&fb))continue;
        if(fx==x && fy==y && fw==w && fh==h && fr==r && fg==g && fb==b)
            return 1;
    }
    return 0;
}
static int visible_cards(unsigned n)
{
    int visible=0;
    for(unsigned i=0;i<n;++i) {
        int x=209+(int)i*190;
        if(x+171>1920)break;
        ++visible;
    }
    return visible;
}
static int find_card_blit(const oa_nav_trace *trace,int x,int y,int w,int h,
    int sw,int sh)
{
    int count=trace->scaled_count();
    for(int i=0;i<count;++i) {
        int bx,by,bw,bh,bsw,bsh;
        if(trace->scaled_at(i,&bx,&by,&bw,&bh,&bsw,&bsh))continue;
        if(bx==x && by==y && bw==w && bh==h && bsw==sw && bsh==sh)
            return 1;
    }
    return 0;
}
static int png_first_px(const char *name,int *r,int *g,int *b,int *a)
{
    static const struct { const char *name; int r,g,b,a; } px[] = {
        {"launcher_app_thumb336",26,152,255,255},
        {"launcher_app_thumb354",224,0,0,255},
        {"launcher_app_thumb355",224,0,0,255},
        {"launcher_app_thumb356",224,0,0,255},
        {"launcher_app_thumb368",255,255,255,255},
        {"launcher_app_thumb385",31,9,44,255},
        {"launcher_app_thumb476",0,0,0,255},
        {"launcher_app_thumb495",0,0,0,255},
        {"launcher_app_thumb61",255,255,255,255},
    };
    for(size_t i=0;i<sizeof px/sizeof px[0];++i)
        if(!strcmp(px[i].name,name)) {
            *r=px[i].r;*g=px[i].g;*b=px[i].b;*a=px[i].a;
            return 1;
        }
    return 0;
}
static int card_blit_first(const oa_nav_trace *trace,int x,int y,int w,int h,
    const char *name)
{
    int er,eg,eb,ea;
    if(!png_first_px(name,&er,&eg,&eb,&ea))return 1;
    int count=trace->scaled_count();
    for(int i=0;i<count;++i) {
        int bx,by,bw,bh,bsw,bsh;
        if(trace->scaled_at(i,&bx,&by,&bw,&bh,&bsw,&bsh))continue;
        if(bx!=x || by!=y || bw!=w || bh!=h)continue;
        int r,g,b,a;
        if(trace->scaled_first(i,&r,&g,&b,&a))continue;
        return r==er && g==eg && b==eb && a==ea;
    }
    return 0;
}

int main(int argc,char **argv)
{
    assert(argc>=3 && argc<=5);
    snprintf(g_modules,sizeof g_modules,"%s",argv[1]);
    g_apps=(argc==5 && !strcmp(argv[4],"apps")) || getenv("OA_TEST_APPS")!=NULL;
    g_png=argc==5 && !strcmp(argv[4],"png");
    const char *sel=getenv("OA_TEST_GRID_SEL");
    if(sel)snprintf(g_grid_sel,sizeof g_grid_sel,"%s",sel);
    char dir[]="/tmp/oa-live-cards.XXXXXX";
    assert(mkdtemp(dir));
    snprintf(g_tmp,sizeof g_tmp,"%s",dir);
    assert(!atexit(cleanup_tmp));
    snprintf(g_ui_dir,sizeof g_ui_dir,"%s/ui",dir);
    assert(!mkdir(g_ui_dir,0755));
    if(!strcmp(argv[2],"synthetic")) {
        snprintf(g_db,sizeof g_db,"%s/settings.db",dir);
        synthetic_db(g_db,g_apps,g_png);
    } else {
        snprintf(g_db,sizeof g_db,"%s",argv[2]);
        struct stat st;
        if(stat(g_db,&st)) {
            fprintf(stderr,"skipping: %s unavailable\n",g_db);
            return 77;
        }
    }
    if(g_png)
        snprintf(g_png_dir,sizeof g_png_dir,"%s",argv[3]);
    else if(argc>=4 && argv[3][0])
        snprintf(g_assets,sizeof g_assets,"%s",argv[3]);
    struct card expected[MAX_EXPECT];
    int expected_w[MAX_EXPECT],expected_h[MAX_EXPECT];
    unsigned total=expected_cards(g_db,expected);
    assert(total>0);
    if(!g_apps && total>12)total=12;
    /* Without an asset dir, unique source sizes encode the sorted position;
     * with one, the real OAI1 dimensions are read and verified instead.
     * Excluded names get a distinctive 200x200 so an accidental hit is
     * unmistakable. */
    for(unsigned i=0;i<total;++i) {
        if(g_png) {
            expected_w[i]=400;
            expected_h[i]=225;
        } else {
            ensure_blob(expected[i].name,8+(int)i,4+(int)i,
                &expected_w[i],&expected_h[i]);
        }
    }
    if(!g_png) {
        int e1,e2;
        ensure_blob("live_deleted",200,200,&e1,&e2);
        ensure_blob("live_other",200,200,&e1,&e2);
        ensure_blob("live_menu",200,200,&e1,&e2);
    }
    if(total>0) {
        oa_host host;
        assert(!oa_host_init(&host,g_modules,NULL));
        host.log_level=-1;host.api.config_get=config;
        char out[512];
        assert(!command(&host,"load nav_display",out,sizeof out));
        assert(!command(&host,"start nav_display",out,sizeof out));
        assert(!command(&host,"load settings",out,sizeof out));
        assert(!command(&host,"start settings",out,sizeof out));
        if(g_png) {
            assert(!command(&host,"load image_decode",out,sizeof out));
            assert(!command(&host,"start image_decode",out,sizeof out));
        }
        assert(!command(&host,"load ui_engine",out,sizeof out));
        assert(!command(&host,"start ui_engine",out,sizeof out));
        if(g_apps && getenv("OA_TEST_MANY")) {
            const oa_ui_engine_iface *engine=host.api.lookup(&host,OA_SVC_UI_ENGINE,2);
            assert(engine);
            for(int i=0;i<32;++i) {
                char name[32];int w,h;
                snprintf(name,sizeof name,"other_screen_%d",i);
                ensure_blob(name,2,2,&w,&h);
                assert(engine->icon(name));
            }
            host.api.release(&host,OA_SVC_UI_ENGINE);
        }
        assert(!command(&host,"load ui_vendor",out,sizeof out));
        assert(!command(&host,"start ui_vendor",out,sizeof out));
        const oa_nav_trace *trace=host.api.lookup(&host,OA_SVC_NAV_TRACE,1);
        const oa_portal_iface *portal=host.api.lookup(&host,OA_SVC_PORTAL,1);
        assert(trace && portal);
        assert(portal->screen()==(g_apps?5:0));
        if(g_apps) {
            int first=(atoi(g_grid_sel)/15)*15;
            if(first>=(int)total)first=0;
            int visible=(int)total-first;
            if(visible>15)visible=15;
            for(int i=0;i<visible;++i) {
                int x=183+(i%5)*310,y=180+(i/5)*230;
                int ew=expected_w[first+i],eh=expected_h[first+i];
                int cw=310-40,ch=cw*eh/ew;
                if(ch>230-20){ch=230-20;cw=ch*ew/eh;}
                int dx=x+(310-cw)/2,dy=y+(230-ch)/2;
                if(!find_card_blit(trace,dx,dy,cw,ch,ew,eh)) {
                    fprintf(stderr,"missing apps cell %d at %d,%d src %dx%d\n",
                        i,dx,dy,ew,eh);
                    return 1;
                }
                if(g_png && !card_blit_first(trace,dx,dy,cw,ch,
                        expected[first+i].name)) {
                    fprintf(stderr,"wrong PNG pixels in apps cell %d\n",i);
                    return 1;
                }
            }
            assert(find_fill(trace,0,0,1921,162,72,23,48));
            /* Track caps/row are fixed; the bar size/position follow the
             * shipped formula for this row count (15 visible, 36 px min). */
            assert(find_fill(trace,1750,180,8,20,129,25,36));
            assert(find_fill(trace,1750,200,8,684,129,25,36));
            assert(find_fill(trace,1750,884,8,16,129,25,36));
            int bar=720*15/(int)total;
            if(bar<36)bar=36;
            if(bar>=720)bar=720;
            int pos=(int)total>15+first?(720-bar)*first/((int)total-15):720-bar;
            if(bar==720)bar=0;
            if(bar>0) {
                assert(find_fill(trace,1750,180+pos,8,20,255,255,255));
                assert(find_fill(trace,1750,180+pos+20,8,bar-36,255,255,255));
                assert(find_fill(trace,1750,180+pos+bar-16,8,16,255,255,255));
            }
            const char *cap=getenv("OA_NAV_CAPTURE");
            if(cap) {
                char path[512];
                snprintf(path,sizeof path,"%s.apps",cap);
                assert(!trace->capture(path));
            }
            printf("ok ui apps grid: %u rows, %d visible cells, first %d, scrollbar %d\n",
                total,visible,first,bar);
            if(getenv("OA_TEST_MANY")) {
                /* Browse every page with other-screen assets already cached. */
                for(int page=1;page<5;++page) {
                    for(int i=0;i<3;++i) {
                        oa_key_event ev={.id=19,.kind=OA_KEY_PRESS};
                        host.api.emit(&host,OA_TOPIC_KEY,&ev);oa_host_dispatch_posts(&host);
                    }
                    int at=page*15,w=270,h=w*expected_h[at]/expected_w[at];
                    if(h>210){h=210;w=h*expected_w[at]/expected_h[at];}
                    assert(find_card_blit(trace,183+(310-w)/2,180+(230-h)/2,
                        w,h,expected_w[at],expected_h[at]));
                }
                puts("ok ui apps cache: all64 globally ordered cards render after other screens");
            }
            if(!strcmp(argv[2],"synthetic") && !getenv("OA_TEST_MANY") && atoi(g_grid_sel)==0) {
                for(int i=0;i<3;++i) {
                    oa_key_event ev={.id=19,.kind=OA_KEY_PRESS};
                    host.api.emit(&host,OA_TOPIC_KEY,&ev);
                    oa_host_dispatch_posts(&host);
                }
                assert(find_fill(trace,1750,360,8,20,255,255,255));
                host.api.release(&host,OA_SVC_PORTAL);
                assert(!command(&host,"reload ui_vendor",out,sizeof out));
                oa_host_dispatch_posts(&host);
                assert(find_fill(trace,1750,360,8,20,255,255,255));
                /* Reuse the same context with a single explicit card: old
                 * cached rows and the no-longer-needed track must disappear. */
                g_override="live_00";
                assert(!command(&host,"stop ui_vendor",out,sizeof out));
                assert(!command(&host,"start ui_vendor",out,sizeof out));
                oa_host_dispatch_posts(&host);
                assert(trace->scaled_count()==1);
                assert(!find_fill(trace,1750,180,8,20,129,25,36));
                puts("ok ui apps lifecycle: selection survives reload, restart replaces rows and hides idle scrollbar");
            }
        } else {
            int visible=visible_cards(total);
            for(int i=0;i<visible;++i) {
                int x=209+i*190,y=478,w=171,h=95;
                int sw=expected_w[i],sh=expected_h[i];
                if(!find_card_blit(trace,x,y,w,h,sw,sh)) {
                    fprintf(stderr,"missing live card %d at %d,%d src %dx%d\n",
                        i,x,y,sw,sh);
                    return 1;
                }
                if(g_png && !card_blit_first(trace,x,y,w,h,expected[i].name)) {
                    fprintf(stderr,"wrong PNG pixels in live card %d\n",i);
                    return 1;
                }
            }
            if(!find_card_blit(trace,209,478,171,95,200,200))
                puts("ok live cards: excluded rows not painted");
            else {
                fputs("excluded card was painted\n",stderr);return 1;
            }
            const char *cap=getenv("OA_NAV_CAPTURE");
            if(cap) {
                char path[512];
                snprintf(path,sizeof path,"%s.portal",cap);
                assert(!trace->capture(path));
            }
            if (g_png)
                printf("ok ui png cards: %u downloaded PNGs decoded at 400x225 with source pixels\n",
                    total);
            else
                printf("ok ui live cards: %u rows, %d visible cards ordered by m_order_index\n",
                    total,visible);
        }
    }
    return 0;
}
