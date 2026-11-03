#include "openaurora/settings_row_render.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
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
    case 5:return "selectionbox";
    case 6:return "image1";
    case 7:return "image2";
    case 8:return "banner";
    case 9:return "slidebar";
    case 10:return "text2";
    case 11:return "text3";
    default:return "text4";
    }
}
static int ready(const struct oa_setting_row_render_ops *o)
{
    if(!o || !o->base_render || !o->container_of || !o->get_rect || !o->rect_words ||
        !o->row_type || !o->row_word || !o->row_flag || !o->row_label ||
        !o->row_button_label || !o->row_value_text || !o->row_ustring || !o->child_at || !o->is_created || !o->set_name ||
        !o->set_focus || !o->set_elipses || !o->set_font || !o->get_point ||
        !o->set_rect || !o->set_text || !o->create_text ||
        !o->create_button || !o->set_text_alignment || !o->set_checked ||
        !o->create_checkbox || !o->set_selected_item || !o->create_selection ||
        !o->set_selection_image || !o->set_entry_mode || !o->set_max_chars ||
        !o->set_cursor_auto || !o->set_numeric_contents ||
        !o->create_text_entry || !o->store_row_word || !o->destroy_child ||
        !o->reset_cached_xml || !o->glyph_id || !o->set_glyph ||
        !o->set_highlight || !o->set_sublauncher_highlight || !o->set_shadow ||
        !o->set_visible || !o->set_scaling || !o->create_image ||
        !o->set_draw_on_top || !o->create_textbox ||
        !o->create_slider || !o->set_slider_value || !o->set_slider_focus ||
        !o->set_slider_range)return EINVAL;
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
/* Text-entry cases 0xd/0xe/0xf/0x10: drive row+0x18d8 from the
 * "selectionbox" rectangle. Uncreated writes row[0x2ec0]/[0x2ebc]/[0x2ec8]/
 * [0x2ec4] into slots 1/0/3/2 and creates from an empty label with the
 * (0,0xc,0xd) triplet; created applies EASIRect math and setWindowRect, then
 * either setNumericContents(row[0x2e00]) (0xd/0xe/0xf) or
 * setText(row[0x2e04]) (0x10). All four finish with the case's entry mode,
 * max-chars and automatic cursor movement: 0xd mode0/4, 0xe and 0xf
 * mode0xa/5, 0x10 mode1/10. */
static int case_te(void *row,const struct oa_setting_row_render_ops *o,void *box,
    uint32_t mode,uint32_t maxchars,int use_text)
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
        if(use_text)o->set_text(child,o->row_value_text(row));
        else o->set_numeric_contents(child,o->row_word(row,UINT32_C(0x2e00)));
    } else {
        o->set_name(child,UINT32_C(1),o->row_word(row,UINT32_C(0x2ec0)));
        o->set_name(child,(uint32_t)created,o->row_word(row,UINT32_C(0x2ebc)));
        o->set_name(child,UINT32_C(3),o->row_word(row,UINT32_C(0x2ec8)));
        o->set_name(child,UINT32_C(2),o->row_word(row,UINT32_C(0x2ec4)));
        o->create_text_entry(child,0,UINT32_C(0xc),UINT32_C(0xd),"",base,row);
    }
    o->set_entry_mode(child,mode);
    o->set_max_chars(child,maxchars);
    o->set_cursor_auto(child,1);
    return 0;
}
/* Cases 0x5/0x6: the launcher button plus two images. Three rectangle
 * lookups ("button"/"image1"/"image2"); the button body either refreshes
 * (created and no rebuild flag) or destroys, bulk-initialises and recreates
 * with the 0x2e70/0x2e6c/0x2e74/0x2e78 (type 5) or
 * 0x2e80/0x2e7c/0x2e84/0x2e88 (type 6) names, four glyph ids from
 * glyph_id(row[0x2dd4], state), highlight row[0x2e68], sublauncher 0x237,
 * elipses 1 and shadow 0x94. The common tail stores 0x92/0x93 into
 * row[0x2e4c]/[0x2e50] and drives the row+0x2b28 / row+0x2c0c images from
 * their rectangles before setDrawOnTop(1). The rebuild path reloads the
 * image1 rectangle (faults natively when it is null, so excluded). */
static int case_btn56(void *row,const struct oa_setting_row_render_ops *o,void *box,uint32_t type,
    void *button_rect,void *image1,void *image2)
{
    void *button;int created;uint32_t base[4];
    (void)box;
    if(!button_rect)return 0;
    button=o->child_at(row,UINT32_C(0x684));
    created=o->is_created(button);
    o->rect_words(button_rect,base);
    if(created && !o->row_flag(row,UINT32_C(0x2e49))) {
        uint32_t moved[4];int32_t pt[2];
        o->get_point(row,pt);rect_math(moved,base,pt);
        o->set_rect(button,moved);o->set_text(button,o->row_button_label(row));
    } else {
        if(o->is_created(button))o->destroy_child(button);
        if(o->row_flag(row,UINT32_C(0x2e49)))o->reset_cached_xml(button);
        if(type==UINT32_C(6)) {
            o->set_name(button,UINT32_C(1),o->row_word(row,UINT32_C(0x2e80)));
            o->set_name(button,UINT32_C(3),o->row_word(row,UINT32_C(0x2e88)));
            o->set_name(button,(uint32_t)created,o->row_word(row,UINT32_C(0x2e7c)));
            o->set_name(button,UINT32_C(2),o->row_word(row,UINT32_C(0x2e84)));
        } else {
            o->set_name(button,UINT32_C(1),o->row_word(row,UINT32_C(0x2e70)));
            o->set_name(button,UINT32_C(3),o->row_word(row,UINT32_C(0x2e78)));
            o->set_name(button,(uint32_t)created,o->row_word(row,UINT32_C(0x2e6c)));
            o->set_name(button,UINT32_C(2),o->row_word(row,UINT32_C(0x2e74)));
        }
        {
            uint32_t op=o->row_word(row,UINT32_C(0x2dd4));
            o->set_glyph(button,o->glyph_id(op,UINT32_C(1)),UINT32_C(1));
            o->set_glyph(button,o->glyph_id(op,0),0);
            o->set_glyph(button,o->glyph_id(op,UINT32_C(2)),UINT32_C(2));
            o->set_glyph(button,o->glyph_id(op,UINT32_C(3)),UINT32_C(3));
        }
        o->set_highlight(button,o->row_word(row,UINT32_C(0x2e68)));
        o->set_sublauncher_highlight(button,UINT32_C(0x237));
        o->set_elipses(button,1);
        o->set_shadow(button,UINT32_C(0x94));
        if(o->row_flag(row,UINT32_C(0x2e45)))o->set_text_alignment(button,UINT32_C(4));
        o->create_button(button,o->row_word(row,UINT32_C(0x2dd4)),
            o->row_button_label(row),base,row);
    }
    o->store_row_word(row,UINT32_C(0x2e4c),UINT32_C(0x92));
    o->store_row_word(row,UINT32_C(0x2e50),UINT32_C(0x93));
    if(image1) {
        void *im=o->child_at(row,UINT32_C(0x2b28));
        uint32_t w[4];int imc=o->is_created(im);
        o->rect_words(image1,w);
        if(imc) {
            uint32_t moved[4];int32_t pt[2];
            o->get_point(row,pt);rect_math(moved,w,pt);
            o->set_rect(im,moved);o->set_visible(im,0);
        } else {
            uint32_t scale=o->row_word(row,UINT32_C(0x2e4c));
            /* The shipped body passes the stale isCreated result (0 here) to
             * setScalingType and the row word only to create (objdump). */
            if(scale) { o->set_scaling(im,(uint32_t)imc);o->create_image(im,scale,w,row); }
        }
    }
    if(image2) {
        void *im=o->child_at(row,UINT32_C(0x2c0c));
        uint32_t w[4];int imc=o->is_created(im);
        o->rect_words(image2,w);
        if(imc) {
            uint32_t moved[4];int32_t pt[2];
            o->get_point(row,pt);rect_math(moved,w,pt);
            o->set_rect(im,moved);o->set_visible(im,0);
        } else {
            uint32_t scale=o->row_word(row,UINT32_C(0x2e50));
            if(scale) { o->set_scaling(im,(uint32_t)imc);o->create_image(im,scale,w,row); }
        }
    }
    o->set_draw_on_top(button,1);
    return 0;
}
/* Case 0x7: the banner-button variant. Looks up "banner", drives the
 * row+0x684 button, and either refreshes it (created: EASIRect math plus
 * setWindowRect only, no setText) or writes the type-5 names into slots
 * 1/3/0/2, four glyph ids from getImageId(row[0x2dd4], state), elipses 1 and
 * shadow 0x94, then creates with icon row[0x2dd4] and finally sets draw-on-top.
 * The rebuild path folds the text1 rectangle's first word into the temp, so a
 * null text1 with the 0x2e45 flag faults natively. */
static int case_btn7(void *row,const struct oa_setting_row_render_ops *o,void *box)
{
    void *r=o->get_rect(box,key_for(8)),*button;int created;uint32_t base[4];
    if(!r)return 0;
    button=o->child_at(row,UINT32_C(0x684));
    created=o->is_created(button);
    o->rect_words(r,base);
    if(created) {
        uint32_t moved[4];int32_t pt[2];
        o->get_point(row,pt);rect_math(moved,base,pt);
        o->set_rect(button,moved);
    } else {
        uint32_t op=o->row_word(row,UINT32_C(0x2dd4));
        o->set_name(button,UINT32_C(1),o->row_word(row,UINT32_C(0x2e70)));
        o->set_name(button,UINT32_C(3),o->row_word(row,UINT32_C(0x2e78)));
        o->set_name(button,(uint32_t)created,o->row_word(row,UINT32_C(0x2e6c)));
        o->set_name(button,UINT32_C(2),o->row_word(row,UINT32_C(0x2e74)));
        o->set_glyph(button,o->glyph_id(op,UINT32_C(1)),UINT32_C(1));
        o->set_glyph(button,o->glyph_id(op,0),0);
        o->set_glyph(button,o->glyph_id(op,UINT32_C(2)),UINT32_C(2));
        o->set_glyph(button,o->glyph_id(op,UINT32_C(3)),UINT32_C(3));
        o->set_elipses(button,1);
        o->set_shadow(button,UINT32_C(0x94));
        if(o->row_flag(row,UINT32_C(0x2e45)))o->set_text_alignment(button,UINT32_C(4));
        o->create_button(button,o->row_word(row,UINT32_C(0x2dd4)),
            o->row_button_label(row),base,row);
    }
    o->set_draw_on_top(button,1);
    return 0;
}
/* Case 0x8: the plain button. Looks up "button", drives the row+0x684
 * child; created applies EASIRect math, setWindowRect and setText, otherwise
 * writes row[0x2eb0]/[0x2eac]/[0x2eb8]/[0x2eb4] into name slots 1/0/3/2 and
 * creates with icon 0. No images, glyphs, stores or draw-on-top. */
static int case_btn8(void *row,const struct oa_setting_row_render_ops *o,void *box)
{
    void *r=o->get_rect(box,key_for(3)),*button;int created;uint32_t base[4];
    if(!r)return 0;
    button=o->child_at(row,UINT32_C(0x684));
    created=o->is_created(button);
    o->rect_words(r,base);
    if(created) {
        uint32_t moved[4];int32_t pt[2];
        o->get_point(row,pt);rect_math(moved,base,pt);
        o->set_rect(button,moved);o->set_text(button,o->row_button_label(row));
        return 0;
    }
    o->set_name(button,UINT32_C(1),o->row_word(row,UINT32_C(0x2eb0)));
    o->set_name(button,(uint32_t)created,o->row_word(row,UINT32_C(0x2eac)));
    o->set_name(button,UINT32_C(3),o->row_word(row,UINT32_C(0x2eb8)));
    o->set_name(button,UINT32_C(2),o->row_word(row,UINT32_C(0x2eb4)));
    o->create_button(button,0,o->row_button_label(row),base,row);
    return 0;
}
/* Case 0x9: slider row. Looks up an unnamed text rectangle and a slider
 * rectangle. The row+0x1ca8 text box is created with the char* empty label and
 * extra 1 (or refreshed via setWindowRect) and always receives the formatted
 * row[0x2df8] value; uncreated texts use 0x26d (key1 present) or
 * 0x1e8/0x1e9/0x1ea/0x1eb names and font 0x1e. The row+0x13c0 slider is
 * created with row[0x2df0]/row[0x2df4] as range and then setSliderValue from
 * row[0x2df8]; uncreated sliders use 0x26c names and setFocus(0). */
static int case_slider(void *row,const struct oa_setting_row_render_ops *o,void *box,
    void *text_rect,void *slider_rect,void *alexa)
{
    (void)box;
    if(text_rect) {
        void *tb=o->child_at(row,UINT32_C(0x1ca8));int created=o->is_created(tb);
        uint32_t base[4];o->rect_words(text_rect,base);
        if(created) {
            uint32_t moved[4];int32_t pt[2];
            o->get_point(row,pt);rect_math(moved,base,pt);
            o->set_rect(tb,moved);
        } else {
            if(alexa) {
                o->set_name(tb,UINT32_C(1),UINT32_C(0x26d));
                o->set_name(tb,(uint32_t)created,UINT32_C(0x26d));
                o->set_name(tb,UINT32_C(3),UINT32_C(0x26d));
                o->set_name(tb,UINT32_C(2),UINT32_C(0x26d));
                o->set_font(tb,UINT32_C(0x1e));
            } else {
                o->set_name(tb,UINT32_C(1),UINT32_C(0x1e8));
                o->set_name(tb,(uint32_t)created,UINT32_C(0x1e9));
                o->set_name(tb,UINT32_C(3),UINT32_C(0x1ea));
                o->set_name(tb,UINT32_C(2),UINT32_C(0x1eb));
            }
            o->set_focus(tb,0);o->set_elipses(tb,0);
            o->create_textbox(tb,0,"",base,UINT32_C(1),row);
        }
        { char text[16];int32_t v=(int32_t)o->row_word(row,UINT32_C(0x2df8));
          snprintf(text,sizeof text,"%d",(int)v);o->set_text(tb,text); }
    }
    if(!slider_rect)return 0;
    {
        void *sb=o->child_at(row,UINT32_C(0x13c0));int created=o->is_created(sb);
        uint32_t base[4];o->rect_words(slider_rect,base);
        if(created) {
            uint32_t moved[4];int32_t pt[2];
            o->get_point(row,pt);rect_math(moved,base,pt);
            o->set_rect(sb,moved);
            o->set_slider_range(sb,(int32_t)o->row_word(row,UINT32_C(0x2df0)),
                (int32_t)o->row_word(row,UINT32_C(0x2df4)));
            o->set_slider_value(sb,(int32_t)o->row_word(row,UINT32_C(0x2df8)));
        } else {
            if(alexa) {
                o->set_name(sb,UINT32_C(1),UINT32_C(0x26c));
                o->set_name(sb,(uint32_t)created,UINT32_C(0x26c));
                o->set_name(sb,UINT32_C(3),UINT32_C(0x26c));
                o->set_name(sb,UINT32_C(2),UINT32_C(0x26c));
            }
            o->set_slider_focus(sb,0);
            o->create_slider(sb,0,(int32_t)o->row_word(row,UINT32_C(0x2df0)),
                (int32_t)o->row_word(row,UINT32_C(0x2df4)),base,row);
            o->set_slider_value(sb,(int32_t)o->row_word(row,UINT32_C(0x2df8)));
        }
    }
    return 0;
}
/* Case 0xa: the second slider row. Same "slidebar"/"text2" lookups as 0x9,
 * but the row+0x1ca8 text box always uses the 0x1e8/0x1e9/0x1ea/0x1eb names
 * in slots 1/0/3/2 with no font, and the row+0x1498 slide bar uses
 * 0x214/0x216/0x215/0x217 in slots 1/0/3/2. Both children refresh with
 * EASIRect math and the formatted row[0x2df8] value; uncreated sliders are
 * created from row[0x2df0]/[0x2df4], created ones get setSliderRangeValues. */
static int case_slider2(void *row,const struct oa_setting_row_render_ops *o,void *box,
    void *text_rect,void *slider_rect)
{
    (void)box;
    if(text_rect) {
        void *tb=o->child_at(row,UINT32_C(0x1ca8));int created=o->is_created(tb);
        uint32_t base[4];o->rect_words(text_rect,base);
        if(created) {
            uint32_t moved[4];int32_t pt[2];
            o->get_point(row,pt);rect_math(moved,base,pt);
            o->set_rect(tb,moved);
        } else {
            o->set_name(tb,UINT32_C(1),UINT32_C(0x1e8));
            o->set_name(tb,(uint32_t)created,UINT32_C(0x1e9));
            o->set_name(tb,UINT32_C(3),UINT32_C(0x1ea));
            o->set_name(tb,UINT32_C(2),UINT32_C(0x1eb));
            o->set_focus(tb,0);o->set_elipses(tb,0);
            o->create_textbox(tb,0,"",base,UINT32_C(1),row);
        }
        { char text[16];int32_t v=(int32_t)o->row_word(row,UINT32_C(0x2df8));
          snprintf(text,sizeof text,"%d",(int)v);o->set_text(tb,text); }
    }
    if(!slider_rect)return 0;
    {
        void *sb=o->child_at(row,UINT32_C(0x1498));int created=o->is_created(sb);
        uint32_t base[4];o->rect_words(slider_rect,base);
        if(created) {
            uint32_t moved[4];int32_t pt[2];
            o->get_point(row,pt);rect_math(moved,base,pt);
            o->set_rect(sb,moved);
            o->set_slider_range(sb,(int32_t)o->row_word(row,UINT32_C(0x2df0)),
                (int32_t)o->row_word(row,UINT32_C(0x2df4)));
        } else {
            o->set_name(sb,UINT32_C(1),UINT32_C(0x214));
            o->set_name(sb,(uint32_t)created,UINT32_C(0x216));
            o->set_name(sb,UINT32_C(3),UINT32_C(0x215));
            o->set_name(sb,UINT32_C(2),UINT32_C(0x217));
            o->set_slider_focus(sb,0);
            o->create_slider(sb,0,(int32_t)o->row_word(row,UINT32_C(0x2df0)),
                (int32_t)o->row_word(row,UINT32_C(0x2df4)),base,row);
        }
        o->set_slider_value(sb,(int32_t)o->row_word(row,UINT32_C(0x2df8)));
    }
    return 0;
}
/* Case 0xb: slider bar with two images. Looks up "slidebar", "image1" and
 * "image2". When both image rectangles exist it either refreshes both (both
 * children created: plain setWindowRect with the rectangle words, no point
 * offset) or creates each from its row[0x2e4c]/[0x2e50] scaling word. Note
 * the shipped body falls through to the create block when only the first
 * child is created, so it can re-create an existing image; that is
 * reproduced. Then the row+0x1498 slide bar is refreshed (rect + range +
 * value) or created from row[0x2df0]/[0x2df4] with the 0x214/0x216/0x215/
 * 0x217 names. */
static int case_0xb(void *row,const struct oa_setting_row_render_ops *o,void *box)
{
    void *sb=o->get_rect(box,key_for(9));
    void *i1=o->get_rect(box,key_for(6));
    void *i2=o->get_rect(box,key_for(7));
    int done_images=0;
    if(i1 && i2) {
        void *im1=o->child_at(row,UINT32_C(0x2b28));
        int c1=o->is_created(im1);
        if(c1) {
            void *im2=o->child_at(row,UINT32_C(0x2c0c));
            if(o->is_created(im2)) {
                uint32_t w1[4],w2[4];
                o->rect_words(i1,w1);o->rect_words(i2,w2);
                o->set_rect(im1,w1);o->set_rect(im2,w2);
                done_images=1;
            }
        }
        if(!done_images) {
            void *im2=o->child_at(row,UINT32_C(0x2c0c));
            uint32_t w1[4],w2[4],sc;
            o->rect_words(i1,w1);o->rect_words(i2,w2);
            sc=o->row_word(row,UINT32_C(0x2e4c));
            if(sc)o->create_image(im1,sc,w1,row);
            sc=o->row_word(row,UINT32_C(0x2e50));
            if(sc)o->create_image(im2,sc,w2,row);
        }
    }
    if(!sb)return 0;
    {
        void *sl=o->child_at(row,UINT32_C(0x1498));int created=o->is_created(sl);
        uint32_t base[4];o->rect_words(sb,base);
        if(created) {
            uint32_t moved[4];int32_t pt[2];
            o->get_point(row,pt);rect_math(moved,base,pt);
            o->set_rect(sl,moved);
            o->set_slider_range(sl,(int32_t)o->row_word(row,UINT32_C(0x2df0)),
                (int32_t)o->row_word(row,UINT32_C(0x2df4)));
        } else {
            o->set_name(sl,UINT32_C(1),UINT32_C(0x214));
            o->set_name(sl,(uint32_t)created,UINT32_C(0x216));
            o->set_name(sl,UINT32_C(3),UINT32_C(0x215));
            o->set_name(sl,UINT32_C(2),UINT32_C(0x217));
            o->set_slider_focus(sl,0);
            o->create_slider(sl,0,(int32_t)o->row_word(row,UINT32_C(0x2df0)),
                (int32_t)o->row_word(row,UINT32_C(0x2df4)),base,row);
        }
        o->set_slider_value(sl,(int32_t)o->row_word(row,UINT32_C(0x2df8)));
    }
    return 0;
}
/* Case 0xc: text2/text3/text4 plus the shared slidebar. text2 drives the
 * row+0x1ca8 box with the formatted row[0x2df8] value; text3 and text4 (both
 * rectangles required) drive row+0x1f68 and row+0x2228 from the row UStrings
 * at 0x2e2c/0x2e34, with text alignment 6 applied only to the 0x1f68 box.
 * Only the 0x1f68 child's isCreated is consulted, so a created 0x1f68 with an
 * uncreated 0x2228 still takes the refresh path (firmware quirk reproduced).
 * The slidebar then refreshes (rect + range + value) or, uncreated, does
 * setFocus(0) + create + value WITHOUT setting names. */
static int case_0xc(void *row,const struct oa_setting_row_render_ops *o,void *box)
{
    void *sb=o->get_rect(box,key_for(9));
    void *t2=o->get_rect(box,key_for(10));
    void *t3=o->get_rect(box,key_for(11));
    void *t4=o->get_rect(box,key_for(12));
    if(t2) {
        void *tb=o->child_at(row,UINT32_C(0x1ca8));int created=o->is_created(tb);
        uint32_t base[4];o->rect_words(t2,base);
        if(created) {
            uint32_t moved[4];int32_t pt[2];
            o->get_point(row,pt);rect_math(moved,base,pt);
            o->set_rect(tb,moved);
        } else {
            o->set_name(tb,UINT32_C(1),UINT32_C(0x1e8));
            o->set_name(tb,(uint32_t)created,UINT32_C(0x1e9));
            o->set_name(tb,UINT32_C(3),UINT32_C(0x1ea));
            o->set_name(tb,UINT32_C(2),UINT32_C(0x1eb));
            o->set_focus(tb,0);o->set_elipses(tb,0);
            o->create_textbox(tb,0,"",base,UINT32_C(1),row);
        }
        { char text[16];int32_t v=(int32_t)o->row_word(row,UINT32_C(0x2df8));
          snprintf(text,sizeof text,"%d",(int)v);o->set_text(tb,text); }
    }
    if(t3 && t4) {
        void *b2=o->child_at(row,UINT32_C(0x1f68)),*b3=o->child_at(row,UINT32_C(0x2228));
        int created=o->is_created(b2);
        uint32_t w3[4],w4[4];
        o->rect_words(t3,w3);o->rect_words(t4,w4);
        if(created) {
            uint32_t m1[4],m2[4];int32_t pt[2];
            o->get_point(row,pt);
            rect_math(m1,w3,pt);o->set_rect(b2,m1);
            rect_math(m2,w4,pt);o->set_rect(b3,m2);
            o->set_text(b2,o->row_ustring(row,UINT32_C(0x2e2c)));
            o->set_text(b3,o->row_ustring(row,UINT32_C(0x2e34)));
        } else {
            o->set_name(b2,UINT32_C(1),UINT32_C(0x1e8));
            o->set_name(b2,(uint32_t)created,UINT32_C(0x1e9));
            o->set_name(b2,UINT32_C(3),UINT32_C(0x1ea));
            o->set_name(b2,UINT32_C(2),UINT32_C(0x1eb));
            o->set_focus(b2,0);o->set_elipses(b2,0);
            o->set_text_alignment(b2,UINT32_C(6));
            o->create_textbox(b2,0,"",w3,UINT32_C(1),row);
            o->set_text(b2,o->row_ustring(row,UINT32_C(0x2e2c)));
            o->set_name(b3,UINT32_C(1),UINT32_C(0x1e8));
            o->set_name(b3,(uint32_t)created,UINT32_C(0x1e9));
            o->set_name(b3,UINT32_C(3),UINT32_C(0x1ea));
            o->set_name(b3,UINT32_C(2),UINT32_C(0x1eb));
            o->set_focus(b3,0);
            o->create_textbox(b3,0,"",w4,UINT32_C(1),row);
            o->set_text(b3,o->row_ustring(row,UINT32_C(0x2e34)));
        }
    }
    if(!sb)return 0;
    {
        void *sl=o->child_at(row,UINT32_C(0x1498));int created=o->is_created(sl);
        uint32_t base[4];o->rect_words(sb,base);
        if(created) {
            uint32_t moved[4];int32_t pt[2];
            o->get_point(row,pt);rect_math(moved,base,pt);
            o->set_rect(sl,moved);
            o->set_slider_range(sl,(int32_t)o->row_word(row,UINT32_C(0x2df0)),
                (int32_t)o->row_word(row,UINT32_C(0x2df4)));
        } else {
            o->set_slider_focus(sl,0);
            o->create_slider(sl,0,(int32_t)o->row_word(row,UINT32_C(0x2df0)),
                (int32_t)o->row_word(row,UINT32_C(0x2df4)),base,row);
        }
        o->set_slider_value(sl,(int32_t)o->row_word(row,UINT32_C(0x2df8)));
    }
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
    o->create_button(button,0,o->row_button_label(row),base,row);
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
            o->create_button(button,0,o->row_button_label(row),base,row);
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
            case_te(row,o,box,0,4,0);
            return 0;
        }
        if(type==UINT32_C(0xe) || type==UINT32_C(0xf)) {
            case_te(row,o,box,UINT32_C(0xa),5,0);
            return 0;
        }
        if(type==UINT32_C(0x10)) {
            case_te(row,o,box,1,10,1);
            return 0;
        }
        if(type==UINT32_C(5) || type==UINT32_C(6)) {
            void *b=o->get_rect(box,key_for(3)),*i1=o->get_rect(box,key_for(6)),
                 *i2=o->get_rect(box,key_for(7));
            case_btn56(row,o,box,type,b,i1,i2);
            return 0;
        }
        if(type==UINT32_C(7)) {
            case_btn7(row,o,box);
            return 0;
        }
        if(type==UINT32_C(8)) {
            case_btn8(row,o,box);
            return 0;
        }
        if(type==UINT32_C(0xc)) {
            case_0xc(row,o,box);
            return 0;
        }
        if(type==UINT32_C(0xb)) {
            case_0xb(row,o,box);
            return 0;
        }
        if(type==UINT32_C(0xa)) {
            void *sbrect=o->get_rect(box,key_for(9));
            void *t2rect=o->get_rect(box,key_for(10));
            case_slider2(row,o,box,t2rect,sbrect);
            return 0;
        }
        if(type==UINT32_C(9)) {
            /* Sequence the lookups: text first, then slider (argument
             * evaluation order is unspecified). */
            void *sbrect=o->get_rect(box,key_for(9));   /* "slidebar" first */
            void *t2rect=o->get_rect(box,key_for(10));  /* then "text2" */
            case_slider(row,o,box,t2rect,sbrect,r1);
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
            case_te(row,o,box,0,4,0);
            return 0;
        }
        if(type==UINT32_C(0xe) || type==UINT32_C(0xf)) {
            case_te(row,o,box,UINT32_C(0xa),5,0);
            return 0;
        }
        if(type==UINT32_C(0x10)) {
            case_te(row,o,box,1,10,1);
            return 0;
        }
        if(type==UINT32_C(5) || type==UINT32_C(6)) {
            void *b=o->get_rect(box,key_for(3)),*i1=o->get_rect(box,key_for(6)),
                 *i2=o->get_rect(box,key_for(7));
            case_btn56(row,o,box,type,b,i1,i2);
            return 0;
        }
        if(type==UINT32_C(7)) {
            case_btn7(row,o,box);
            return 0;
        }
        if(type==UINT32_C(8)) {
            case_btn8(row,o,box);
            return 0;
        }
        if(type==UINT32_C(0xc)) {
            case_0xc(row,o,box);
            return 0;
        }
        if(type==UINT32_C(0xb)) {
            case_0xb(row,o,box);
            return 0;
        }
        if(type==UINT32_C(0xa)) {
            void *sbrect=o->get_rect(box,key_for(9));
            void *t2rect=o->get_rect(box,key_for(10));
            case_slider2(row,o,box,t2rect,sbrect);
            return 0;
        }
        if(type==UINT32_C(9)) {
            /* Sequence the lookups: text first, then slider (argument
             * evaluation order is unspecified). */
            void *sbrect=o->get_rect(box,key_for(9));   /* "slidebar" first */
            void *t2rect=o->get_rect(box,key_for(10));  /* then "text2" */
            case_slider(row,o,box,t2rect,sbrect,r1);
            return 0;
        }
        return ENOTSUP;
    }
    /* Type 0 already returned through the shared tail above. */
    return 0;
}
