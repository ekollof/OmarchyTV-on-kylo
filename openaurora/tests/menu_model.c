/* JSON-driven firmware menu tree: geometry, branches, actions and the
 * recovered 300 ms sub-launcher item animation. The extra-branch case proves
 * a new menu entry is data, not a new C table. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/launcher_anim.h"
#include "openaurora/menu_model.h"

struct recorder {
    char last[128];
    int calls;
};

static int record(void *ud, const char *action)
{
    struct recorder *r = ud;
    snprintf(r->last, sizeof r->last, "%s", action);
    r->calls++;
    return 0;
}

static int has_value(const struct oa_menu_item *item, const char *key,
                     const char *action)
{
    for (unsigned i = 0; i < item->value_count; i++)
        if (!strcmp(item->values[i].key, key) &&
            ((!action && !item->values[i].action) ||
             (action && item->values[i].action &&
              !strcmp(item->values[i].action, action))))
            return 1;
    return 0;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    struct oa_menu_doc *doc = NULL;
    int rc = oa_menu_load(argv[1], &doc);
    assert(rc == 0 && doc);

    const struct oa_menu_dialog *d = oa_menu_find(doc, "sub_launcher");
    assert(d);
    assert(!strcmp(d->class_name, "SubLauncherDialog"));
    assert(!strcmp(d->name, "SubLauncher"));
    assert(!strcmp(d->svg, "DialogSubLauncher.svg"));
    assert(d->list_x == 189 && d->list_y == 255);
    assert(d->list_w == 1774 && d->list_h == 222);
    assert(d->item_w == 240 && d->item_h == 140);
    assert(d->scalerect_x == 121 && d->scalerect_y == 251);
    assert(d->glyph_x == 121 && d->glyph_y == 253);
    assert(d->text_x == 79 && d->text_y == 374);
    assert(d->anim_ms == 300);
    assert(d->scale_from == 1.0f && d->scale_to == 1.35f);
    assert(d->glyph_dx == -42 && d->glyph_dy == -24);
    assert(d->branch_count == 5);

    const struct oa_menu_branch *picture = oa_menu_branch(d, 2);
    assert(picture && !strcmp(picture->name, "picture"));
    assert(picture->item_count == 3);
    assert(picture->items[0].id == 0 && !strcmp(picture->items[0].key, "MODE"));
    assert(!strcmp(picture->items[0].action, "setting:MODE"));
    assert(picture->items[0].value_count == 1);
    assert(!strcmp(picture->items[0].values[0].key, "picture_modes"));
    assert(!strcmp(picture->items[1].key, "ENERGY_SAVING"));
    assert(picture->items[1].value_count == 7);
    assert(has_value(&picture->items[1], "OFF", "set:ENERGY_SAVING=OFF"));
    assert(has_value(&picture->items[1], "CUSTOM",
                     "set:ENERGY_SAVING=CUSTOM"));
    assert(!strcmp(picture->items[2].key, "PICTURE_ZOOM"));
    assert(has_value(&picture->items[2], "ZOOM_MODE_16_9",
                     "set:PICTURE_ZOOM=ZOOM_MODE_16_9"));
    assert(has_value(&picture->items[2], "21:9",
                     "set:PICTURE_ZOOM=21:9"));

    const struct oa_menu_branch *sound = oa_menu_branch(d, 3);
    assert(sound && sound->item_count == 6);
    assert(!strcmp(sound->items[0].key, "EQUALIZER"));
    assert(!strcmp(sound->items[1].key, "CEC_SPEAKERS"));
    assert(!strcmp(sound->items[2].key, "DOLBY_AUDIO_OR_MODE"));
    assert(sound->items[2].dynamic &&
           strstr(sound->items[2].dynamic, "DAT_00ee1de0"));
    assert(!strcmp(sound->items[3].key, "SOUND_OUTPUT"));
    assert(has_value(&sound->items[3], "HDMI_ARC",
                     "set:SOUND_OUTPUT=HDMI_ARC"));
    assert(!strcmp(sound->items[4].key, "AVL"));
    assert(!strcmp(sound->items[5].key, "AVL"));

    const struct oa_menu_branch *system = oa_menu_branch(d, 7);
    assert(system && system->item_count == 4);
    assert(!strcmp(system->items[0].key, "SLEEP_TIMER"));
    assert(system->items[0].dynamic &&
           strstr(system->items[0].dynamic, "SLEEP_TIMER_STR"));
    assert(!strcmp(system->items[1].key, "AUTO_TV_OFF"));
    assert(system->items[1].dynamic &&
           strstr(system->items[1].dynamic, "AUTO_TV_OFF"));
    assert(!strcmp(system->items[2].key, "SUBTITLE"));
    assert(!strcmp(system->items[3].key, "AUDIO_LANG"));
    assert(has_value(&system->items[2], "ON", "set:SUBTITLE=ON"));

    const struct oa_menu_branch *channel = oa_menu_branch(d, 0);
    assert(channel && channel->item_count == 1);
    assert(!strcmp(channel->items[0].key, "FILTER_FAVOURITES_LABEL"));
    const struct oa_menu_branch *ai = oa_menu_branch(d, 61);
    assert(ai && ai->item_count == 1);
    assert(!strcmp(ai->items[0].key, "AI_TXT"));
    assert(has_value(&ai->items[0], "OFF", "set:AI_TXT=OFF"));

    /* FVP, recommendations and advertising are explicitly excluded. */
    assert(!oa_menu_branch(d, 52));
    assert(!oa_menu_branch(d, -1));

    struct recorder rec = { { 0 }, 0 };
    assert(oa_menu_dispatch(picture->items[1].values[0].action, record, &rec) == 0);
    assert(rec.calls == 1 && !strcmp(rec.last, "set:ENERGY_SAVING=OFF"));
    assert(oa_menu_dispatch(picture->items[0].action, record, &rec) == 0);
    assert(!strcmp(rec.last, "setting:MODE"));

    assert(d->card_x == 201 && d->card_y == 276);
    assert(d->card_w == 235 && d->card_h == 130);
    assert(d->card_pitch == 250);
    assert(d->runtime_item_x == 199 && d->runtime_item_y == 274);

    oa_launcher_rect card, item;
    oa_menu_card_rect(d, 0, 0.0f, &card, &item);
    assert(card.x1 == 201 && card.y1 == 276);
    assert(card.x2 == 436 && card.y2 == 406);
    assert(item.x1 == 199 && item.y1 == 274);
    assert(item.x2 == 439 && item.y2 == 414);
    oa_menu_card_rect(d, 2, 0.0f, &card, &item);
    assert(card.x1 == 701 && item.x1 == 699);
    oa_menu_card_rect(d, 0, 0.5f, &card, &item);
    assert(card.x2 == 477 && card.y2 == 428);
    oa_menu_card_rect(d, 0, 1.0f, &card, &item);
    assert(card.x2 == 518 && card.y2 == 451);

    oa_launcher_rect r;
    int gx = 0, gy = 0;
    oa_menu_item_anim(d, 100, 200, 0.0f, &r, &gx, &gy);
    assert(r.x1 == 100 && r.y1 == 200 && r.x2 == 340 && r.y2 == 340);
    assert(gx == 0 && gy == 0);
    oa_menu_item_anim(d, 100, 200, 0.5f, &r, &gx, &gy);
    assert(r.x2 == 382 && r.y2 == 364);
    assert(gx == -21 && gy == -12);
    oa_menu_item_anim(d, 100, 200, 1.0f, &r, &gx, &gy);
    assert(r.x2 == 424 && r.y2 == 389);
    assert(gx == -42 && gy == -24);

    oa_menu_free(doc);

    /* Additive: a new branch and entry is a JSON edit, no C table change. */
    const char *extra =
        "{\"dialogs\":[{\"id\":\"test\",\"list\":[0,0,100,100],"
        "\"item\":{\"w\":10,\"h\":10},"
        "\"branches\":[{\"launcher_type\":99,\"name\":\"extra\",\"items\":["
        "{\"id\":7,\"key\":\"NEW\",\"action\":\"open:new_dialog\","
        "\"values\":[{\"key\":\"ON\",\"action\":\"set:NEW=ON\"}]}]}]}]}";
    rc = oa_menu_load_text(extra, &doc);
    assert(rc == 0 && doc);
    const struct oa_menu_dialog *td = oa_menu_find(doc, "test");
    assert(td && td->branch_count == 1);
    const struct oa_menu_branch *xb = oa_menu_branch(td, 99);
    assert(xb && xb->item_count == 1);
    assert(!strcmp(xb->items[0].key, "NEW"));
    assert(!strcmp(xb->items[0].action, "open:new_dialog"));
    assert(oa_menu_dispatch(xb->items[0].action, record, &rec) == 0);
    assert(!strcmp(rec.last, "open:new_dialog"));
    oa_menu_free(doc);

    doc = NULL;
    assert(oa_menu_load_text("{", &doc) != 0 && doc == NULL);
    assert(oa_menu_load_text("{\"dialogs\":[{\"id\":\"x\"}]}", &doc) != 0 &&
           doc == NULL);

    puts("ok menu model: JSON firmware tree, actions and 300 ms item animation");
    return 0;
}
