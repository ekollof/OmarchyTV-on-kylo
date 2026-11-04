#include "openaurora/launcher_db.h"
#include <assert.h>
#include <errno.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const struct oa_launcher_info *find_id(const struct oa_launcher_list *l,
    int32_t id)
{
    for(size_t i=0;i<l->count;++i)
        if(l->rows[i].id==id)return &l->rows[i];
    return NULL;
}

static int probe(const char *path)
{
    struct oa_launcher_list all={0},parent0={0},parent1={0};
    char error[160];
    int rc=oa_launcher_db_read_all(path,&all,error,sizeof error);
    if(rc){fprintf(stderr,"probe read_all failed: %s\n",error);return 1;}
    rc=oa_launcher_db_read_parent(path,0,0,0,&parent0,error,sizeof error);
    if(rc){fprintf(stderr,"probe read_parent(0) failed: %s\n",error);return 1;}
    rc=oa_launcher_db_read_parent(path,1,1,1,&parent1,error,sizeof error);
    if(rc){fprintf(stderr,"probe read_parent(1) failed: %s\n",error);return 1;}
    printf("ok live launcher db probe: all=%zu parent0=%zu parent1(incl-deleted)=%zu\n",
        all.count,parent0.count,parent1.count);
    oa_launcher_db_free(&all);oa_launcher_db_free(&parent0);
    oa_launcher_db_free(&parent1);
    return 0;
}

static void make_truncation_db(const char *path)
{
    sqlite3 *db=NULL;
    assert(sqlite3_open(path,&db)==SQLITE_OK);
    const char *schema=
        "CREATE TABLE LauncherItem(m_application_image_crc integer,"
        "m_application_thumb_update_time integer,"
        "m_application_update_time integer,m_channel_handle integer,"
        "m_id integer,m_is_delete_disabled integer,m_is_deleted integer,"
        "m_is_featured integer,m_is_moved_by_user integer,"
        "m_is_out_of_sync integer,m_item_type integer,m_menu_return_id integer,"
        "m_order_index integer,m_original_id integer,m_parent_id integer,"
        "m_server_id integer,m_application_ad_image_url blob,"
        "m_application_ad_url blob,m_application_name blob,"
        "m_application_thumb_file_name blob,m_application_thumb_url blob,"
        "m_application_url blob,m_fvp_application_id blob,"
        "m_menu_string_id blob,m_recommendation_url blob,"
        "m_application_ad_tracking_url blob)";
    char *errmsg=NULL;
    if(sqlite3_exec(db,schema,NULL,NULL,&errmsg)!=SQLITE_OK) {
        fprintf(stderr,"truncation schema: %s\n",errmsg?errmsg:"?");
        sqlite3_free(errmsg);exit(1);
    }
    char name[128],recommendation[512];
    memset(name,'A',100);name[100]='\0';
    memset(recommendation,'B',400);recommendation[400]='\0';
    char sql[1024];
    snprintf(sql,sizeof sql,
        "INSERT INTO LauncherItem VALUES(1,2,3,-1,7,0,0,1,0,0,2,-1,0,0,0,0,"
        "'','','%s','thumb','','','','','%s','')",name,recommendation);
    if(sqlite3_exec(db,sql,NULL,NULL,&errmsg)!=SQLITE_OK) {
        fprintf(stderr,"truncation insert: %s\n",errmsg?errmsg:"?");
        sqlite3_free(errmsg);exit(1);
    }
    assert(sqlite3_close(db)==SQLITE_OK);
}

int main(int argc,char **argv)
{
    if(argc<2) {
        fprintf(stderr,"usage: %s <settingsDB.db> [probe]\n",argv[0]);
        return 77;
    }
    const char *path=argv[1];
    struct stat before;
    if(stat(path,&before)) {
        fprintf(stderr,"skipping: %s unavailable\n",path);
        return 77;
    }
    if(argc>2 && !strcmp(argv[2],"probe"))return probe(path);

    char error[160];
    struct oa_launcher_list all={0};
    int rc=oa_launcher_db_read_all(path,&all,error,sizeof error);
    if(rc){fprintf(stderr,"read_all: %s\n",error);return 1;}
    assert(all.count==50);
    assert(all.rows[0].id==1);

    const struct oa_launcher_info *row=find_id(&all,23);
    assert(row);
    assert(row->parent_id==0 && row->order_index==1 && row->item_type==2);
    assert(row->menu_return_id==-1 && row->channel_handle==-1);
    assert(row->server_id==1 && row->original_id==61);
    assert(row->image_crc==UINT32_C(2138913375));
    assert(row->application_update_time==UINT32_C(1784178668));
    assert(row->thumb_update_time==UINT32_C(1566204965));
    assert(row->is_featured==1 && row->is_deleted==0);
    assert(!strcmp(row->application_name,"Netflix"));
    assert(!strcmp(row->thumb_file_name,"launcher_app_thumb61"));
    assert(!strcmp(row->application_url,
        "http://www.portaltv.tv/swf/netflix/netflix.swf"));
    assert(!row->menu_string_id[0] && !row->recommendation_url[0]);
    assert(!row->fvp_application_id[0] && !row->ad_url[0]);
    assert(!row->ad_image_url[0] && !row->ad_tracking_url[0]);
    assert(!strcmp(row->thumb_url,
        "https://s3-eu-west-1.amazonaws.com/orionportal/orion/applogos/netflix_61.png"));

    /* Deleted rows: id43 under parent1, id49 under parent0. */
    row=find_id(&all,43);
    assert(row && row->is_deleted==1 && row->parent_id==1);
    row=find_id(&all,49);
    assert(row && row->is_deleted==1 && row->parent_id==0);
    oa_launcher_db_free(&all);

    struct oa_launcher_list list={0};
    assert(!oa_launcher_db_read_parent(path,0,0,0,&list,error,sizeof error));
    assert(list.count==23);
    oa_launcher_db_free(&list);
    assert(!oa_launcher_db_read_parent(path,0,1,0,&list,error,sizeof error));
    assert(list.count==24);
    oa_launcher_db_free(&list);
    assert(!oa_launcher_db_read_parent(path,1,0,0,&list,error,sizeof error));
    assert(list.count==23);
    oa_launcher_db_free(&list);
    assert(!oa_launcher_db_read_parent(path,1,1,1,&list,error,sizeof error));
    assert(list.count==24);
    oa_launcher_db_free(&list);
    assert(!oa_launcher_db_read_parent(path,-1,0,0,&list,error,sizeof error));
    assert(list.count==2);
    oa_launcher_db_free(&list);
    assert(!oa_launcher_db_read_parent(path,2,1,1,&list,error,sizeof error));
    assert(list.count==0);
    oa_launcher_db_free(&list);

    /* Fixed-size text fields truncate to size-1 and stay NUL-terminated. */
    char temp[]="/tmp/oa-launcher-db-XXXXXX";
    int fd=mkstemp(temp);
    assert(fd>=0);
    close(fd);
    remove(temp);
    make_truncation_db(temp);
    assert(!oa_launcher_db_read_all(temp,&list,error,sizeof error));
    assert(list.count==1);
    assert(strlen(list.rows[0].application_name)==59);
    assert(strlen(list.rows[0].recommendation_url)==299);
    assert(list.rows[0].application_name[0]=='A');
    assert(list.rows[0].recommendation_url[0]=='B');
    oa_launcher_db_free(&list);
    remove(temp);

    /* Missing database is an error, not an empty list. */
    assert(oa_launcher_db_read_all("/nonexistent/settingsDB.db",&list,
        error,sizeof error)<0);
    assert(error[0]);

    /* The read path must not modify the source database. */
    struct stat after;
    assert(!stat(path,&after));
    assert(after.st_size==before.st_size && after.st_mtime==before.st_mtime);

    puts("ok launcher db: 50 rows, parent/deleted filter, truncation and read-only");
    return 0;
}
