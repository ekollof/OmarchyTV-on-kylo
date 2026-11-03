#include "openaurora/settings_row_render.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
/* Rectangle keys are shipped rodata ("alexa" at ELF 0x14be1b, "text1" at
 * 0x1543bbf, "labelLong" at 0x14df0f4, "button" at 0x151cfff); the native
 * reference reads them through its literal pool, so a transcription slip
 * fails the get_rect trace compare. Child offsets, setSVGName slots/IDs,
 * the 0x1b immediate default and the switch bound (type-2 > 0x18) are
 * asserted the same way. Only row_type/row_word/row_flag/row_label/
 * row_button_label/rect_words/get_point/child_at/container_of are pure;
 * every other callback records a trace entry on both sides. */
static const char *key_for(int which)
{
    switch(which) {
    case 0:return "alexa";
    case 1:return "text1";
    case 2:return "labelLong";
    default:return "button";
    }
}
static int ready(const struct oa_setting_row_render_ops *o)
{
    if(!o || !o->base_render || !o->container_of || !o->get_rect || !o->rect_words ||
        !o->row_type || !o->row_word || !o->row_flag || !o->row_label ||
        !o->row_button_label || !o->child_at || !o->is_created || !o->set_name ||
        !o->set_focus || !o->set_elipses || !o->set_font || !o->get_point ||
        !o->set_rect || !o->set_text || !o->create_text ||
        !o->create_button)return EINVAL;
    return 0;
}
static void rect_math(uint32_t out[4],const uint32_t base[4],const int32_t pt[2])
{
    out[0]=base[0]+(uint32_t)pt[0];out[1]=base[1]+(uint32_t)pt[1];
    out[2]=base[2]+(uint32_t)pt[0];out[3]=base[3]+(uint32_t)pt[1];
}
/* Shared row+0x104 tail. The rebuild create passes the stale 0x2dec
 * type-offset word as its label argument (r10 is never reloaded on this
 * path; verified by objdump); only the 0/1 label and button paths carry
 * real label pointers. */
static int tail_104(void *row,const struct oa_setting_row_render_ops *o,void *rect,int key1missing)
{
    void *child=o->child_at(row,UINT32_C(0x104));int created=o->is_created(child);
    uint32_t base[4];
    o->rect_words(rect,base);
    if(created) {
        uint32_t moved[4];int32_t pt[2];
        o->get_point(row,pt);rect_math(moved,base,pt);
        o->set_rect(child,moved);o->set_text(child,o->row_label(row));return 0;
    }
    if(key1missing) {
        o->set_name(child,UINT32_C(1),o->row_word(row,UINT32_C(0x2e90)));
        o->set_name(child,UINT32_C(0),o->row_word(row,UINT32_C(0x2e8c)));
        o->set_name(child,UINT32_C(3),o->row_word(row,UINT32_C(0x2e98)));
        o->set_name(child,UINT32_C(2),o->row_word(row,UINT32_C(0x2e94)));
    } else {
        o->set_name(child,UINT32_C(1),UINT32_C(0x26d));
        o->set_name(child,(uint32_t)created,UINT32_C(0x26d));
        o->set_name(child,UINT32_C(3),UINT32_C(0x26d));
        o->set_name(child,UINT32_C(2),UINT32_C(0x26d));
        o->set_font(child,UINT32_C(0x1a));
    }
    o->set_focus(child,0);o->set_elipses(child,0);
    o->create_text(child,0,o->row_label(row),base,UINT32_C(1),row);
    return 0;
}
int oa_setting_row_render(void *row,const struct oa_setting_row_render_ops *o)
{
    void *box,*r1,*r2,*r3;uint32_t type;
    if(!row || ready(o))return EINVAL;
    type=o->row_type(row);
    if(type==UINT32_C(0x1b))return 0;
    o->base_render(row);
    box=o->container_of(row);
    r1=o->get_rect(box,key_for(0));
    r2=o->get_rect(box,key_for(1));
    r3=o->get_rect(box,key_for(2));
    if(type==UINT32_C(4)) {
        void *key4,*button;int created;int flag=o->row_flag(row,UINT32_C(0x2e45));
        /* The rebuild flag diverts through the shared tail when the stored
         * key3 rectangle is present; the tail's create rejoins the switch
         * head, which dispatches case 4 (table[2] is 0xd544a4, read from the
         * ELF, so the key4 lookup below still runs). With a null key3 the
         * switch dispatches case 4 directly. Either way the button body
         * below runs; only the 0x1b/default exits skip it. */
        if(flag && r3)
            tail_104(row,o,r3,r1==NULL);
        else if(!flag && r2)
            tail_104(row,o,r2,r1==NULL);
        key4=o->get_rect(box,key_for(3));
        if(!key4)return 0;
        button=o->child_at(row,UINT32_C(0x684));
        created=o->is_created(button);
        if(created) {
            uint32_t base[4],moved[4];int32_t pt[2];
            o->rect_words(key4,base);o->get_point(row,pt);rect_math(moved,base,pt);
            o->set_rect(button,moved);o->set_text(button,o->row_button_label(row));return 0;
        }
        if(!r1) {
            o->set_name(button,UINT32_C(1),o->row_word(row,UINT32_C(0x2eb0)));
            o->set_name(button,UINT32_C(0),o->row_word(row,UINT32_C(0x2eac)));
            o->set_name(button,UINT32_C(3),o->row_word(row,UINT32_C(0x2eb8)));
            o->set_name(button,UINT32_C(2),o->row_word(row,UINT32_C(0x2eb4)));
        } else {
            o->set_name(button,UINT32_C(1),UINT32_C(0x269));
            o->set_name(button,(uint32_t)created,UINT32_C(0x269));
            o->set_name(button,UINT32_C(3),UINT32_C(0x269));
            o->set_name(button,UINT32_C(2),UINT32_C(0x269));
            o->set_font(button,UINT32_C(0x1e));
        }
        {
            /* The shipped body default-constructs a stack temp, copies the
             * key4 rectangle over it and folds one shared-slot word in when
             * the rebuild flag is set, but the create call below passes the
             * key4 rectangle directly (mov r3,r9 at 0xd54730, objdump
             * verified), so the temp is dead. The fold's loads still execute
             * natively: with the flag set and a null shared slot it faults
             * (ldr r4,[r0,#0x8] at 0xd54726 with r0==0), so that input stays
             * excluded from the sweep. */
            uint32_t base[4];
            o->rect_words(key4,base);
            o->create_button(button,o->row_button_label(row),base,row);
        }
        return 0;
    }
    if(type<UINT32_C(2)) {
        if(!r2)return 0;
        if(type!=UINT32_C(1)) {
            tail_104(row,o,r2,r1==NULL);
            return 0;
        }
        {
            void *child=o->child_at(row,UINT32_C(0x3c4));int created=o->is_created(child);
            if(created) {
                uint32_t base[4],moved[4];int32_t pt[2];
                o->rect_words(r2,base);o->get_point(row,pt);rect_math(moved,base,pt);
                o->set_rect(child,moved);o->set_text(child,o->row_label(row));return 0;
            }
            o->set_name(child,UINT32_C(1),UINT32_C(0x1ec));
            o->set_name(child,(uint32_t)created,UINT32_C(0x1ed));
            o->set_name(child,UINT32_C(3),UINT32_C(0x1ee));
            o->set_name(child,UINT32_C(2),UINT32_C(0x1ef));
            o->set_focus(child,0);o->set_elipses(child,0);
            {
                uint32_t base[4];
                o->rect_words(r2,base);
                o->create_text(child,0,o->row_label(row),base,type,row);
            }
            return 0;
        }
    }
    if(!r2) {
        /* Key2rect missing: the switch dispatches at once. */
        if(type<UINT32_C(2) || type>UINT32_C(26))return 0;
        return ENOTSUP;
    }
    if(type!=UINT32_C(1) && type!=UINT32_C(0)) {
        /* The shared 0x104 tail runs for every other non-4 type with a key2
         * rectangle (objdump-verified fallthrough); only 0x1b exits before
         * it. The 0x1a sub-branch and the switch case bodies stay open. */
        if(type==UINT32_C(0x1a))return ENOTSUP;
        if(type>UINT32_C(26)) {
            tail_104(row,o,r2,r1==NULL);
            return 0;
        }
        tail_104(row,o,r2,r1==NULL);
        return ENOTSUP;
    }
    /* Type 0 already returned through the shared tail above. */
    return 0;
}
