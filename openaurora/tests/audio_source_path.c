/* Call-order/argument test for the recovered ICE_AudioStart external branch.
 * Fake HAL only; no TV, no vendor library. */
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio_source_fake.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

extern const oa_module *oa_module_entry(void);
static const oa_audio_source_iface *iface;
static int enabled;
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
    (void)p;
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
static const struct audio_fake_event *next(void)
{
    assert(pos < audio_fake_count());
    return audio_fake_at(pos++);
}
static void expect(int kind, int a, int b, int c, const char *name)
{
    const struct audio_fake_event *ev = next();
    if (ev->kind != kind || ev->a != a || ev->b != b || ev->c != c ||
        (name && strcmp(ev->name ? ev->name : "", name))) {
        fprintf(stderr, "pos %d: kind %d a %d b %d c %d name %s\n", pos - 1,
                ev->kind, ev->a, ev->b, ev->c, ev->name ? ev->name : "(null)");
        assert(0);
    }
}

static void expect_mute_pair(int code, int value)
{
    expect(AF_KITEM, code, value, 0, NULL);
}

/* soft_mute(1,1) on the stock speaker table: 0..3, types 0..3. */
static void expect_soft_mute(void)
{
    expect_mute_pair(0x43, 1);
    expect(AF_SET, 0x46, 1, 0, NULL);
    expect(AF_SET, 0x4e, 1, 0, NULL);
    expect(AF_SET, 0x4b, 1, 0, NULL);
}

int main(void)
{
    /* Disabled gate: no HAL traffic at all. */
    reset();
    void *ctx = start(0);
    assert(iface->select_source(37) == -2);
    assert(audio_fake_count() == 0);
    assert(iface->current_source() == -1 && iface->is_available() == 1);
    done(ctx);

    /* HDMI-class source 37 -> table 0x801. */
    reset();
    ctx = start(1);
    assert(iface->select_source(37) == 0);
    expect_soft_mute();
    expect(AF_CTRL, 0x2a, 0xfff, 1, NULL);
    expect(AF_CTRL, 0x2a, 0x802, 0, NULL);
    expect(AF_SELECT, 0, 1, 0, "adc on");
    expect(AF_SELECT, 2, 0, 0, "decoder0");
    expect(AF_SELECT, 3, 1, 0, "hdmirx audio on");
    expect(AF_SELECT, 0x32, 1, 0, "dec0 open");
    expect(AF_SELECT, 0x32, 2, 0, ""); /* no id-2 record, as in the vendor table */
    expect(AF_SELECT, 5, 0x801, 0, "HDMI ES");
    expect_mute_pair(0x52, 0);
    assert(pos == audio_fake_count());
    assert(iface->current_source() == 37);

    /* Same source again: AIN is cached, but the start sequence still runs. */
    reset();
    assert(iface->select_source(37) == 0);
    expect_soft_mute();
    expect_mute_pair(0x52, 0);
    assert(pos == audio_fake_count());

    /* ATV source 8 -> table 0x101: no PIP/ADEC, EasySelect(1) and path 5. */
    reset();
    assert(iface->select_source(8) == 0);
    expect_soft_mute();
    expect(AF_SELECT, 1, 1, 0, "atv on");
    expect(AF_SELECT, 5, 0x101, 0, "ATV");
    expect_mute_pair(0x52, 0);
    assert(pos == audio_fake_count());
    assert(iface->current_source() == 8);

    /* Source 10 is the vendor's "skip AIN connect" special case. */
    reset();
    assert(iface->select_source(10) == 0);
    expect_soft_mute();
    expect_mute_pair(0x52, 0);
    assert(pos == audio_fake_count());

    /* Out-of-range device source. */
    reset();
    assert(iface->select_source(56) == -1);
    assert(audio_fake_count() == 0);
    assert(iface->select_source(-1) == -1);
    assert(audio_fake_count() == 0);
    done(ctx);

    puts("ok audio source: soft mute, AIN/ADEC/audio EasySelect, output mute, gate");
    return 0;
}
