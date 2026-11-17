#ifndef OPENAURORA_CHANNEL_EDIT_H
#define OPENAURORA_CHANNEL_EDIT_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Stock channel edit-options pane, recovered from
 * ChannelEditOptionsDialog::initialiseFields 0x00e8decc (internal name
 * "ChannelEditOptions") and DialogEditOptionsList.svg (BinSVG 100):
 *   panel 0,35,780,1080 white; left strip 0,160,142,1080 #010101 at 6/10
 *   over the panel (composed 103,103,103); content 142,160,782,1080 #3F0B13;
 *   list 152,211,768,1059; dialogTitle is offscreen (1285,-60).
 * The switch adds the options in this order:
 *   0 ADD_TO_HOME          (single selection, not already at home)
 *   1 MOVE_TO_POSITION     (multiple selection)
 *   2 DELETE / REMOVE      (not Rovi, deletable; DELETE for single, REMOVE for
 *                           multiple)
 *   3 EDIT_NAME            (name editable, single selection)
 *   4 HLP_LOCK_UNLOCK      (always)
 *   5 HLP_FAVOURITE_ADD_REMOVE + " " + FAVOURITES (always; one combined label)
 *   6 HLP_TAG              (always)
 *   7 HLP_TAG_ALL          (always)
 * The runtime flags come from the service/list/CA state (`updateSettings`);
 * this model takes them explicitly. Row pitch 70 is the settings-list step;
 * 12 rows fit the 211..1059 list. No UI/host I/O.
 */
typedef struct {
    int x, y, x1, y1;
} oa_channel_edit_rect;

typedef struct {
    const char *key;      /* language key */
    unsigned case_id;     /* switch case 0..7 (case 5 is the combined label) */
} oa_channel_edit_option;

typedef struct {
    int single_selection;     /* this[0x4ac] == 0 */
    int multiple_selection;   /* local_7a */
    int name_editable;        /* local_7b */
    int deletable;            /* local_7c */
    int at_home;              /* +0x298 vtable +0x18 != 0 */
    int rovi;                 /* isRoviPatentEnabled() */
} oa_channel_edit_inputs;

typedef struct {
    oa_channel_edit_rect panel, strip, content, list;
    int row_pitch;            /* 70 */
    int row_height;           /* 66 */
    int visible_rows;         /* 12 */
    unsigned char panel_rgb[3], strip_rgb[3], content_rgb[3];
    unsigned char focus_rgb[3], text_rgb[3], focus_text_rgb[3];
} oa_channel_edit_layout;

#define OA_CHANNEL_EDIT_OPTIONS 8

/* Fill the record-100 defaults. -EINVAL for NULL. */
int oa_channel_edit_layout_default(oa_channel_edit_layout *out);

/* The fixed option catalogue (case order). */
const oa_channel_edit_option *oa_channel_edit_catalogue(void);

/* Fill the visible case ids in switch order and report the count. `cap` is
 * the caller's array size; the count is always the full visible count.
 * -EINVAL for NULL, -EOVERFLOW when cap is too small. */
int oa_channel_edit_visible(const oa_channel_edit_inputs *in,
                            unsigned *out, unsigned cap, unsigned *count);

/* The label key for a visible case: case 2 is DELETE for single selection and
 * REMOVE for multiple; case 5 is the combined favourite label. Returns a
 * language key or NULL for an unknown case. */
const char *oa_channel_edit_label(unsigned case_id, int multiple_selection);

#ifdef __cplusplus
}
#endif

#endif
