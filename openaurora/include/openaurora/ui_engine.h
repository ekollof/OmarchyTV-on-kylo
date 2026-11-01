/* Generic UI engine: display discovery, OAI1 assets, draw/present/ack and
 * readiness. Skins (the vendor UI today, OmarchyTV later) draw through this
 * service and never hold the display themselves.
 *
 * The service is a singleton; function pointers use the engine's global state so
 * a skin can keep the same call shape it had when it talked to display directly.
 */
#ifndef OPENAURORA_UI_ENGINE_H
#define OPENAURORA_UI_ENGINE_H
#include <stdint.h>

#define OA_SVC_UI_ENGINE "ui.engine"
#define OA_SVC_UI_SKIN "ui.skin"
/* Copied snapshot for the host's skin selector, independent of drawing ABI. */
typedef struct {
    char name[48];
    uint32_t owner;
    int entered, ready, error;
} oa_ui_skin_status;
typedef struct { int (*get)(oa_ui_skin_status *out); } oa_ui_skin_status_iface;

typedef struct {
    char name[48];
    int w, h;
    unsigned char *rgba;
} oa_ui_icon;

/* A skin owns its screens and key semantics; the engine owns display/assets,
 * present/ack, the key topic and the repaint timer. Callbacks are posted back to
 * the skin's owner token, so timers/actions created there stay skin-owned. */
typedef struct {
    const char *name;
    int (*enter)(void *ud);          /* first time the display is ready */
    void (*leave)(void *ud);
    void (*paint)(void *ud);
    int (*key)(void *ud, int id);    /* 0 handled, nonzero ignored */
} oa_ui_skin;

typedef struct {
    int (*info)(int *w, int *h, int *bpp, int *frames);
    int (*clear)(int r, int g, int b);
    int (*clear_rgba)(int r, int g, int b, int a);
    int (*fill)(int x, int y, int w, int h, int r, int g, int b);
    int (*blit)(int x, int y, int w, int h, const unsigned char *rgba);
    int (*blit_scaled)(int x, int y, int w, int h, int sw, int sh, const unsigned char *rgba);
    int (*present)(void);
    int (*present_pending)(void);  /* 1 pending, 0 completed, -1 failed (errno) */
    const oa_ui_icon *(*icon)(const char *name);   /* lazy OAI1 load from ui.dir */
    int (*set_scene_transform)(int type, float progress);
    /* A new skin generation must earn its own readiness receipt. */
    void (*reset_ready)(void);
    uint32_t (*now_ms)(void);
    /* Active skin. ui.skin selects by name; an empty/missing key accepts the
     * first registered skin (the vendor fallback). */
    int (*register_skin)(const oa_ui_skin *skin, void *ud);
    void (*unregister_skin)(const oa_ui_skin *skin);
    /* v2: post only the skin's own trampoline/code and module-lifetime ud.
     * It calls dispatch_skin() from its owner context. No engine callback is
     * borrowed by another module's queue. v1 register_skin fails ENOTSUP. */
    int (*register_skin_dispatch)(const oa_ui_skin *skin, void *ud, void (*dispatch)(void *));
    void (*dispatch_skin)(void);
} oa_ui_engine_iface;

#endif
