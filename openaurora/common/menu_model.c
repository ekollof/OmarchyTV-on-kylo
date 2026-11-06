#include "openaurora/menu_model.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openaurora/json.h"

static int rect4(const struct oa_json *v, int out[4])
{
    if (!v || v->type != OA_JSON_ARRAY || v->count != 4) return EINVAL;
    for (unsigned i = 0; i < 4; i++)
        if (v->items[i]->type != OA_JSON_NUMBER) return EINVAL;
    for (unsigned i = 0; i < 4; i++) out[i] = (int)v->items[i]->number;
    return 0;
}

static int pair2(const struct oa_json *v, double out[2])
{
    if (!v || v->type != OA_JSON_ARRAY || v->count != 2) return EINVAL;
    for (unsigned i = 0; i < 2; i++)
        if (v->items[i]->type != OA_JSON_NUMBER) return EINVAL;
    for (unsigned i = 0; i < 2; i++) out[i] = v->items[i]->number;
    return 0;
}

static int load_values(const struct oa_json *v, struct oa_menu_item *item)
{
    if (!v) return 0;
    if (v->type == OA_JSON_STRING) {
        struct oa_menu_value *values = calloc(1, sizeof *values);
        if (!values) return ENOMEM;
        values[0].key = v->string;
        values[0].id = -1;
        item->values = values;
        item->value_count = 1;
        return 0;
    }
    if (v->type != OA_JSON_ARRAY) return EINVAL;
    struct oa_menu_value *values = calloc(v->count ? v->count : 1, sizeof *values);
    if (!values) return ENOMEM;
    for (unsigned i = 0; i < v->count; i++) {
        const struct oa_json *entry = v->items[i];
        if (!entry || entry->type != OA_JSON_OBJECT) {
            free(values);
            return EINVAL;
        }
        values[i].key = oa_json_string(entry, "key", "");
        values[i].action = oa_json_string(entry, "action", NULL);
        values[i].id = oa_json_int(entry, "id", -1);
    }
    item->values = values;
    item->value_count = v->count;
    return 0;
}

static int load_items(const struct oa_json *array, struct oa_menu_branch *branch)
{
    if (!array || array->type != OA_JSON_ARRAY) return EINVAL;
    struct oa_menu_item *items = calloc(array->count ? array->count : 1,
                                        sizeof *items);
    if (!items) return ENOMEM;
    unsigned count = 0;
    for (unsigned i = 0; i < array->count; i++) {
        const struct oa_json *entry = array->items[i];
        if (!entry || entry->type != OA_JSON_OBJECT) {
            for (unsigned j = 0; j < count; j++) free((void *)items[j].values);
            free(items);
            return EINVAL;
        }
        struct oa_menu_item *item = &items[count];
        item->id = oa_json_int(entry, "id", -1);
        item->key = oa_json_string(entry, "key", "");
        item->action = oa_json_string(entry, "action", NULL);
        item->dynamic = oa_json_string(entry, "dynamic", NULL);
        int rc = load_values(oa_json_get(entry, "values"), item);
        if (rc) {
            for (unsigned j = 0; j < count; j++) free((void *)items[j].values);
            free(items);
            return rc;
        }
        count++;
    }
    branch->items = items;
    branch->item_count = count;
    return 0;
}

static int load_branches(const struct oa_json *array, struct oa_menu_dialog *dialog)
{
    if (!array || array->type != OA_JSON_ARRAY) return EINVAL;
    struct oa_menu_branch *branches = calloc(array->count ? array->count : 1,
                                             sizeof *branches);
    if (!branches) return ENOMEM;
    unsigned count = 0;
    for (unsigned i = 0; i < array->count; i++) {
        const struct oa_json *entry = array->items[i];
        if (!entry || entry->type != OA_JSON_OBJECT) goto invalid;
        const struct oa_json *excluded = oa_json_get(entry, "excluded");
        if (excluded && excluded->type == OA_JSON_BOOL && excluded->boolean)
            continue;
        struct oa_menu_branch *branch = &branches[count];
        branch->launcher_type = oa_json_int(entry, "launcher_type", -1);
        branch->name = oa_json_string(entry, "name", "");
        int rc = load_items(oa_json_get(entry, "items"), branch);
        if (rc) {
            for (unsigned j = 0; j < count; j++) {
                for (unsigned k = 0; k < branches[j].item_count; k++)
                    free((void *)branches[j].items[k].values);
                free((void *)branches[j].items);
            }
            free(branches);
            return rc;
        }
        count++;
    }
    dialog->branches = branches;
    dialog->branch_count = count;
    return 0;

invalid:
    for (unsigned j = 0; j < count; j++) {
        for (unsigned k = 0; k < branches[j].item_count; k++)
            free((void *)branches[j].items[k].values);
        free((void *)branches[j].items);
    }
    free(branches);
    return EINVAL;
}

static int load_dialog(const struct oa_json *o, struct oa_menu_dialog *dialog)
{
    if (!o || o->type != OA_JSON_OBJECT) return EINVAL;
    memset(dialog, 0, sizeof *dialog);
    dialog->id = oa_json_string(o, "id", "");
    dialog->class_name = oa_json_string(o, "class", "");
    dialog->name = oa_json_string(o, "name", "");
    dialog->svg = oa_json_string(o, "svg", "");
    int list[4] = { 0, 0, 0, 0 };
    if (rect4(oa_json_get(o, "list"), list) == 0) {
        dialog->list_x = list[0];
        dialog->list_y = list[1];
        dialog->list_w = list[2];
        dialog->list_h = list[3];
    }
    const struct oa_json *layout = oa_json_get(o, "layout");
    if (layout && layout->type == OA_JSON_OBJECT) {
        int r[4];
        if (rect4(oa_json_get(layout, "card"), r) == 0) {
            dialog->card_x = r[0];
            dialog->card_y = r[1];
            dialog->card_w = r[2];
            dialog->card_h = r[3];
        }
        dialog->card_pitch = oa_json_int(layout, "pitch", 0);
        if (rect4(oa_json_get(layout, "item"), r) == 0) {
            dialog->runtime_item_x = r[0];
            dialog->runtime_item_y = r[1];
        }
    }
    const struct oa_json *item = oa_json_get(o, "item");
    if (item && item->type == OA_JSON_OBJECT) {
        dialog->item_w = oa_json_int(item, "w", 0);
        dialog->item_h = oa_json_int(item, "h", 0);
        int r[4];
        if (rect4(oa_json_get(item, "scalerect"), r) == 0) {
            dialog->scalerect_x = r[0];
            dialog->scalerect_y = r[1];
        }
        if (rect4(oa_json_get(item, "glyph"), r) == 0) {
            dialog->glyph_x = r[0];
            dialog->glyph_y = r[1];
            dialog->glyph_w = r[2];
            dialog->glyph_h = r[3];
        }
        if (rect4(oa_json_get(item, "text"), r) == 0) {
            dialog->text_x = r[0];
            dialog->text_y = r[1];
            dialog->text_w = r[2];
            dialog->text_h = r[3];
        }
        dialog->anim_ms = (unsigned)oa_json_int(item, "anim_ms", 0);
        double scale[2];
        if (pair2(oa_json_get(item, "scale"), scale) == 0) {
            dialog->scale_from = (float)scale[0];
            dialog->scale_to = (float)scale[1];
        } else {
            dialog->scale_from = 1.0f;
            dialog->scale_to = 1.0f;
        }
        const struct oa_json *gd = oa_json_get(item, "glyph_delta");
        if (gd && gd->type == OA_JSON_ARRAY && gd->count == 2 &&
            gd->items[0]->type == OA_JSON_NUMBER &&
            gd->items[1]->type == OA_JSON_NUMBER) {
            dialog->glyph_dx = (int)gd->items[0]->number;
            dialog->glyph_dy = (int)gd->items[1]->number;
        }
    }
    if (dialog->card_w <= 0) {
        dialog->card_x = dialog->list_x;
        dialog->card_y = dialog->list_y;
        dialog->card_w = dialog->item_w;
        dialog->card_h = dialog->item_h;
    }
    if (dialog->card_pitch <= 0) dialog->card_pitch = dialog->item_w;
    if (dialog->runtime_item_x == 0 && dialog->runtime_item_y == 0) {
        dialog->runtime_item_x = dialog->card_x;
        dialog->runtime_item_y = dialog->card_y;
    }
    return load_branches(oa_json_get(o, "branches"), dialog);
}

static void free_dialog(struct oa_menu_dialog *dialog)
{
    for (unsigned i = 0; i < dialog->branch_count; i++) {
        struct oa_menu_branch *branch = (struct oa_menu_branch *)&dialog->branches[i];
        for (unsigned j = 0; j < branch->item_count; j++)
            free((void *)branch->items[j].values);
        free((void *)branch->items);
    }
    free((void *)dialog->branches);
}

void oa_menu_free(struct oa_menu_doc *doc)
{
    if (!doc) return;
    for (unsigned i = 0; i < doc->dialog_count; i++)
        free_dialog(&doc->dialogs[i]);
    free(doc->dialogs);
    oa_json_free(doc->root);
    free(doc);
}

int oa_menu_load_text(const char *text, struct oa_menu_doc **out)
{
    if (!text || !out) return EINVAL;
    *out = NULL;
    char err[160];
    struct oa_json *root = oa_json_parse(text, err, sizeof err);
    if (!root) return EINVAL;
    const struct oa_json *dialogs = oa_json_get(root, "dialogs");
    if (dialogs && dialogs->type != OA_JSON_ARRAY) {
        oa_json_free(root);
        return EINVAL;
    }
    unsigned count = dialogs ? (unsigned)dialogs->count : 1;
    struct oa_menu_doc *doc = calloc(1, sizeof *doc);
    if (!doc) {
        oa_json_free(root);
        return ENOMEM;
    }
    doc->root = root;
    doc->dialogs = calloc(count ? count : 1, sizeof *doc->dialogs);
    if (!doc->dialogs) {
        oa_menu_free(doc);
        return ENOMEM;
    }
    if (dialogs) {
        for (unsigned i = 0; i < count; i++) {
            if (load_dialog(dialogs->items[i], &doc->dialogs[i]) != 0) {
                doc->dialog_count = i;
                oa_menu_free(doc);
                return EINVAL;
            }
            doc->dialog_count = i + 1;
        }
    } else {
        if (load_dialog(root, &doc->dialogs[0]) != 0) {
            oa_menu_free(doc);
            return EINVAL;
        }
        doc->dialog_count = 1;
    }
    *out = doc;
    return 0;
}

int oa_menu_load(const char *path, struct oa_menu_doc **out)
{
    if (!path || !out) return EINVAL;
    *out = NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return errno;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return EIO;
    }
    long size = ftell(f);
    if (size < 0 || size > 1024 * 1024) {
        fclose(f);
        return size < 0 ? EIO : EFBIG;
    }
    rewind(f);
    char *text = malloc((size_t)size + 1);
    if (!text) {
        fclose(f);
        return ENOMEM;
    }
    if (fread(text, 1, (size_t)size, f) != (size_t)size) {
        free(text);
        fclose(f);
        return EIO;
    }
    text[size] = '\0';
    fclose(f);
    int rc = oa_menu_load_text(text, out);
    free(text);
    return rc;
}

const struct oa_menu_dialog *oa_menu_find(const struct oa_menu_doc *doc,
                                          const char *id)
{
    if (!doc || !id) return NULL;
    for (unsigned i = 0; i < doc->dialog_count; i++)
        if (!strcmp(doc->dialogs[i].id, id)) return &doc->dialogs[i];
    return NULL;
}

const struct oa_menu_branch *oa_menu_branch(const struct oa_menu_dialog *dialog,
                                            int launcher_type)
{
    if (!dialog) return NULL;
    for (unsigned i = 0; i < dialog->branch_count; i++)
        if (dialog->branches[i].launcher_type == launcher_type)
            return &dialog->branches[i];
    return NULL;
}

void oa_menu_item_anim(const struct oa_menu_dialog *dialog, int item_x,
                       int item_y, float progress, oa_launcher_rect *rect,
                       int *glyph_dx, int *glyph_dy)
{
    if (!dialog) return;
    oa_launcher_scale_rect(item_x, item_y, dialog->item_w, dialog->item_h,
                           dialog->scale_from, dialog->scale_to, progress,
                           rect);
    int dx = 0, dy = 0;
    oa_launcher_motion_point(0, 0, 0, 0, dialog->glyph_dx, dialog->glyph_dy,
                             progress, &dx, &dy);
    if (glyph_dx) *glyph_dx = dx;
    if (glyph_dy) *glyph_dy = dy;
}

void oa_menu_card_rect(const struct oa_menu_dialog *dialog, unsigned index,
                       float progress, oa_launcher_rect *card,
                       oa_launcher_rect *item)
{
    if (!dialog) return;
    int cx = dialog->card_x + (int)index * dialog->card_pitch;
    int cy = dialog->card_y;
    if (card)
        oa_launcher_scale_rect(cx, cy, dialog->card_w, dialog->card_h,
                               dialog->scale_from, dialog->scale_to, progress,
                               card);
    if (item) {
        item->x1 = dialog->runtime_item_x + (int)index * dialog->card_pitch;
        item->y1 = dialog->runtime_item_y;
        item->x2 = item->x1 + dialog->item_w;
        item->y2 = item->y1 + dialog->item_h;
    }
}

int oa_menu_dispatch(const char *action, oa_menu_action_fn fn, void *ud)
{
    if (!action || !fn) return EINVAL;
    return fn(ud, action);
}
