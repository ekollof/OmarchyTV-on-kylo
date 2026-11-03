#include "openaurora/svg_list_anim.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
static int ready(const struct oa_svg_list_anim_ops *o)
{
    if(!o || !o->anim_enabled || !o->selected_item || !o->item_row || !o->get_rect ||
       !o->read_cur || !o->write_prev || !o->write_cur || !o->std_name || !o->anim_count ||
       !o->anim_at || !o->anim_name || !o->anim_text || !o->anim_controller || !o->anim_owner ||
       !o->anim_container || !o->anim_target || !o->anim_start || !o->ustr_new || !o->ustr_len ||
       !o->ustr_sub || !o->ustr_equal || !o->ustr_free)return EINVAL;
    return 0;
}
int oa_svg_list_start_animation(const struct oa_svg_list_anim_ops *o,void *list,void *manager,float *started)
{
    uint32_t item,rect[4],cur[4],count,i;void *row;const char *stdname;
    if(!list || !manager || !started || ready(o))return EINVAL;
    if(!o->anim_enabled(list))return 0;
    item=o->selected_item(manager);
    row=o->item_row(manager,item);
    if(!row)return 0;
    o->get_rect(row,rect);
    o->read_cur(list,cur);
    if(rect[0]==cur[0] && rect[1]==cur[1] && rect[2]==cur[2] && rect[3]==cur[3])return 0;
    o->write_prev(list,cur);
    o->write_cur(list,rect);
    stdname=o->std_name(list,UINT32_C(55));
    {
        void *container=o->anim_container(list);
        count=o->anim_count(container);
        for(i=0;i<count;++i) {
            void *anim=o->anim_at(container,i);
            const char *name,*sub;uint32_t length;double duration;
            if(!anim)continue;
            name=o->anim_name(anim);
            o->ustr_new(name);
            length=o->ustr_len(name);
            sub=o->ustr_sub(name,UINT32_C(1),length-UINT32_C(1));
            if(!o->ustr_equal(stdname,sub)) { o->ustr_free(sub);o->ustr_free(name);continue; }
            o->ustr_free(sub);o->ustr_free(name);
            duration=atof(o->anim_text(anim));
            *started=(float)duration*1000.0f;
            {
                void *controller=o->anim_controller(o->anim_owner(list));
                o->anim_start(controller,o->anim_target(list),*started);
            }
            o->ustr_free(stdname);
            return 0;
        }
        o->ustr_free(stdname);
    }
    return 0;
}
