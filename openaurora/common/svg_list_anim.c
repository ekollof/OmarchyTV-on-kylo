#include "openaurora/svg_list_anim.h"
#include <errno.h>
#include <stddef.h>
static int ready(const struct oa_svg_list_anim_ops *o)
{
    return o && o->anim_enabled && o->selected_item && o->item_row && o->get_rect &&
        o->read_cur && o->write_prev && o->write_cur && o->std_name && o->anim_count &&
        o->anim_at && o->anim_name && o->anim_text && o->anim_controller && o->anim_owner &&
        o->anim_container && o->anim_target && o->anim_start && o->ustr_new && o->ustr_len &&
        o->ustr_sub && o->ustr_equal && o->ustr_free && o->parse_duration;
}
int oa_svg_list_start_animation(const struct oa_svg_list_anim_ops *o,void *list,void *manager,float *started)
{
    if(!list || !manager || !started || !ready(o))return EINVAL;
    void *ctx=o->context;
    if(!o->anim_enabled(ctx,list))return 0;
    void *row=o->item_row(ctx,manager,o->selected_item(ctx,manager));
    if(!row)return 0;
    uint32_t rect[4],cur[4];o->get_rect(ctx,row,rect);o->read_cur(ctx,list,cur);
    if(rect[0]==cur[0] && rect[1]==cur[1] && rect[2]==cur[2] && rect[3]==cur[3])return 0;
    o->write_prev(ctx,list,cur);o->write_cur(ctx,list,rect);
    void *standard=o->std_name(ctx,list,55);
    if(!standard)return ENOMEM;
    int error=0;void *container=o->anim_container(ctx,list);
    if(!container) { error=ENODATA;goto release; }
    uint32_t count=o->anim_count(ctx,container);
    for(uint32_t i=0;count<=INT32_MAX && i<count;++i) {
        void *anim=o->anim_at(ctx,container,i);
        if(!anim)continue;
        const char *borrowed=o->anim_name(ctx,anim);
        void *name=o->ustr_new(ctx,borrowed);
        if(!name) { error=ENOMEM;break; }
        void *sub=o->ustr_sub(ctx,name,1,o->ustr_len(ctx,name)-UINT32_C(1));
        if(!sub) { o->ustr_free(ctx,name);error=ENOMEM;break; }
        int match=o->ustr_equal(ctx,sub,standard);
        float value=0.0f;
        if(match) {
            const char *text=o->anim_text(ctx,anim);
            if(!text)error=ENODATA;
            else value=(float)o->parse_duration(ctx,text)*1000.0f;
        }
        o->ustr_free(ctx,sub);o->ustr_free(ctx,name);
        if(error)break;
        if(match) {
            void *owner=o->anim_owner(ctx,list);
            void *controller=owner?o->anim_controller(ctx,owner):NULL;
            void *target=o->anim_target(ctx,list);
            if(!controller || !target) { error=ENODATA;break; }
            o->anim_start(ctx,controller,target,value);*started=value;break;
        }
    }
release:
    o->ustr_free(ctx,standard);return error;
}
