/* settingsDB LauncherItem read through the reloadable settings module and the
 * CLI/control path. Uses the dumped profile database, never the TV. */
#include "host.h"
#include "openaurora/launcher_db.h"
#include "openaurora/services.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *db_path;
static const char *config(void *p,const char *key,const char *def)
{
    (void)p;
    if(!strcmp(key,"settings.db"))return db_path;
    if(!strcmp(key,"settings.readonly"))return "1";
    return def;
}
static int command(oa_host *h,const char *text,char *out,size_t cap)
{
    char line[128];
    snprintf(line,sizeof line,"%s",text);
    return oa_control_command(h,line,out,cap);
}
static int contains(const char *text,const char *needle)
{ return strstr(text,needle)!=NULL; }

int main(int argc,char **argv)
{
    assert(argc==3);
    db_path=argv[2];
    FILE *probe=fopen(db_path,"rb");
    if(!probe){fprintf(stderr,"skipping: %s unavailable\n",db_path);return 77;}
    fclose(probe);
    oa_host *h=calloc(1,sizeof *h);
    assert(h && !oa_host_init(h,argv[1],NULL));
    h->log_level=-1;h->api.config_get=config;
    oa_mod *m=oa_module_load(h,"settings");
    assert(m && !oa_module_start(h,m));
    char out[8192];
    assert(command(h,"launcher-items 999999999999999999999999999999",out,sizeof out)==-1);
    assert(contains(out,"failed errno 22"));
    assert(command(h,"launcher-items -999999999999999999999999999999",out,sizeof out)==-1);
    assert(contains(out,"failed errno 22"));
    assert(!command(h,"launcher-items all",out,sizeof out));
    assert(contains(out,"count 50"));
    assert(!command(h,"launcher-items 0",out,sizeof out));
    assert(contains(out,"count 23"));
    assert(contains(out,"name=Netflix"));
    assert(!contains(out,"id=49"));
    assert(!command(h,"launcher-items 0 deleted",out,sizeof out));
    assert(contains(out,"count 24") && contains(out,"id=49"));
    assert(!command(h,"launcher-items 1",out,sizeof out));
    assert(contains(out,"count 23") && !contains(out,"id=43"));
    assert(!command(h,"launcher-items -1",out,sizeof out));
    assert(contains(out,"count 2"));
    assert(command(h,"launcher-items 0 badflag",out,sizeof out)==-1);
    assert(contains(out,"failed errno 22"));
    /* Reload the module: the service must be republished with the same data. */
    assert(!command(h,"reload settings",out,sizeof out));
    m=oa_host_find_module(h,"settings");assert(m);
    assert(!command(h,"launcher-items 0",out,sizeof out));
    assert(contains(out,"count 23") && contains(out,"name=Netflix"));
    /* Stop removes the service, and the CLI reports ENODEV. */
    assert(!command(h,"stop settings",out,sizeof out));
    assert(command(h,"launcher-items 0",out,sizeof out)==-1);
    assert(contains(out,"failed errno 19"));
    puts("ok launcher db module: 50 rows via reloadable settings service, filters and CLI");
    free(h);
    return 0;
}
