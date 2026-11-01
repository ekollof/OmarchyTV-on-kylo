/*
 * Well-known service interfaces. A module publishes one with api->publish(),
 * consumers get it back with api->lookup() at a minimum version.
 *
 * Interfaces are plain structs of function pointers: no vtables, no C++.
 */
#ifndef OPENAURORA_SERVICES_H
#define OPENAURORA_SERVICES_H

#include "openaurora/audio_curve.h"
#include "openaurora/audio_broker.h"
#include "openaurora/cec_policy.h"
#include "openaurora/ui_anim.h"

#include <stdint.h>

/* Host-owned sleep coordinator. Loop-thread only. The owner pins the entry
 * callback's module through prepare/entry/restore; 0 denotes host code. */
#define OA_SVC_SLEEP "host.sleep"
typedef struct {
    int prepare_rc;
    int entry_attempted;
    int entry_rc;
    int restore_rc;
} oa_sleep_result;
typedef struct {
    void *context;
    int (*cycle)(void *context, uint32_t owner, int (*enter)(void *), void *ud,
                  oa_sleep_result *result);
} oa_sleep_iface;

/* cec.owner — worker lifecycle only. hardware_open_supported is 0 until a
 * firmware-backed authority and exclusive-open backend is explicitly installed. */
#define OA_SVC_CEC_OWNER "cec.owner"
typedef struct {
    int (*hardware_open_supported)(void);
} oa_cec_service;

/* cec.audio — TV System Audio / ARC policy. No device or amplifier access.
 * local_volume_permitted is 0 while System Audio Mode is on. */
#define OA_SVC_CEC_AUDIO "cec.audio"
typedef struct {
    int (*handle)(const oa_cec_message *incoming, oa_cec_message *replies, int capacity);
    void (*route)(oa_cec_audio_route *out);
    int (*local_volume_permitted)(void);
    int (*request_system_audio)(int on, uint16_t source_pa, oa_cec_message *out);
    int (*request_arc)(int initiate, oa_cec_message *out);
    int (*query)(int opcode, oa_cec_message *out);
    int (*user_volume)(int action, oa_cec_message *out);
    int (*set_arc_link)(int adjacent, int can_transmit);
} oa_cec_audio_iface;

/* clock — monotonic milliseconds. Published by mod_ticker. */
#define OA_SVC_CLOCK "clock"
typedef struct {
    uint64_t (*now_ms)(void);
} oa_clock_iface;

/* ui.clock — vendor FROST application time, distinct from raw clock. Values
 * roll over at the vendor period; use elapsed(), never plain subtraction. */
#define OA_SVC_UI_CLOCK "ui.clock"
typedef struct {
    uint32_t (*now_ms)(void);
    uint32_t (*elapsed)(uint32_t now, uint32_t before);
} oa_ui_clock_iface;

/* panel — backlight. Published by mod_panel, backed by the vendor PWM HAL. */
#define OA_SVC_PANEL "panel"
typedef struct {
    int (*set_backlight_percent)(int pct); /* 0..100, 0 = off      */
    int (*get_backlight_percent)(void);    /* -1 when unavailable  */
} oa_panel_iface;

/* power — standby. Published by mod_power. */
#define OA_SVC_POWER "power"
typedef struct {
    int (*standby)(void);     /* 0 after entry+restoration succeed; -1 if any phase fails */
    int (*is_available)(void);/* 0 when disabled (host build / no vendor node) */
} oa_power_iface;

/* plat — the vendor platform bring-up. Published by mod_plat.
 *
 * Mirrors PLAT_DRV_INI_Init (aurora @0089b6e4), which every consumer of the vendor HAL needs to have
 * run first: audio prelude → plat_common_init("DVIF") → Hal_Gpio_Init → Hal_Iic_Init → VIN slots →
 * NDAL_VideoExt_OpenDevice(0,10). Modules that touch the HAL declare it in requires[] so the host
 * refuses to start them before it. */
#define OA_SVC_PLAT "plat"
typedef struct {
    int (*is_ready)(void);   /* 1 = bring-up sequence completed, 0 = stopped; inspect stage_errors too */
    int (*stage_errors)(void); /* bitmask of the optional stages that failed */
} oa_plat_iface;

/* display — the OSD layer via fbdev. Published by mod_display. */
#define OA_SVC_DISPLAY "display"
typedef struct {
    int (*clear)(int r, int g, int b);
    int (*fill)(int x, int y, int w, int h, int r, int g, int b);
    /* rgba is w*h*4 bytes, straight RGBA (OAI1 assets). The display converts
     * this input to raw BGRA OSD storage; framebuffer captures are not RGBA. */
    int (*blit)(int x, int y, int w, int h, const unsigned char *rgba);
    /* nearest-neighbour scale from sw x sh to w x h */
    int (*blit_scaled)(int x, int y, int w, int h, int sw, int sh, const unsigned char *rgba);
    int (*present)(void);  /* commit rendered frame; -1 on sync/commit failure */
    int (*info)(int *w, int *h, int *bpp, int *frames);
    /* v2: full-screen transition applied at present. The scene is rendered offscreen and mapped
     * through the recovered four-vertex clip-space quad (type 1 keystone, 2/3 x-slide, 4/5
     * y-slide, 7/8 scale), with fan-triangle UVs, linear filtering and progress
     * alpha fade (including type6). Active output is premultiplied BGRA, transparent
     * outside the quad. type 0 or progress outside 0..1 clears the transform. */
    int (*set_scene_transform)(int type, float progress);
} oa_display_iface;

/* audio.curve — immutable AQ-derived DSP table, published by mod_audio_curve.
 * Hold the provider through lookup/release while using this interface. copy()
 * returns data owned by the caller; no pointer into a reloadable DSO escapes.
 * These loop-thread operations do not touch hardware. v1 does not implement
 * routing, full AQ processing, compiled fallback or native gain application. */
#define OA_SVC_AUDIO_CURVE "audio.curve"
typedef struct {
    int (*copy)(oa_audio_curve *out);
    int (*value)(unsigned volume, uint16_t *out);
} oa_audio_curve_iface;

/* audio — volume/mute. Published by mod_audio.
 * bind_session is reserved and returns ENOTSUP for every argument until an
 * asynchronous service completion contract exists. The lower owner supports
 * injected synchronous RPCs and bounded ticketed async requests; the service
 * adapter and its consumer/provider lifetime handling are still pending. */
#define OA_SVC_AUDIO "audio"
typedef struct {
    int (*set_volume)(int v);   /* 0..100 */
    int (*get_volume)(void);    /* last accepted request; -1 unknown, not a hardware readback */
    int (*set_mute)(int on);
    int (*is_available)(void);  /* 0 when disabled (host build / not on the TV) */
    int (*bind_session)(const void *io); /* currently -1/ENOTSUP; never opens a socket */
} oa_audio_iface;

/* audio.mixer — read-only separate-PID query session, loop-thread only.
 * Exactly one pending request or uncollected result. No borrowed caller data
 * or callback. Hold this service while using it; cancel abandoned tickets before
 * releasing. Tickets are provider-instance scoped and cannot cross reload.
 * gain:0 accepted/-1 errno; take:1 collected/0 pending/-1 errno, output unchanged
 * otherwise; cancel discards/drains without replay. A reply is query status,
 * never a receiver/DSP/audible-operation ACK. No setters are exposed. */
#define OA_SVC_AUDIO_MIXER "audio.mixer"
typedef struct {
    int (*gain)(unsigned device, uint64_t *ticket);
    int (*take)(uint64_t ticket, oa_audio_broker_reply *out);
    int (*cancel)(uint64_t ticket);
} oa_audio_mixer_iface;

/* settings — the settings database. Published by mod_settings. */
#define OA_SVC_SETTINGS "settings"
typedef struct {
    int (*get_int)(const char *col, int *out); /* 0 = ok */
    int (*set_int)(const char *col, int v);    /* 0 = ok, -2 = read-only */
    int (*is_writable)(void);
} oa_settings_iface;

/* ui.anim — recovered list/dialog animation. Published by mod_ui_anim.
 * set_enabled only writes the flag. start() reads ui.clock and rejects a
 * duration below 1. sample() is the timer update and accepts an explicit now. */
#define OA_SVC_UI_ANIM "ui.anim"
typedef struct {
    int (*set_enabled)(int on);
    int (*enabled)(void);
    int (*start)(int id, int kind, const oa_anim_rect *from, const oa_anim_rect *to,
                 uint32_t duration, uint32_t *started);
    int (*cancel)(int id);
    int (*finalise)(int id);
    int (*sample)(uint32_t now);
    int (*get)(int id, int *kind, oa_anim_rect *rect, float *eased, uint32_t *started,
               uint32_t *duration);
    int (*bind)(oa_anim_fn fn, void *ud);
    /* Text slide, v2: AnimationHelper calculateSlidingDuration/calculateSlidingMargin
     * (pause 800 ms, speed 50, x1000). duration_ms may be fractional; margin is in pixels. */
    float (*slide_duration)(int distance);
    int (*slide_margin)(uint32_t elapsed_ms, float duration_ms, int distance, int reverse);
} oa_ui_anim_iface;

/* ui.portal — measured home layout. Published by mod_ui_fb.
 * hit() uses the same rectangles paint uses. screen 0 is the portal and
 * screen 1 is the one selected tile. */
#define OA_SVC_PORTAL "ui.portal"
typedef struct {
    int (*hit)(int x, int y, int *band, int *index);
    int (*activate_at)(int x, int y);
    int (*screen)(void);
} oa_portal_iface;

/* input — remote frames. Published by mod_ir.
 * feed() is the UART parser. An empty ir.device leaves the port closed.
 * device_open() is 1 only while that port's fd is held. */
#define OA_SVC_INPUT "input"
typedef struct {
    int (*feed)(const uint8_t *bytes, size_t n);
    int (*device_open)(void);
} oa_input_iface;

/* source — input switching via the vendor VIN HAL. Published by mod_source. */
#define OA_SVC_SOURCE "source"
typedef struct {
    int (*select_type)(int type); /* 0 = switched, -2 = disabled by config */
    int (*current_type)(void);
    int (*is_available)(void);
} oa_source_iface;

#endif /* OPENAURORA_SERVICES_H */
