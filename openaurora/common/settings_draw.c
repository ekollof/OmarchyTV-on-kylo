#include "openaurora/settings_draw.h"
#include <errno.h>
struct drawing {
    const struct oa_setting_draw_ops *o;void *row;void *text[4];unsigned used;
    struct oa_setting_values values;int copied;
};
static void emit(struct drawing *d,enum oa_setting_draw_action action,uint32_t a,uint32_t b,
                 const void *text,const void *text2,const void *image)
{
    struct oa_setting_draw_event event={action,a,b,text,text2,image};
    d->o->run(d->o->context,d->row,&event);
}
#define SET(action,a,b) emit(&d,action,a,b,NULL,NULL,NULL)
#define TEXT(action,text,text2) emit(&d,action,0,0,text,text2,NULL)
#define IMAGE(action,image) emit(&d,action,0,0,NULL,NULL,image)
static void release_text(struct drawing *d)
{ while(d->used)d->o->text_free(d->o->context,d->text[--d->used]); }
static void *new_text(struct drawing *d,const void *source)
{
    void *text=d->o->text_new(d->o->context,source);
    if(text)d->text[d->used++]=text;
    return text;
}
static int button_value(struct drawing *d,const struct oa_setting_item *item,const struct oa_setting_draw_style *style)
{
    void *text=new_text(d,NULL);if(!text)return ENOMEM;
    if(item->value_count)d->o->text_assign(d->o->context,text,item->values[0].text);
    else if(!style->no_default)d->o->lookup_default(d->o->context,text);
    return 0;
}
int oa_settings_draw_row(const struct oa_setting_draw_ops *o,struct oa_setting_item *const *items,
                        int32_t count,int32_t index,void *row,const struct oa_setting_draw_style *style)
{
    struct drawing d={.o=o,.row=row};struct oa_setting_item *item;
    uint32_t type;int error=0;
    if(!o || !row || !style || !o->run || !o->store_type || !o->values_copy || !o->values_drop ||
       !o->text_new || !o->text_assign || !o->text_append_int || !o->text_empty ||
       !o->text_equals_empty || !o->text_free || !o->lookup_default)return EINVAL;
    if(count<=0 || index<0 || index>=count)return 0;
    if(!items || !(item=items[index]) || (item->value_count && !item->values))return ENODATA;
    SET(OA_SD_ROW_ID,item->id,0);type=item->type;
    if(style->progress) {
        SET(OA_SD_PROGRESS_MODE,1,0);
        if(type>=9 && type<=11) { type+=14;o->store_type(o->context,item,type); }
    } else if(style->mixed) {
        SET(OA_SD_MIXED_MODE,1,0);
        if(type==9) { type=23;o->store_type(o->context,item,type); }
    }
    if(type>=23 && type<=25)SET(OA_SD_BALL,style->ball,0);
    if(type==4 || type==8 || type==26) {
        if((error=button_value(&d,item,style)))goto done;
        uint32_t long_button=style->no_default?o->text_equals_empty(o->context,d.text[0])!=0:0;
        SET(OA_SD_LONG_BUTTON,long_button,0);release_text(&d);
    }
    if(type==11 || type==25)SET(OA_SD_BAR_IMAGES,item->min_image,item->max_image);
    SET(OA_SD_TYPE,type,0);SET(OA_SD_ACTIVE,item->active,0);
    if(type==5 || type==6) {
        if(!new_text(&d,item->label)) { error=ENOMEM;goto done; }
        TEXT(OA_SD_BUTTON_TEXT,d.text[0],NULL);release_text(&d);
        SET(OA_SD_IMAGE_VISIBLE,0,0);SET(OA_SD_SELECTABLE,1,0);
        if(item->additional.id!=UINT32_MAX)IMAGE(OA_SD_ADDITIONAL,item->additional.handle);
        if(item->service_logo<=INT32_MAX) {
            if(item->image.id==UINT32_MAX)SET(OA_SD_SERVICE_LOGO,item->service_logo,0);
            else IMAGE(OA_SD_IMAGE_DETAIL,item->image.handle);
        } else if(!o->text_empty(o->context,item->glyph_path)) {
            TEXT(OA_SD_GLYPH_PATH,item->glyph_path,item->image_url);
        } else if(item->image.id!=UINT32_MAX) {
            IMAGE(OA_SD_IMAGE_DETAIL,item->image.handle);
        } else if(!o->text_empty(o->context,item->image_url)) {
            TEXT(OA_SD_IMAGE_URL,item->image_url,NULL);
        } else SET(OA_SD_GLYPH,item->glyph,0);
        goto done;
    }
    if(type==7) {
        SET(OA_SD_IMAGE_VISIBLE,0,0);
        if(item->additional.id!=UINT32_MAX)IMAGE(OA_SD_ADDITIONAL,item->additional.handle);
        if(!o->text_empty(o->context,item->image_url))TEXT(OA_SD_IMAGE_URL,item->image_url,NULL);
        goto done;
    }
    if(!new_text(&d,item->label)) { error=ENOMEM;goto done; }
    TEXT(OA_SD_LABEL,d.text[0],NULL);release_text(&d);
    switch(type) {
    case 4:case 8:case 26:
        if((error=button_value(&d,item,style)))goto done;
        if(!new_text(&d,d.text[0])) { error=ENOMEM;goto done; }
        TEXT(OA_SD_BUTTON_TEXT,d.text[1],NULL);goto done;
    case 9:case 10:case 11:case 12:case 23:case 24:case 25:
        SET(OA_SD_RANGE,item->minimum,item->maximum);
        SET(type>=23?OA_SD_PROGRESS:OA_SD_SLIDE,item->value,0);
        if(type!=10 && type!=12 && type!=24)goto done;
        break;
    case 13:SET(OA_SD_NUMERIC,item->value,0);goto done;
    case 22:if(item->value_count)SET(OA_SD_CHECKED,item->checked,0);goto done;
    case 27:SET(OA_SD_SELECTABLE,0,0);goto done;
    case 2:case 3:SET(OA_SD_CLEAR_ITEMS,0,0);break;
    case 16:case 17:case 18:case 19:case 20:case 21:break;
    default:goto done;
    }
    error=o->values_copy(o->context,item,&d.values);if(error)goto done;
    d.copied=1;
    if(d.values.count && !d.values.items) { error=ENODATA;goto done; }
    if(type==2 || type==3) {
        for(size_t i=0;i<d.values.count;++i) {
            if(!new_text(&d,d.values.items[i].text)) { error=ENOMEM;goto done; }
            emit(&d,OA_SD_APPEND_ITEM,d.values.items[i].id,0,d.text[0],NULL,NULL);release_text(&d);
        }
        SET(OA_SD_SELECTED_ID,item->value,0);
        if(type==3)SET(OA_SD_SELECTION_IMAGE,item->max_image,0);
    } else if(type==10 || type==12 || type==24) {
        if(d.values.count>=2) {
            if(!new_text(&d,d.values.items[0].text) || !new_text(&d,d.values.items[1].text)) { error=ENOMEM;goto done; }
            TEXT(OA_SD_MINMAX_TEXT,d.text[0],d.text[1]);
        } else {
            if(!new_text(&d,NULL)) { error=ENOMEM;goto done; }
            o->text_append_int(o->context,d.text[0],item->minimum);
            if(!new_text(&d,NULL)) { error=ENOMEM;goto done; }
            o->text_append_int(o->context,d.text[1],item->maximum);
            if(!new_text(&d,d.text[0]) || !new_text(&d,d.text[1])) { error=ENOMEM;goto done; }
            TEXT(OA_SD_MINMAX_TEXT,d.text[2],d.text[3]);
        }
    } else if(d.values.count) {
        static const enum oa_setting_draw_action actions[]={OA_SD_TEXT,OA_SD_DATE,OA_SD_TIME,OA_SD_DURATION,OA_SD_PASSWORD,OA_SD_IP};
        if(!new_text(&d,d.values.items[0].text)) { error=ENOMEM;goto done; }
        TEXT(actions[type-16],d.text[0],NULL);
    }
done:
    release_text(&d);
    if(d.copied)o->values_drop(o->context,&d.values);
    return error;
}
