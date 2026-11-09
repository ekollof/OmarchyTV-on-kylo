/* Call-order/argument test for the recovered ICE_AudioStart external branch.
 * Fake HAL only; no TV, no vendor library. */
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "audio_source_fake.h"
#include "openaurora/audio_curve.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

extern const oa_module *oa_module_entry(void);
static const oa_audio_source_iface *iface;
static int enabled;
static int unmute;
static const char *aq_path;
static int pos;

static void log_message(void *p, int level, const char *fmt, ...)
{
    (void)p; (void)level; (void)fmt;
}
static const char *config(void *p, const char *key, const char *def)
{
    (void)p;
    if (!strcmp(key, "audio.source_enable"))
        return enabled ? "1" : "0";
    if (!strcmp(key, "audio.source.unmute"))
        return unmute ? "1" : "0";
    if (!strcmp(key, "audio.source.aq"))
        return aq_path;
    return def;
}
static int publish(void *p, const char *name, uint32_t version, const void *ifc)
{
    (void)p;
    assert(!strcmp(name, OA_SVC_AUDIO_SOURCE) && version == 1);
    iface = ifc;
    return 0;
}
static int unpublish(void *p, const char *name)
{
    (void)p; (void)name;
    assert(!strcmp(name, OA_SVC_AUDIO_SOURCE));
    iface = NULL;
    return 0;
}
static void release(void *p, const char *name) { (void)p; (void)name; }
static const oa_host_api api = {
    .size = sizeof(oa_host_api), .abi = OA_ABI_VERSION,
    .log = log_message, .config_get = config, .publish = publish,
    .unpublish = unpublish, .release = release
};

static void *start(int on)
{
    enabled = on;
    void *ctx = NULL;
    assert(oa_module_entry()->init(&api, &ctx) == 0 && ctx);
    assert(oa_module_entry()->start(ctx) == 0 && iface);
    return ctx;
}
static void done(void *ctx)
{
    oa_module_entry()->stop(ctx);
    oa_module_entry()->destroy(ctx);
    assert(!iface);
}

static void reset(void)
{
    pos = 0;
    audio_fake_reset();
}
static void expect(int kind, int a, int b, int c, const char *name)
{
    assert(pos < audio_fake_count());
    const struct audio_fake_event *ev = audio_fake_at(pos++);
    if (ev->kind != kind || ev->a != a || ev->b != b || ev->c != c ||
        (name && strcmp(ev->name ? ev->name : "", name))) {
        fprintf(stderr, "pos %d: kind %d a %d b %d c %d name %s\n", pos - 1,
                ev->kind, ev->a, ev->b, ev->c, ev->name ? ev->name : "(null)");
        assert(0);
    }
}
static void expect_at(int index, int kind, int a, int b, int c, const char *name)
{
    assert(index >= 0 && index < audio_fake_count());
    const struct audio_fake_event *ev = audio_fake_at(index);
    if (ev->kind != kind || ev->a != a || ev->b != b || ev->c != c ||
        (name && strcmp(ev->name ? ev->name : "", name))) {
        fprintf(stderr, "at %d: kind %d a %d b %d c %d name %s\n", index,
                ev->kind, ev->a, ev->b, ev->c, ev->name ? ev->name : "(null)");
        assert(0);
    }
}
static void expect_ctrl(int op, int w0, int w1, int w2, int w3)
{
    assert(pos < audio_fake_count());
    const struct audio_fake_event *ev = audio_fake_at(pos++);
    if (ev->kind != AF_CTRL || ev->a != op || ev->b != w0 || ev->c != w1 ||
        ev->d != w2 || ev->e != w3) {
        fprintf(stderr, "pos %d ctrl: op %d %d,%d,%d,%d want %d %d,%d,%d,%d\n",
                pos - 1, ev->a, ev->b, ev->c, ev->d, ev->e, op, w0, w1, w2, w3);
        assert(0);
    }
}

static void expect_send(const char *name, int src, int port)
{
    assert(pos < audio_fake_count());
    const struct audio_fake_event *ev = audio_fake_at(pos++);
    if (ev->kind != AF_SEND || ev->a != src || ev->b != port ||
        !ev->name || strcmp(ev->name, name)) {
        fprintf(stderr, "pos %d send: kind %d %d,%d name %s want %s %d,%d\n",
                pos - 1, ev->kind, ev->a, ev->b,
                ev->name ? ev->name : "(null)", name, src, port);
        assert(0);
    }
}

static void expect_dual(int op)
{
    assert(pos < audio_fake_count());
    const struct audio_fake_event *ev = audio_fake_at(pos++);
    if (ev->kind != AF_DUAL || ev->a != op) {
        fprintf(stderr, "pos %d dual: kind %d op %d want %d\n", pos - 1,
                ev->kind, ev->a, op);
        assert(0);
    }
}

static void expect_ctrl_op_count(int op, int w0, int w2, int w3)
{
    assert(pos < audio_fake_count());
    const struct audio_fake_event *ev = audio_fake_at(pos++);
    if (ev->kind != AF_CTRL || ev->a != op || ev->b != w0 ||
        ev->d != w2 || ev->e != w3) {
        fprintf(stderr, "pos %d ctrl*: op %d %d,%d,%d,%d want %d %d,*,%d,%d\n",
                pos - 1, ev->a, ev->b, ev->c, ev->d, ev->e, op, w0, w2, w3);
        assert(0);
    }
}

/* soft_mute(1,1) on the stock speaker table: 0..3, types 0..3. */
static void expect_soft_mute(void)
{
    expect(AF_KITEM, 0x43, 1, 0, NULL);
    expect(AF_SET, 0x46, 1, 0, NULL);
    expect(AF_SET, 0x4e, 1, 0, NULL);
    expect(AF_SET, 0x4b, 1, 0, NULL);
}

/* PLAT_VAUD_SwitchHP(3) on the stock speaker configuration. */
static void expect_hp_mode3(void)
{
    for (int i = 0; i < 9; i++)
        expect_ctrl(0x2c, i, 4, 0x0f, 0x01);
    for (int i = 0; i < 9; i++)
        expect_ctrl(0x2d, i, 4, 0x34, 0x00);
    expect_ctrl(0x2b, 1, 0, 0, 0);
    expect_ctrl(0x2b, 0, 0, 0, 0);
}

static void expect_tail(void)
{
    expect(AF_KITEM, 0x52, 0, 0, NULL);
    expect_hp_mode3();
}

int main(void)
{
    aq_path = "/tmp/oa-audio-source-aq.bin";
    FILE *af = fopen(aq_path, "wb");
    assert(af);
    uint8_t zbuf[OA_AUDIO_AQ_SIZE] = { 0 };
    assert(fwrite(zbuf, 1, sizeof zbuf, af) == sizeof zbuf);
    fclose(af);

    /* Disabled gate: no HAL traffic at all. */
    reset();
    void *ctx = start(0);
    assert(iface->select_source(37) == -2);
    assert(audio_fake_count() == 0);
    assert(iface->select_video_type(17) == -2);
    assert(audio_fake_count() == 0);
    assert(iface->current_source() == -1 && iface->is_available() == 1);
    done(ctx);

    /* HDMI-class source 37 -> table 0x801. */
    reset();
    ctx = start(1);
    assert(iface->select_source(37) == 0);
    expect_ctrl(0x11, 1, 0, 0, 0);
    expect_ctrl(0x40, 0, 0, 0, 0);
    expect_ctrl(0x1e, 1, 0, 0, 0);
    expect_ctrl(0x37, 0, 0, 0, 0);
    expect_ctrl(0x1e, 0, 0, 0, 0);
    expect_dual(0x4a);
    expect_ctrl_op_count(0x2e, 0, 101, 0);
    expect_ctrl(0x3e, 0, 0, 0, 0);
    expect_send("MuxInput", 0x801, 0);
    expect_soft_mute();
    expect_ctrl(0x2a, 0xfff, 1, 0, 0);
    expect_ctrl(0x2a, 0x802, 0, 0, 0);
    expect(AF_SELECT, 0, 1, 0, "adc on");
    expect(AF_SELECT, 2, 0, 0, "decoder0");
    expect(AF_SELECT, 3, 1, 0, "hdmirx audio on");
    expect(AF_SELECT, 0x32, 1, 0, "dec0 open");
    expect(AF_SELECT, 0x32, 2, 0, ""); /* no id-2 record, as in the vendor table */
    expect(AF_SELECT, 5, 0x801, 0, "HDMI ES");
    expect_tail();
    assert(pos == audio_fake_count());
    assert(iface->current_source() == 37);

    /* Same source again: AIN is cached, but the start sequence still runs. */
    reset();
    assert(iface->select_source(37) == 0);
    expect_soft_mute();
    expect_tail();
    assert(pos == audio_fake_count());

    /* ATV source 8 -> table 0x101: no PIP/ADEC, EasySelect(1) and path 5. */
    reset();
    assert(iface->select_source(8) == 0);
    expect_send("MuxInput", 0x101, 8);
    expect_soft_mute();
    expect(AF_SELECT, 1, 1, 0, "atv on");
    expect(AF_SELECT, 5, 0x101, 0, "ATV");
    expect_tail();
    assert(pos == audio_fake_count());
    assert(iface->current_source() == 8);

    /* Source 10 is the vendor's "skip AIN connect" special case. */
    reset();
    assert(iface->select_source(10) == 0);
    expect_send("MuxInput", 0x201, 0);
    expect_soft_mute();
    expect_tail();
    assert(pos == audio_fake_count());

    /* Bronlijst video type 17 -> HDMI1 audio device 0x25 -> table 0x801. */
    reset();
    assert(iface->select_video_type(17) == 0);
    expect_send("MuxInput", 0x801, 0);
    expect_soft_mute();
    expect_ctrl(0x2a, 0xfff, 1, 0, 0);
    expect_ctrl(0x2a, 0x802, 0, 0, 0);
    expect(AF_SELECT, 0, 1, 0, "adc on");
    expect(AF_SELECT, 2, 0, 0, "decoder0");
    expect(AF_SELECT, 3, 1, 0, "hdmirx audio on");
    expect(AF_SELECT, 0x32, 1, 0, "dec0 open");
    expect(AF_SELECT, 0x32, 2, 0, "");
    expect(AF_SELECT, 5, 0x801, 0, "HDMI ES");
    expect_tail();
    assert(pos == audio_fake_count());
    assert(iface->current_source() == 37);

    /* Video type 15 -> CVBS audio 0x23 -> table 0x301 -> AV. */
    reset();
    assert(iface->select_video_type(15) == 0);
    expect_send("MuxInput", 0x301, 1);
    expect_soft_mute();
    expect(AF_SELECT, 5, 0x301, 0, "AV");
    expect(AF_DELAY, 200, 0, 0, NULL); /* CVBS is an ADC source (0x23) */
    expect_tail();
    assert(pos == audio_fake_count());
    assert(iface->current_source() == 0x23);

    /* TV and unknown video types have no external interface. */
    reset();
    assert(iface->select_video_type(0) == -1);
    assert(iface->select_video_type(99) == -1);
    assert(audio_fake_count() == 0);

    /* Opt-in speaker release mirroring _ice_AudioSwitchPath2MM. */
    done(ctx);
    unmute = 1;
    reset();
    ctx = start(1);
    assert(iface->select_video_type(17) == 0);
    int n = audio_fake_count();
    assert(n >= 4);
    expect_at(n - 4, AF_KITEM, 0x43, 0, 0, NULL);
    expect_at(n - 3, AF_SET, 0x46, 0, 0, NULL);
    expect_at(n - 2, AF_SET, 0x4e, 0, 0, NULL);
    expect_at(n - 1, AF_SET, 0x4b, 0, 0, NULL);
    assert(iface->current_source() == 37);
    done(ctx);
    unmute = 0;
    ctx = start(1);

    /* Out-of-range device source. */
    reset();
    assert(iface->select_source(56) == -1);
    assert(audio_fake_count() == 0);
    assert(iface->select_source(-1) == -1);
    assert(audio_fake_count() == 0);
    done(ctx);
    unlink(aq_path);

    puts("ok audio source: soft mute, AIN/ADEC/audio EasySelect, output mute, HP, AQ table, unmute, video map, gate");
    return 0;
}
