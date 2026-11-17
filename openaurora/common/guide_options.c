/* EPG event options dialog model (records 151; dialog bodies). Pure. */
#include "openaurora/guide_options.h"

#include <errno.h>
#include <string.h>

int oa_guide_options_layout_default(oa_guide_options_layout *out)
{
    if (!out)
        return -EINVAL;
    memset(out, 0, sizeof *out);
    out->panel = (oa_guide_rect){ 0, 0, 1920, 1080 };
    out->bg_textbox = (oa_guide_rect){ 610, 806, 1310, 909 };
    out->item = (oa_guide_rect){ 610, 844, 1310, 920 };
    out->title = (oa_guide_rect){ 634, 750, 1320, 825 };
    out->item_pitch = 65;
    out->item_height = 76;
    out->bg_opacity = 7;
    const unsigned char panel[3] = { 255, 255, 255 };
    const unsigned char bg[3] = { 255, 255, 255 };
    const unsigned char item[3] = { 1, 1, 1 };          /* #010101 */
    const unsigned char focus[3] = { 254, 148, 25 };    /* #FE9419 */
    const unsigned char text[3] = { 255, 255, 255 };
    const unsigned char focus_text[3] = { 126, 0, 10 }; /* #7E000A */
    memcpy(out->panel_rgb, panel, sizeof panel);
    memcpy(out->bg_rgb, bg, sizeof bg);
    memcpy(out->item_rgb, item, sizeof item);
    memcpy(out->focus_rgb, focus, sizeof focus);
    memcpy(out->text_rgb, text, sizeof text);
    memcpy(out->focus_text_rgb, focus_text, sizeof focus_text);
    return 0;
}

int oa_guide_options_height(const oa_guide_options_layout *layout, int count)
{
    if (!layout || count < 0)
        return -EINVAL;
    return (layout->bg_textbox.y1 - layout->item.y) * count +
           (layout->item.y - layout->bg_textbox.y) +
           (layout->bg_textbox.y1 - layout->item.y1) * 2;
}

int oa_guide_options_item_rect(const oa_guide_options_layout *layout, int i,
                               int offset_y, oa_guide_rect *out)
{
    if (!layout || !out || layout->item_pitch <= 0)
        return -EINVAL;
    if (i < 0)
        return -ERANGE;
    int y = layout->item.y + i * layout->item_pitch + offset_y;
    *out = (oa_guide_rect){ layout->item.x, y, layout->item.x1,
                            layout->item.y1 + i * layout->item_pitch +
                                offset_y };
    return 0;
}

int oa_guide_options_build(const struct oa_guide_options_inputs *in,
                           const char **keys, unsigned cap, unsigned *count)
{
    if (!in || !keys || !count)
        return -EINVAL;
    const char *out[OA_GUIDE_OPTIONS_MAX];
    unsigned total = 0;
#define ADD(key) do {                                \
        if (total < OA_GUIDE_OPTIONS_MAX)            \
            out[total] = (key);                      \
        total++;                                     \
    } while (0)
    ADD("VIEW_CHANNEL");
    if (in->on_demand_mode) {
        if (in->on_demand_available == 2)
            ADD("HLP_WATCH");
        ADD("MORE_INFO");
        if (in->player_available)
            ADD("LAUNCH_SERVICE_PLAYER");
    } else if (in->has_enhanced) {
        if (in->restart_url)
            ADD("RESTART");
        else if (in->on_demand_available == 2)
            ADD("HLP_WATCH");
        if (!in->no_information)
            ADD("MORE_INFO");
        if (in->player_available)
            ADD("LAUNCH_SERVICE_PLAYER");
    } else {
        /* The no-enhanced branch adds MORE_INFO from its first vtable call. */
        ADD("MORE_INFO");
        if (in->player_available)
            ADD("LAUNCH_SERVICE_PLAYER");
    }
#undef ADD
    if (total > OA_GUIDE_OPTIONS_MAX)
        return -EOVERFLOW;
    if (total > cap)
        return -EOVERFLOW;
    for (unsigned i = 0; i < total; i++)
        keys[i] = out[i];
    *count = total;
    return 0;
}
