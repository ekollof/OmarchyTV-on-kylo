#include "host.h"
#include "openaurora/svg_pack.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *index_path, *binary_path;
static const char *config(void *priv, const char *key, const char *def)
{
    (void)priv;
    if (!strcmp(key,"ui_theme.index")) return index_path;
    if (!strcmp(key,"ui_theme.binary")) return binary_path;
    return def;
}
static void write_bytes(const char *path, const void *data, size_t size)
{
    FILE *f=fopen(path,"wb"); assert(f);
    assert(fwrite(data,1,size,f)==size && !fclose(f));
}
static void write_text(const char *path, const char *text) { write_bytes(path,text,strlen(text)); }
/* Own synthetic data, not a copy of a vendor record. */
static void fixture(const char *index, const char *binary, unsigned x)
{
    uint8_t bytes[]={1,'r',0,0,0,0,0,7,0,9,0,0,0,10,0,0,0,0,0,0};
    bytes[3]=(uint8_t)x;
    write_bytes(binary,bytes,sizeof bytes);
    write_text(index,"1\n0xe8eb982e 20\n"); /* CRC32 of SettingListBoxUFA.svg */
}
static int xpos(const oa_ui_theme_iface *s)
{
    struct oa_binsvg v, by_name;
    assert(s->count()==1 && !s->record(0,&v));
    assert(!s->named("SettingListBoxUFA.svg",&by_name));
    assert(v.data==by_name.data && v.size==by_name.size);
    struct oa_binsvg_rect r;
    assert(!oa_binsvg_find_rect(&v,NULL,"r",&r));
    return r.bounds.left;
}
static int command(oa_host *h, const char *text)
{
    char line[80], reply[256]; snprintf(line,sizeof line,"%s",text);
    return oa_control_command(h,line,reply,sizeof reply);
}
static const oa_ui_theme_iface *service(oa_host *h)
{
    const oa_ui_theme_iface *s=h->api.lookup(h,OA_SVC_UI_THEME,1); assert(s); return s;
}
static void corpus(oa_host *h, const char *index, const char *binary)
{
    index_path=index; binary_path=binary;
    oa_mod *m=oa_module_load(h,"ui_theme"); assert(m && !oa_module_start(h,m));
    const oa_ui_theme_iface *s=service(h);
    assert(s->count()==702);
    size_t total=0; unsigned nodes=0;
    for (unsigned i=0;i<s->count();++i) {
        struct oa_binsvg v; assert(!s->record(i,&v)); total+=v.size;
        size_t at=0; struct oa_binsvg_element e;
        while (!oa_binsvg_next(&v,&at,&e)) ++nodes;
        assert(at==v.size);
    }
    assert(total==289764 && nodes==11888);
    struct oa_binsvg named, by_id;
    assert(!s->record(477,&by_id) && !s->named("SettingListBoxUFA.svg",&named));
    assert(named.data==by_id.data && named.size==by_id.size);
    struct oa_binsvg_rect row;
    assert(!oa_binsvg_find_rect(&named,NULL,"rowPos",&row));
    assert(row.bounds.left==244 && row.bounds.top==178 && row.bounds.right==319 && row.bounds.bottom==248);
    assert(!oa_module_unload(h,"ui_theme",0));
    puts("  ok   live-pack real DSO:702 records/11888 elements, CRC name477 and rowPos verified in memory");
}
int main(int argc, char **argv)
{
    assert(argc==2 || argc==4);
    oa_host *h=calloc(1,sizeof *h); assert(h && !oa_host_init(h,argv[1],NULL));
    h->log_level=-1; h->api.config_get=config;
    if (argc==4) corpus(h,argv[2],argv[3]);
    char dir[]="/tmp/oa-theme-module.XXXXXX"; assert(mkdtemp(dir));
    char index[256], binary[256], fifo[256];
    snprintf(index,sizeof index,"%s/index",dir);
    snprintf(binary,sizeof binary,"%s/binary",dir);
    snprintf(fifo,sizeof fifo,"%s/fifo",dir); assert(!mkfifo(fifo,0600));
    fixture(index,binary,11);
    index_path=index; binary_path=binary;
    oa_mod *m=oa_module_load(h,"ui_theme"); assert(m && !command(h,"start ui_theme"));
    const oa_ui_theme_iface *s=service(h); assert(xpos(s)==11);
    struct oa_binsvg held; assert(!s->record(0,&held));
    struct oa_binsvg_rect saved; assert(!oa_binsvg_find_rect(&held,NULL,"r",&saved));
    fixture(index,binary,22);
    assert(xpos(s)==11 && held.data[3]==11); /* no mmap or service-time reads */
    assert(s->record(1,&held)==ENOENT && held.data[3]==11);
    assert(s->named("settinglistboxufa.svg",&held)==ENOENT && held.data[3]==11);
    uint32_t old=m->id;
    assert(!command(h,"reload ui_theme"));
    m=oa_host_find_module(h,"ui_theme"); assert(m && m->id!=old);
    s=service(h); assert(xpos(s)==22 && saved.bounds.left==11);
    puts("  ok   CLI reload replaces snapshot; edits cannot mutate held views, copied geometry survives reload");

    oa_mod *consumer=oa_module_load(h,"ticker"); assert(consumer);
    h->cur_module=consumer->id; assert(h->api.lookup(h,OA_SVC_UI_THEME,1)==s); h->cur_module=0;
    assert(!s->record(0,&held));
    assert(command(h,"stop ui_theme")==-1 && command(h,"unload ui_theme")==-1 && command(h,"reload ui_theme")==-1);
    assert(m->state==OA_ST_STARTED && held.data[3]==22 && xpos(s)==22);
    h->cur_module=consumer->id; h->api.release(h,OA_SVC_UI_THEME); h->cur_module=0;
    assert(!oa_module_unload(h,"ticker",0) && !command(h,"stop ui_theme"));
    assert(!oa_host_service_exists(h,OA_SVC_UI_THEME));
    int peer=42; assert(!h->api.publish(h,OA_SVC_UI_THEME,1,&peer));
    assert(command(h,"start ui_theme")==-1 && h->api.lookup(h,OA_SVC_UI_THEME,1)==&peer);
    assert(!h->api.unpublish(h,OA_SVC_UI_THEME) && !command(h,"start ui_theme"));
    puts("  ok   consumer hold pins views and blocks normal stop/unload/reload; publication collision preserves peer and retries");

    assert(!oa_module_suspend_all(h));
    fixture(index,binary,33); assert(xpos(service(h))==22);
    assert(!oa_module_resume_all(h) && xpos(service(h))==22);
    assert(!oa_module_suspend_all(h) && !command(h,"reload ui_theme"));
    m=oa_host_find_module(h,"ui_theme");
    assert(m && m->state==OA_ST_SUSPENDED && !oa_host_service_exists(h,OA_SVC_UI_THEME));
    binary_path=fifo;
    assert(oa_module_resume_all(h)==-1 && m->state==OA_ST_SUSPENDED);
    assert(!oa_host_service_exists(h,OA_SVC_UI_THEME));
    binary_path=binary;
    assert(!oa_module_resume_all(h) && xpos(service(h))==33);
    assert(!command(h,"stop ui_theme"));
    puts("  ok   standby retains snapshot; suspended reload defers load/publication and failed resume activation retries");

    const char *bad_paths[]={NULL,"","/nonexistent/oa-theme",dir,fifo};
    for (size_t i=0;i<sizeof bad_paths/sizeof *bad_paths;++i) {
        index_path=bad_paths[i]; assert(oa_module_start(h,m)==-1);
        index_path=index; binary_path=bad_paths[i]; assert(oa_module_start(h,m)==-1);
        binary_path=binary;
        assert(!oa_host_service_exists(h,OA_SVC_UI_THEME));
    }
    const char *bad_indexes[]={"0","-1","65537","1 0xe8eb982e 0","1 0xe8eb982e -1",
        "1 0x100000000 20","1 0xe8eb982e 18446744073709551636","1 0xe8eb982e 21",
        "1 0xe8eb982e 19","2 0xe8eb982e 20","1 0xe8eb982e 20 junk","1 0x 20",
        "1 0xe8eb982e 67108865","99999999999999999999999999999999"};
    for (size_t i=0;i<sizeof bad_indexes/sizeof *bad_indexes;++i) {
        write_text(index,bad_indexes[i]);
        struct oa_svg_pack *pack=NULL;
        assert(oa_svg_pack_load(&pack,index,binary)==EINVAL && pack==NULL);
        assert(oa_module_start(h,m)==-1 && !oa_host_service_exists(h,OA_SVC_UI_THEME));
    }
    const char embedded[]="1\n0xe8eb982e 20\0junk";
    write_bytes(index,embedded,sizeof embedded-1); assert(oa_module_start(h,m)==-1);
    fixture(index,binary,44);
    uint8_t bad[20]={12}; write_bytes(binary,bad,sizeof bad);
    assert(oa_module_start(h,m)==-1); /* malformed actual record, not just index */
    fixture(index,binary,44);
    assert(!oa_module_start(h,m) && xpos(service(h))==44);
    assert(!command(h,"stop ui_theme"));
    puts("  ok   missing/special paths, numeric overflow, incomplete/trailing index, NUL and malformed records reject; no stale publication");

    /* First hash wins, even when later records differ. Memory remains valid
     * after both input files disappear. Failed load preserves previous output. */
    FILE *f=fopen(binary,"ab"); assert(f);
    assert(fwrite(bad,1,sizeof bad,f)==sizeof bad && !fclose(f));
    write_text(index,"2 0xe8eb982e 20 0xe8eb982e 20");
    struct oa_svg_pack *pack=NULL;
    assert(oa_svg_pack_load(&pack,index,binary)==EINVAL && !pack); /* validates ALL records */
    fixture(index,binary,55);
    uint8_t second[]={1,'r',0,66,0,0,0,7,0,9,0,0,0,10,0,0,0,0,0,0};
    f=fopen(binary,"ab"); assert(f && fwrite(second,1,sizeof second,f)==sizeof second && !fclose(f));
    write_text(index,"2 0xe8eb982e 20 0xe8eb982e 20");
    assert(!oa_svg_pack_load(&pack,index,binary) && oa_svg_pack_count(pack)==2);
    struct oa_binsvg v; assert(!oa_svg_pack_named(pack,"SettingListBoxUFA.svg",&v) && v.data[3]==55);
    assert(!oa_svg_pack_record(pack,1,&v) && v.data[3]==66);
    struct oa_svg_pack *before=pack;
    assert(oa_svg_pack_load(&pack,fifo,binary)==EINVAL && pack==before);
    assert(!unlink(index) && !unlink(binary));
    assert(!oa_svg_pack_record(pack,1,&v) && v.data[3]==66); oa_svg_pack_free(pack);
    assert(command(h,"start ui_theme")==-1 && !oa_host_service_exists(h,OA_SVC_UI_THEME));
    fixture(index,binary,77);
    assert(!command(h,"start ui_theme") && xpos(service(h))==77);
    assert(!unlink(binary) && command(h,"reload ui_theme")==-1);
    assert(!oa_host_find_module(h,"ui_theme") && !oa_host_service_exists(h,OA_SVC_UI_THEME));
    fixture(index,binary,88);
    m=oa_module_load(h,"ui_theme"); assert(m && !oa_module_start(h,m) && xpos(service(h))==88);
    assert(!command(h,"unload ui_theme") && !unlink(index) && !unlink(binary));
    assert(!unlink(fifo) && !rmdir(dir));
    close(h->selfpipe[0]); close(h->selfpipe[1]); pthread_mutex_destroy(&h->post_mu); free(h);
    puts("  ok   invalid later record rejects whole pack; duplicate CRC first wins; owned snapshot survives input deletion; failed load output preserved");
    puts("  ok   failed replacement removes provider without stale service; explicit load recovers (no host binary rollback)");
    return 0;
}
