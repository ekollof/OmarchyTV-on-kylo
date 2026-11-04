#include "openaurora/launcher_db.h"
#include <errno.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Schema order, matching the vendor's MemberAccess table. */
static const char query_text[]=
    "SELECT m_application_image_crc,m_application_thumb_update_time,"
    "m_application_update_time,m_channel_handle,m_id,m_is_delete_disabled,"
    "m_is_deleted,m_is_featured,m_is_moved_by_user,m_is_out_of_sync,"
    "m_item_type,m_menu_return_id,m_order_index,m_original_id,m_parent_id,"
    "m_server_id,m_application_ad_image_url,m_application_ad_url,"
    "m_application_name,m_application_thumb_file_name,m_application_thumb_url,"
    "m_application_url,m_fvp_application_id,m_menu_string_id,"
    "m_recommendation_url,m_application_ad_tracking_url FROM LauncherItem";

static void set_error(char *error,size_t cap,const char *text)
{
    if(error && cap)snprintf(error,cap,"%s",text?text:"");
}

static void copy_text(char *dst,size_t size,sqlite3_stmt *st,int column)
{
    const unsigned char *src=sqlite3_column_blob(st,column);
    int bytes=sqlite3_column_bytes(st,column);
    size_t length=0;
    if(src && bytes>0) {
        while(length<(size_t)bytes && length+1<size && src[length])++length;
        memcpy(dst,src,length);
    }
    dst[length]='\0';
}

static int append_row(struct oa_launcher_list *list,
    const struct oa_launcher_info *row)
{
    if(list->count==list->capacity) {
        size_t capacity=list->capacity?list->capacity*2:16;
        if(capacity<list->capacity || capacity>SIZE_MAX/sizeof *list->rows)
            return -EOVERFLOW;
        struct oa_launcher_info *rows=realloc(list->rows,
            capacity*sizeof *rows);
        if(!rows)return -ENOMEM;
        list->rows=rows;list->capacity=capacity;
    }
    list->rows[list->count++]=*row;
    return 0;
}

static int read_rows(const char *path,int32_t parent_id,int use_parent,
    int include_deleted,int include_out_of_sync,struct oa_launcher_list *out,
    char *error,size_t error_size)
{
    sqlite3 *db=NULL;
    sqlite3_stmt *st=NULL;
    int result=-EIO;
    if(!path || !out)return -EINVAL;
    oa_launcher_db_free(out);
    set_error(error,error_size,"");
    if(sqlite3_open_v2(path,&db,SQLITE_OPEN_READONLY,NULL)!=SQLITE_OK) {
        set_error(error,error_size,db?sqlite3_errmsg(db):"open failed");
        result=-EIO;
        goto done;
    }
    if(sqlite3_prepare_v2(db,query_text,-1,&st,NULL)!=SQLITE_OK) {
        set_error(error,error_size,sqlite3_errmsg(db));
        result=-EIO;
        goto done;
    }
    for(;;) {
        int step=sqlite3_step(st);
        if(step==SQLITE_DONE)break;
        if(step!=SQLITE_ROW) {
            set_error(error,error_size,sqlite3_errmsg(db));
            result=-EIO;
            goto done;
        }
        struct oa_launcher_info row;
        memset(&row,0,sizeof row);
        row.image_crc=(uint32_t)sqlite3_column_int64(st,0);
        row.thumb_update_time=(uint32_t)sqlite3_column_int64(st,1);
        row.application_update_time=(uint32_t)sqlite3_column_int64(st,2);
        row.channel_handle=(int32_t)sqlite3_column_int(st,3);
        row.id=(int32_t)sqlite3_column_int(st,4);
        row.is_delete_disabled=(uint8_t)sqlite3_column_int(st,5);
        row.is_deleted=(uint8_t)sqlite3_column_int(st,6);
        row.is_featured=(uint8_t)sqlite3_column_int(st,7);
        row.is_moved_by_user=(uint8_t)sqlite3_column_int(st,8);
        row.is_out_of_sync=(uint8_t)sqlite3_column_int(st,9);
        row.item_type=(int32_t)sqlite3_column_int(st,10);
        row.menu_return_id=(int32_t)sqlite3_column_int(st,11);
        row.order_index=(int32_t)sqlite3_column_int(st,12);
        row.original_id=(int32_t)sqlite3_column_int(st,13);
        row.parent_id=(int32_t)sqlite3_column_int(st,14);
        row.server_id=(int32_t)sqlite3_column_int(st,15);
        copy_text(row.ad_image_url,sizeof row.ad_image_url,st,16);
        copy_text(row.ad_url,sizeof row.ad_url,st,17);
        copy_text(row.application_name,sizeof row.application_name,st,18);
        copy_text(row.thumb_file_name,sizeof row.thumb_file_name,st,19);
        copy_text(row.thumb_url,sizeof row.thumb_url,st,20);
        copy_text(row.application_url,sizeof row.application_url,st,21);
        copy_text(row.fvp_application_id,sizeof row.fvp_application_id,st,22);
        copy_text(row.menu_string_id,sizeof row.menu_string_id,st,23);
        copy_text(row.recommendation_url,sizeof row.recommendation_url,st,24);
        copy_text(row.ad_tracking_url,sizeof row.ad_tracking_url,st,25);
        if(use_parent && row.parent_id!=parent_id)continue;
        if(!include_deleted && row.is_deleted)continue;
        if(!include_out_of_sync && row.is_out_of_sync)continue;
        if(append_row(out,&row)) {
            set_error(error,error_size,"out of memory");
            result=-ENOMEM;
            goto done;
        }
    }
    result=0;
done:
    if(st)sqlite3_finalize(st);
    if(db)sqlite3_close(db);
    if(result)oa_launcher_db_free(out);
    return result;
}

int oa_launcher_db_read_all(const char *path,struct oa_launcher_list *out,
    char *error,size_t error_size)
{
    return read_rows(path,0,0,1,1,out,error,error_size);
}

int oa_launcher_db_read_parent(const char *path,int32_t parent_id,
    int include_deleted,int include_out_of_sync,struct oa_launcher_list *out,
    char *error,size_t error_size)
{
    return read_rows(path,parent_id,1,include_deleted,include_out_of_sync,
        out,error,error_size);
}

void oa_launcher_db_free(struct oa_launcher_list *list)
{
    if(!list)return;
    free(list->rows);
    list->rows=NULL;
    list->count=0;
    list->capacity=0;
}
