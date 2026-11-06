/* JSON-driven menu tree and item animation.
 *
 * The vendor firmware builds its settings UI from dialog classes,
 * SettingItemsListBox rows and SettingItem/SettingValue entries. The offline
 * decoder tools/re/menu-tree.py extracts that tree; the openaurora/menus/
 * directory carries the runtime JSON form. A menu entry is data: key, values
 * and an action
 * string such as "open:picture" or "set:MODE=NATURAL". Adding a branch or
 * entry does not require a new C table.
 *
 * The loader is bounded and owns all returned strings through its JSON DOM.
 * Views stay valid until oa_menu_free(); no file or device access after load.
 */
#ifndef OPENAURORA_MENU_MODEL_H
#define OPENAURORA_MENU_MODEL_H

#include "openaurora/launcher_anim.h"

struct oa_json;

struct oa_menu_value {
    const char *key;
    const char *action; /* optional action when this value is chosen */
    int id;             /* vendor SettingValue id; -1 when not decoded */
};

struct oa_menu_item {
    int id;    /* SettingItem id / list order */
    const char *key;    /* label/setting key */
    const char *action; /* optional action when the item is chosen */
    const char *dynamic; /* non-NULL: runtime-selected key; firmware source note */
    const struct oa_menu_value *values;
    unsigned value_count;
};

struct oa_menu_branch {
    int launcher_type; /* NovoLauncherItemType that selects this branch */
    const char *name;
    const struct oa_menu_item *items;
    unsigned item_count;
};

struct oa_menu_dialog {
    const char *id;
    const char *class_name;
    const char *name;
    const char *svg;
    int list_x, list_y, list_w, list_h; /* named listBox rect */
    int item_w, item_h;
    int card_x, card_y, card_w, card_h; /* checked runtime card content */
    int card_pitch;
    int runtime_item_x, runtime_item_y; /* checked runtime item-cell origin */
    int scalerect_x, scalerect_y;
    int glyph_x, glyph_y, glyph_w, glyph_h;
    int text_x, text_y, text_w, text_h;
    unsigned anim_ms;
    float scale_from, scale_to;
    int glyph_dx, glyph_dy;
    const struct oa_menu_branch *branches;
    unsigned branch_count;
};

struct oa_menu_doc {
    struct oa_json *root; /* owns every string returned below */
    struct oa_menu_dialog *dialogs;
    unsigned dialog_count;
};

/* Load a JSON menu document. Returns 0 or an errno value; *out is NULL on
 * failure. The file is bounded to 1 MiB. */
int oa_menu_load(const char *path, struct oa_menu_doc **out);
int oa_menu_load_text(const char *text, struct oa_menu_doc **out);
void oa_menu_free(struct oa_menu_doc *doc);

const struct oa_menu_dialog *oa_menu_find(const struct oa_menu_doc *doc,
                                          const char *id);
const struct oa_menu_branch *oa_menu_branch(const struct oa_menu_dialog *dialog,
                                            int launcher_type);

/* Focused item at progress 0..1: rect is anchored at the item window top-left
 * and uses the dialog's item size and scale range; glyph offsets are the
 * decoded #glyph motion. */
void oa_menu_item_anim(const struct oa_menu_dialog *dialog, int item_x,
                       int item_y, float progress, oa_launcher_rect *rect,
                       int *glyph_dx, int *glyph_dy);

/* Checked runtime row geometry: card content origin/size, item-cell origin
 * and pitch come from the p0/03 capture and the decoded listBox/item
 * resources. progress 0..1 scales the card by the recovered item range,
 * anchored at the card top-left. */
void oa_menu_card_rect(const struct oa_menu_dialog *dialog, unsigned index,
                       float progress, oa_launcher_rect *card,
                       oa_launcher_rect *item);

/* Dispatch an action string to a caller-supplied handler. The library does not
 * own a global action registry; modules decide what an action means. */
typedef int (*oa_menu_action_fn)(void *ud, const char *action);
int oa_menu_dispatch(const char *action, oa_menu_action_fn fn, void *ud);

#endif
