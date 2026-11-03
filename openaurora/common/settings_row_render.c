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
    case 3:return "button";
    case 4:return "checkbox";
    default:return "selectionbox";
    }
}
static int ready(const struct oa_setting_row_render_ops *o)
{
    if(!o || !o->base_render || !o->container_of || !o->get_rect || !o->rect_words ||
        !o->row_type || !o->row_word || !o->row_flag || !o->row_label ||
        !o->row_button_label || !o->child_at || !o->is_created || !o->set_name ||
        !o->set_focus || !o->set_elipses || !o->set_font || !o->get_point ||
        !o->set_rect || !o->set_text || !o->create_text ||
        !o->create_button || !o->set_text_alignment || !o->set_checked ||
        !o->create_checkbox || !o->set_selected_item || !o->create_selection ||
        !o->set_selection_image || !o->set_entry_mode || !o->set_max_chars ||
        !o->set_cursor_auto || !o->set_numeric_contents ||
        !o->create_text_entry)return EINVAL;
    return 0;
}
static void rect_math(uint32_t out[4],const uint32_t base[4],const int32_t pt[2])
{
    out[0]=base[0]+(uint32_t)pt[0];out[1]=base[1]+(uint32_t)pt[1];
    out[2]=base[2]+(uint32_t)pt[0];out[3]=base[3]+(uint32_t)pt[1];
}
/* Shared row+0x104 tail. The rebuild create passes the row label (reloaded
 * into sl at 0xd565ce; objdump-verified). Type 0x1a takes a distinct branch
 * that writes 0x2e8c into all four name slots and skips setFont before the
 * shared focus/elipses/create. */
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
    if(o->row_type(row)==UINT32_C(0x1a)) {
        o->set_name(child,UINT32_C(1),o->row_word(row,UINT32_C(0x2e8c)));
        o->set_name(child,UINT32_C(0),o->row_word(row,UINT32_C(0x2e8c)));
        o->set_name(child,UINT32_C(3),o->row_word(row,UINT32_C(0x2e8c)));
        o->set_name(child,UINT32_C(2),o->row_word(row,UINT32_C(0x2e8c)));
    } else if(key1missing) {
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
/* Case 0x16: drive the row+0x27a8 checkbox from the "checkbox" rectangle.
 * Uncreated writes names 9/10/11/12 into slots 1/0/3/2, alignment 5, then
 * creates from an empty label with the (0,0x74,0x75) triplet and sets the
 * checked byte 0x2e44; created applies EASIRect math plus setWindowRect,
 * setText(empty) and the same checked byte. Both paths end with setChecked. */
static int case_0x16(void *row,const struct oa_setting_row_render_ops *o,void *box)
{
    void *r=o->get_rect(box,key_for(4)),*child;int created;uint32_t base[4];
    if(!r)return 0;
    child=o->child_at(row,UINT32_C(0x27a8));
    created=o->is_created(child);
    o->rect_words(r,base);
    if(created) {
        uint32_t moved[4];int32_t pt[2];
        o->get_point(row,pt);rect_math(moved,base,pt);
        o->set_rect(child,moved);o->set_text(child,"");
        o->set_checked(child,o->row_flag(row,UINT32_C(0x2e44))?1u:0u);
        return 0;
    }
    o->set_name(child,UINT32_C(1),UINT32_C(9));
    o->set_name(child,(uint32_t)created,UINT32_C(10));
    o->set_name(child,UINT32_C(3),UINT32_C(11));
    o->set_name(child,UINT32_C(2),UINT32_C(12));
    o->set_text_alignment(child,UINT32_C(5));
    o->create_checkbox(child,0,UINT32_C(0x74),UINT32_C(0x75),"",base,row);
    o->set_checked(child,o->row_flag(row,UINT32_C(0x2e44))?1u:0u);
    return 0;
}
/* Cases 0x2 and 0x3: drive the row+0x1014 selection box from the
 * "selectionbox" rectangle. Case 0x3 first calls the row's
 * setSelectionBoxImageId with [row+0x2e54]. Created applies EASIRect math,
 * setWindowRect and setSelectedItemWithId([row+0x2de8]); uncreated writes
 * the four names (0x26b+font for case 2 with key1, else 0x2e9c/0x2ea0/0x2ea4/
 * 0x2ea8) in slot order 1/3/0/2 and creates from an empty label with the
 * (0,0xc,0xd) triplet. */
static int case_sel(void *row,const struct oa_setting_row_render_ops *o,void *box,uint32_t type,void *alexa)
{
    void *r,*child;int created;uint32_t base[4];
    r=o->get_rect(box,key_for(5));
    if(type==UINT32_C(3))o->set_selection_image(row,o->row_word(row,UINT32_C(0x2e54)));
    if(!r)return 0;
    child=o->child_at(row,UINT32_C(0x1014));
    created=o->is_created(child);
    o->rect_words(r,base);
    if(created) {
        uint32_t moved[4];int32_t pt[2];
        o->get_point(row,pt);rect_math(moved,base,pt);
        o->set_rect(child,moved);
        o->set_selected_item(child,o->row_word(row,UINT32_C(0x2de8)));
        return 0;
    }
    if(type==UINT32_C(2) && alexa!=NULL) {
        o->set_name(child,UINT32_C(1),UINT32_C(0x26b));
        o->set_name(child,UINT32_C(3),UINT32_C(0x26b));
        o->set_name(child,(uint32_t)created,UINT32_C(0x26b));
        o->set_name(child,UINT32_C(2),UINT32_C(0x26b));
        o->set_font(child,UINT32_C(0x1e));
    } else {
        o->set_name(child,UINT32_C(1),o->row_word(row,UINT32_C(0x2ea0)));
        o->set_name(child,UINT32_C(3),o->row_word(row,UINT32_C(0x2ea8)));
        o->set_name(child,(uint32_t)created,o->row_word(row,UINT32_C(0x2e9c)));
        o->set_name(child,UINT32_C(2),o->row_word(row,UINT32_C(0x2ea4)));
    }
    o->create_selection(child,0,UINT32_C(0xc),UINT32_C(0xd),"",base,row);
    return 0;
}
/* Case 0x1a: drive the row+0x684 button from the "button" rectangle. Not
 * created writes 0x2ed0/0x2ecc/0x2ed8/0x2ed4 into the four name slots then
 * creates; created applies EASIRect math plus setWindowRect/setText. */
/* Case 0xd: drive the row+0x18d8 text-entry box from the "selectionbox"
 * rectangle. Uncreated writes row[0x2ec0]/[0x2ebc]/[0x2ec8]/[0x2ec4] into
 * slots 1/0/3/2, creates from an empty label with the (0,0xc,0xd) triplet,
 * then sets entry mode 0, max chars 4 and automatic cursor movement.
 * Created applies EASIRect math, setWindowRect and setNumericContents from
 * row[0x2e00] before the same three trailing setters. */
static int case_0xd(void *row,const struct oa_setting_row_render_ops *o,void *box)
{
    void *r=o->get_rect(box,key_for(5)),*child;int created;uint32_t base[4];
    if(!r)return 0;
    child=o->child_at(row,UINT32_C(0x18d8));
    created=o->is_created(child);
    o->rect_words(r,base);
    if(created) {
        uint32_t moved[4];int32_t pt[2];
        o->get_point(row,pt);rect_math(moved,base,pt);
        o->set_rect(child,moved);
        o->set_numeric_contents(child,o->row_word(row,UINT32_C(0x2e00)));
    } else {
        o->set_name(child,UINT32_C(1),o->row_word(row,UINT32_C(0x2ec0)));
        o->set_name(child,(uint32_t)created,o->row_word(row,UINT32_C(0x2ebc)));
        o->set_name(child,UINT32_C(3),o->row_word(row,UINT32_C(0x2ec8)));
        o->set_name(child,UINT32_C(2),o->row_word(row,UINT32_C(0x2ec4)));
        o->create_text_entry(child,0,UINT32_C(0xc),UINT32_C(0xd),"",base,row);
    }
    o->set_entry_mode(child,0);
    o->set_max_chars(child,UINT32_C(4));
    o->set_cursor_auto(child,1);
    return 0;
}
static int case_0x1a(void *row,const struct oa_setting_row_render_ops *o,void *box)
{
    void *r=o->get_rect(box,key_for(3)),*button;int created;uint32_t base[4];
    if(!r)return 0;
    button=o->child_at(row,UINT32_C(0x684));
    created=o->is_created(button);
    o->rect_words(r,base);
    if(created) {
        uint32_t moved[4];int32_t pt[2];
        o->get_point(row,pt);rect_math(moved,base,pt);
        o->set_rect(button,moved);o->set_text(button,o->row_button_label(row));return 0;
    }
    o->set_name(button,UINT32_C(1),o->row_word(row,UINT32_C(0x2ed0)));
    o->set_name(button,(uint32_t)created,o->row_word(row,UINT32_C(0x2ecc)));
    o->set_name(button,UINT32_C(3),o->row_word(row,UINT32_C(0x2ed8)));
    o->set_name(button,UINT32_C(2),o->row_word(row,UINT32_C(0x2ed4)));
    o->create_button(button,o->row_button_label(row),base,row);
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
        /* Key2rect missing: the switch dispatches at once. Type 0x1a still
         * runs its button case body. */
        if(type<UINT32_C(2) || type>UINT32_C(26))return 0;
        if(type==UINT32_C(0x1a)) {
            case_0x1a(row,o,box);
            return 0;
        }
        if(type==UINT32_C(0x16)) {
            case_0x16(row,o,box);
            return 0;
        }
        if(type==UINT32_C(2) || type==UINT32_C(3)) {
            case_sel(row,o,box,type,r1);
            return 0;
        }
        if(type==UINT32_C(0xd)) {
            case_0xd(row,o,box);
            return 0;
        }
        return ENOTSUP;
    }
    if(type!=UINT32_C(1) && type!=UINT32_C(0)) {
        /* The shared 0x104 tail runs for every other non-4 type with a key2
         * rectangle (objdump-verified fallthrough); only 0x1b exits before
         * it. Type 0x1a then runs its case body (button); the remaining case
         * bodies 2..25 stay open. */
        if(type>UINT32_C(26)) {
            tail_104(row,o,r2,r1==NULL);
            return 0;
        }
        tail_104(row,o,r2,r1==NULL);
        if(type==UINT32_C(0x1a)) {
            case_0x1a(row,o,box);
            return 0;
        }
        if(type==UINT32_C(0x16)) {
            case_0x16(row,o,box);
            return 0;
        }
        if(type==UINT32_C(2) || type==UINT32_C(3)) {
            case_sel(row,o,box,type,r1);
            return 0;
        }
        if(type==UINT32_C(0xd)) {
            case_0xd(row,o,box);
            return 0;
        }
        return ENOTSUP;
    }
    /* Type 0 already returned through the shared tail above. */
    return 0;
}
