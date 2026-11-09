/* Host fake for oa_audio_source_hal. Records the raw HAL calls in order and
 * resolves the EasySelect record name the same way the vendor HAL would. */
#include <stdint.h>
#include <string.h>

#include "audio_source_fake.h"
#include "openaurora/audio_source_hal.h"

struct rec {
    int id;
    char name[0x28];
};

static struct audio_fake_event g_ev[128];
static int g_n;

static void rec(int kind, int a, int b, int c, const char *name)
{
    if (g_n >= (int)(sizeof g_ev / sizeof g_ev[0]))
        return;
    g_ev[g_n++] = (struct audio_fake_event){ kind, a, b, c, name };
}

void audio_fake_reset(void) { g_n = 0; }
int audio_fake_count(void) { return g_n; }
const struct audio_fake_event *audio_fake_at(int index)
{
    return index >= 0 && index < g_n ? &g_ev[index] : NULL;
}

static const char *lookup(const struct rec *t, int id)
{
    for (int i = 0; i < 16; i++) {
        if (t[i].id == id && t[i].name[0])
            return t[i].name;
    }
    return "";
}

static int f_ctrl(int op, void *params)
{
    const uint32_t *w = params;
    rec(AF_CTRL, op, (int)w[0], (int)w[1], NULL);
    return 0;
}
static int f_select(int path, int id, const void *records)
{
    const struct rec *t = records;
    rec(AF_SELECT, path, id, 0, t ? lookup(t, id) : "");
    return 0;
}
static int f_set(int item, const void *value, int zero)
{
    rec(AF_SET, item, value ? *(const int *)value : -1, zero, NULL);
    return 0;
}
static int f_kitem(int code, const void *value)
{
    rec(AF_KITEM, code, value ? *(const int *)value : -1, 0, NULL);
    return 0;
}
static void f_delay(unsigned ms) { rec(AF_DELAY, (int)ms, 0, 0, NULL); }

static const struct oa_audio_source_hal g_fake = {
    f_ctrl, f_select, f_set, f_kitem, f_delay
};

const struct oa_audio_source_hal *oa_audio_source_hal(void)
{
    return &g_fake;
}
