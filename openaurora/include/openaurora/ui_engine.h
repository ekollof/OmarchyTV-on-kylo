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

typedef struct {
    char name[48];
    int w, h;
    unsigned char *rgba;
} oa_ui_icon;

typedef struct {
    int (*info)(int *w, int *h, int *bpp, int *frames);
    int (*clear)(int r, int g, int b);
    int (*clear_rgba)(int r, int g, int b, int a);
    int (*fill)(int x, int y, int w, int h, int r, int g, int b);
    int (*blit)(int x, int y, int w, int h, const unsigned char *rgba);
    int (*blit_scaled)(int x, int y, int w, int h, int sw, int sh, const unsigned char *rgba);
    int (*present)(void);
    int (*present_pending)(void);
    const oa_ui_icon *(*icon)(const char *name);   /* lazy OAI1 load from ui.dir */
    int (*set_scene_transform)(int type, float progress);
    /* A new skin generation must earn its own readiness receipt. */
    void (*reset_ready)(void);
    uint32_t (*now_ms)(void);
} oa_ui_engine_iface;

#endif
