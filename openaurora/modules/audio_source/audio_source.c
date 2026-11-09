/*
 * mod_audio_source — recovered ICE_AudioStart external-source branch.
 *
 * Firmware chain (AUDIO-SOURCE-TRANSPORT-2026-11-09):
 *   DvbAudioPlayer::startNonDVBAudio
 *     -> IceAudioIF vtable +0x20 -> ICE_AudioStart 0x007e2114
 *     -> PLAT_VAUD_SetSoftMute / PLAT_AOUT_SpeakerConnect /
 *        PLAT_AIN_Connect / PLAT_AIN_Enable / PLAT_AOUT_Connect /
 *        PLAT_AOUT_SetMute
 *
 * The PLAT functions are aurora-internal; only the Hal_Audio_* calls are
 * exported. This module mirrors the PLAT state machines and calls the HAL
 * through oa_audio_source_hal(). It is gated by audio.source_enable (0).
 *
 * Recovered details:
 *   - source table 0x012d818c (56 ints), indexed by ICE_DeviceSource;
 *   - PLAT_AIN_Connect(in, src, 0): for src class 0x800 -> Hal_Audio_Ctrl(0x2a,
 *     {0xfff,1}) + {0x802,0} + plat_adec_connect(0,2) for 0x801..0x804; for
 *     class 0x100 -> Hal_Audio_EasySelect(1,1,&atv_on); then
 *     plat_audio_connect(in,src) -> EasySelect(5/8, target, &easy_table);
 *   - plat_adec_connect(0,2) -> the five EasySelect calls below;
 *   - PLAT_AOUT_SetMute(0,0) -> Hal_Audio_KItem(0x52) + SetControl;
 *   - PLAT_VAUD_SetSoftMute(1,1) -> speaker mutes 0..3 via KItem(0x43) /
 *     EasySet(0x46/0x4e/0x4b), using the stock speaker types 0..3.
 *
 * Not ported (deliberately, see the report): the sound-output/HP detection
 * policy (PLAT_VAUD_SwitchHP), foreground/scart routing, speaker main gain
 * and the full PLAT_AOUT/PLAT_AIN tables. The source list is device-source
 * based; mapping a video source type to an ICE_DeviceSource still needs the
 * ExternalAVSourceManager table.
 */
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openaurora/audio_curve.h"
#include "openaurora/audio_source_hal.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

struct rec {
    int id;
    char name[0x28];
};

/* All record tables passed to EasySelect are 16 entries so the host fake can
 * scan them without knowing the vendor's table extent. The real tables are
 * searched until the id matches; padding with id 0/empty name only affects
 * lookups that the vendor also would not find (e.g. dec0_open id 2). */
static const struct rec g_adc_on_tab[16] = { { 1, "adc on" } };
static const struct rec g_atv_on_tab[16] = { { 1, "atv on" } };
static const struct rec g_hdmirx_on_tab[16] = { { 1, "hdmirx audio on" } };
static const struct rec g_dec0_open_tab[16] = { { 1, "dec0 open" } };
static const struct rec g_decoder0_tab[16] = { { 0, "decoder0" } };

/* EasySelect record table at 0x01773180, first 16 entries; the HAL matches
 * the id and compares the record name with the ALSA control name. */
static const struct rec g_easy_table[16] = {
    { 0x0201, "DTV DEC0" },
    { 0x0202, "DTV DEC1" },
    { 0x0801, "HDMI ES" },
    { 0x0801, "HDMI PCM" },
    { 0x0501, "LineIn/Composite" },
    { 0x1501, "SCART" },
    { 0x0101, "ATV" },
    { 0x1200, "MM" },
    { 0x0801, "HDMI" },
    { 0x1100, "PCM" },
    { 0x1900, "I2S" },
    { 0x0301, "AV" },
    { 0x0701, "DVI" },
    { 0x0601, "VGA" },
    { 0x1800, "SPDIF" },
    { 0x7fff, "None" },
};

/* source table 0x012d818c, 56 entries */
static const int g_ain_source[56] = {
    0x201, 0x202, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff,
    0x101, 0x102, 0x201, 0x1200, 0x1201, 0x201, 0x7fff, 0x7fff,
    0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff,
    0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff,
    0x7fff, 0x7fff, 0x1501, 0x301, 0x601, 0x801, 0x802, 0x803,
    0x804, 0x301, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff,
    0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x1800, 0x7fff, 0x7fff, 0x201,
};

#define SPK_COUNT 9

struct ctx {
    const oa_host_api *api;
    const struct oa_audio_source_hal *h;
    int enabled;
    int current;
    int soft_last;
    int mute_mask;       /* DAT_0197f99c, stock 0 */
    int ain_id[2];       /* *puVar3, stock 0 */
    int ain_src[2];
    int aout_in[3];
    int spk_out[SPK_COUNT];
    int spk_type[SPK_COUNT];
    int spk_found[SPK_COUNT];
    int spk_mute[SPK_COUNT];
    unsigned adc_delay_ms;
    int sound_output;
    int ep_plugged;
    int unmute;          /* mirror _ice_AudioSwitchPath2MM's SetSoftMute(0,1) */
    int ain_ready;
    int ain_handle;
    int path_src;
    int path_port;
    int vaud_ready;
    oa_audio_curve vol_curve;
};

static struct ctx *g_ctx;

static int is_adc_source(int src)
{
    if (src < 0x22 || src > 0x29)
        return 0;
    return (1 << (src - 0x22)) & 0x87;
}

static int ctrl_words(struct ctx *c, int op, int w0, int w1, int w2, int w3)
{
    if (!c->h->audio_ctrl)
        return -1;
    uint32_t params[0x38 / 4];
    memset(params, 0, sizeof params);
    params[0] = (uint32_t)w0;
    params[1] = (uint32_t)w1;
    params[2] = (uint32_t)w2;
    params[3] = (uint32_t)w3;
    return c->h->audio_ctrl(op, params);
}
static int ctrl_pip(struct ctx *c, int code, int value)
{
    return ctrl_words(c, 0x2a, code, value, 0, 0);
}

/* PLAT_AIN_Init/Open from PLAT_INI_Init/ICE_AudioOpen. The vendor runs this
 * at boot; the service runs it lazily before the first source. PLAT_AIN_Open
 * stores a software handle, not an fd. */
static void ain_init(struct ctx *c)
{
    (void)ctrl_words(c, 0x11, 1, 0, 0, 0);
    (void)ctrl_words(c, 0x40, 0, 0, 0, 0);
    (void)ctrl_words(c, 0x1e, 1, 0, 0, 0);
    c->ain_handle = 0x12345678;
    c->ain_ready = 1;
    c->api->log(c->api->priv, OA_LOG_INFO,
                "audio source: AIN init (0x11/0x40/0x1e, handle 0x12345678)");
}

static int cfg_int(struct ctx *c, const char *key, int def);

/* PLAT_VAUD_InitAQSetting -> PLAT_VAUD_SetVolumeTable: the 101-entry AQ curve
 * is sent with Hal_Audio_Ctrl(0x2e,{0,table,101}) and then Ctrl(0x3e,{0}).
 * The existing audio_curve parser decodes the same AQ.bin layout. */
static int load_aq(struct ctx *c)
{
    const char *path = c->api->config_get(c->api->priv, "audio.source.aq",
                                          "/conf/AQ.bin");
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    uint8_t buf[OA_AUDIO_AQ_SIZE];
    size_t n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    if (n != sizeof buf)
        return -1;
    int profile = cfg_int(c, "audio.source.aq_profile", 0);
    return oa_audio_curve_from_aq(&c->vol_curve, buf, n, profile);
}

/* PLAT_VAUD_Init from ICE_AudioInit: Ctrl 0x37 and 0x1e with zeroed 0x38
 * payloads, then DualCmd 0x4a. The AQ volume table follows; the AQ/PP
 * post-param tables (EQ/AVL) are not ported. */
static void vaud_init(struct ctx *c)
{
    (void)ctrl_words(c, 0x37, 0, 0, 0, 0);
    (void)ctrl_words(c, 0x1e, 0, 0, 0, 0);
    uint32_t params[0x38 / 4];
    memset(params, 0, sizeof params);
    int rc = c->h->audio_dual_cmd ? c->h->audio_dual_cmd(0x4a, params) : -1;
    int aq = load_aq(c);
    if (aq == 0) {
        (void)ctrl_words(c, 0x2e, 0, (int)(uintptr_t)c->vol_curve.points, 101, 0);
        (void)ctrl_words(c, 0x3e, 0, 0, 0, 0);
    }
    c->vaud_ready = 1;
    c->api->log(c->api->priv, OA_LOG_INFO,
                "audio source: VAUD init (0x37/0x1e/0x4a rc %d, AQ %s)", rc,
                aq == 0 ? "loaded" : "unavailable");
}

/* PLAT_AIN_SetPathConfig(source, port): Hal_Audio_Send("MuxInput",
 * {source, port}, 0x38). Ports are the PLAT_VAUD_Init pairs for the sources
 * this service uses; 0x7fff is a no-op in the vendor. */
static int ain_path_port(int src)
{
    switch (src) {
    case 0x101: return 8;
    case 0x201: case 0x801: case 0x802: case 0x1100: case 0x1900:
    case 0x1800: return 0;
    case 0x1501: return 2;
    case 0x601: return 3;
    case 0x301: case 0x302: return 1;
    default: return 0;
    }
}
static int ain_path_config(struct ctx *c, int src)
{
    if (src == 0x7fff)
        return 0;
    int port = ain_path_port(src);
    if (c->path_src == src && c->path_port == port)
        return 0;
    uint32_t params[0x38 / 4];
    memset(params, 0, sizeof params);
    params[0] = (uint32_t)src;
    params[1] = (uint32_t)port;
    int rc = c->h->audio_send ? c->h->audio_send("MuxInput", params, 0x38) : -1;
    c->path_src = src;
    c->path_port = port;
    c->api->log(c->api->priv, OA_LOG_INFO,
                "audio source: MuxInput 0x%x port %d (rc %d)", src, port, rc);
    return rc;
}

static int speaker_mute(struct ctx *c, int spk, int mask)
{
    if (spk < 0 || spk >= SPK_COUNT || !c->spk_found[spk])
        return -1;
    c->spk_mute[spk] = mask ? 1 : 0;
    int value = mask ? 1 : 0;
    switch (c->spk_type[spk]) {
    case 0:
        return c->h->kitem_set ? c->h->kitem_set(0x43, &value) : -1;
    case 1:
        return c->h->easy_set ? c->h->easy_set(0x46, &value, 0) : -1;
    case 2:
        return c->h->easy_set ? c->h->easy_set(0x4e, &value, 0) : -1;
    case 3:
        return c->h->easy_set ? c->h->easy_set(0x4b, &value, 0) : -1;
    default:
        return -1;
    }
}

/* PLAT_VAUD_SetSoftMute + FUN_008d11f0: the stock DAT_0176da40/da00/da6c
 * speaker ids are 0/1/2 and the fourth is 3; DAT_0197f99c (the output-mute
 * mask) is 0 on this set. */
static int soft_mute(struct ctx *c, int mask, int second)
{
    if (mask == c->soft_last && second == 0)
        return 1;
    if (second <= mask || c->soft_last == 0)
        (void)speaker_mute(c, 0, c->soft_last | mask);
    if (mask & c->mute_mask) {
        int value = 0;
        if (c->h->kitem_set)
            (void)c->h->kitem_set(c->aout_in[0] == 0 ? 0x52 : 0x5b, &value);
    }
    if (second != 0) {
        (void)speaker_mute(c, 1, c->soft_last | mask);
        (void)speaker_mute(c, 2, c->soft_last | mask);
        (void)speaker_mute(c, 3, c->soft_last | mask);
    }
    if (second == 0)
        c->soft_last = mask;
    return 1;
}

static int aout_mute(struct ctx *c, int out, int mute)
{
    if (out < 0 || out > 2 || !c->h->kitem_set)
        return -1;
    int code = c->aout_in[out] == 0 ? 0x52 : 0x5b;
    int value = mute ? 1 : 0;
    return c->h->kitem_set(code, &value);
}

/* plat_adec_connect(0, 2): recovered EasySelect chain. */
static int adec_connect(struct ctx *c, int dec, int input_type)
{
    if (!c->h->easy_select || dec != 0 || input_type != 2)
        return -1;
    (void)c->h->easy_select(0, 1, g_adc_on_tab);
    (void)c->h->easy_select(2, 0, g_decoder0_tab);
    (void)c->h->easy_select(3, 1, g_hdmirx_on_tab);
    (void)c->h->easy_select(0x32, 1, g_dec0_open_tab);
    (void)c->h->easy_select(0x32, input_type, g_dec0_open_tab);
    return 0;
}

/* plat_audio_connect(in, src): path 5 for input 0, 8 otherwise; target is
 * 0x801 for the HDMI class, 0x1200 for the MM class, else src. */
static int audio_connect(struct ctx *c, int in, int src)
{
    if (!c->h->easy_select)
        return -1;
    int target = src;
    if (src & 0x800)
        target = 0x801;
    else if ((src & 0x1200) == 0x1200)
        target = 0x1200;
    int path = in == 0 ? 5 : 8;
    return c->h->easy_select(path, target, g_easy_table);
}

/* PLAT_AIN_Connect(in, src, 0). */
static int ain_connect(struct ctx *c, int in, int src, int unused)
{
    (void)unused;
    if (src == 0x7fff || in < 0 || in > 1)
        return -1;
    if (c->ain_src[in] == src)
        return 0;
    c->ain_src[in] = src;
    if ((src & 0xf00) == 0x800) {
        (void)ctrl_pip(c, 0xfff, 1);
        (void)ctrl_pip(c, 0x802, 0);
        if ((unsigned)(src - 0x801) < 4)
            (void)adec_connect(c, 0, 2);
    } else if ((unsigned)(src - 0x801) < 4) {
        (void)adec_connect(c, 0, 2);
    } else if ((src & 0xf00) == 0x100) {
        if (c->h->easy_select)
            (void)c->h->easy_select(1, 1, g_atv_on_tab);
    }
    return audio_connect(c, in, src);
}

/* PLAT_VAUD_SwitchHP's stock speaker configuration branch. Table values are
 * PLAT profile state read from the running stock process (0x0198128c, 5
 * records of four bytes). SetSpeakerDACCICGain loops spin 0..8 for each of
 * the two gain types; FUN_008d0dc4(p) is Ctrl(0x2b,{p^1}). */
static const int g_hp_gain[5][4] = {
    { 0x12, 0x02, 0x36, 0x00 },
    { 0x0f, 0x01, 0x34, 0x00 },
    { 0x08, 0x00, 0x3b, 0x00 },
    { 0x0f, 0x01, 0x34, 0x00 },
    { 0x08, 0x00, 0x3b, 0x00 },
};

/* _ice_Sound_Control_GetSoundOutput + DEV_GPIO_EP_IsPlugIn policy from
 * ICE_AudioStart 0x007e23b6..0x007e2450. Stock values: sound output 1,
 * EP not plugged -> mode 3. */
static int hp_mode(struct ctx *c)
{
    if (c->sound_output == 3)
        return c->ep_plugged ? 1 : 0;
    if (c->sound_output == 4)
        return 2;
    if (c->sound_output == 0 && !c->ep_plugged)
        return 0;
    return 3;
}

static void switch_hp(struct ctx *c, int mode)
{
    if (mode < 0 || mode > 4) {
        (void)ctrl_words(c, 0x2b, 1, 0, 0, 0);
        (void)ctrl_words(c, 0x2b, 0, 0, 0, 0);
        return;
    }
    const int *g = g_hp_gain[mode];
    /* SetSpeakerDACCICGain(4, 0, g[0], g[1]) and (4, 1, g[2], g[3]) */
    for (int i = 0; i < 9; i++)
        (void)ctrl_words(c, 0x2c, i, 4, g[0], g[1]);
    for (int i = 0; i < 9; i++)
        (void)ctrl_words(c, 0x2d, i, 4, g[2], g[3]);
    (void)ctrl_words(c, 0x2b, 1, 0, 0, 0);
    (void)ctrl_words(c, 0x2b, 0, 0, 0, 0);
}

static int select_source(struct ctx *c, int src)
{
    if (!c->enabled)
        return -2;
    if (src < 0 || src > 55)
        return -1;
    int mapped = g_ain_source[src];
    if (!c->ain_ready)
        ain_init(c);
    if (!c->vaud_ready)
        vaud_init(c);
    (void)ain_path_config(c, mapped);
    (void)soft_mute(c, 1, 1);
    for (int spk = 0; spk < 4; spk++)
        c->spk_out[spk] = 0; /* PLAT_AOUT_SpeakerConnect(spk, 0), state only */

    if (src != 10) {
        (void)ain_connect(c, c->ain_id[0], mapped, 0);
        /* PLAT_AIN_Enable is a range check + trace only on this firmware. */
        if (c->ain_id[0] < 0 || c->ain_id[0] > 1)
            return -1;
    }
    if (is_adc_source(src) && c->h->delay_ms)
        c->h->delay_ms(c->adc_delay_ms);

    /* PLAT_AOUT_Connect(0, mapped, ain_id): state only on this firmware. */
    c->aout_in[0] = c->ain_id[0];
    (void)aout_mute(c, 0, 0);
    switch_hp(c, hp_mode(c));
    /* The HDMI branch of ICE_AudioStart has no explicit soft-mute release;
     * _ice_AudioSwitchPath2MM ends with SetSoftMute(0,1). This opt-in mirrors
     * that recovered unmute for the acceptance test. */
    if (c->unmute) {
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "audio source: speaker release (0,1)");
        (void)soft_mute(c, 0, 1);
    }
    c->current = src;
    c->api->log(c->api->priv, OA_LOG_INFO,
                "audio source -> %d (ain 0x%x, out0 in %d)", src, mapped,
                c->aout_in[0]);
    return 0;
}

/* ExternalAVSourceManager interface table (0x0175de8c, 13 records of 0x38)
 * video device -> audio device. The Bronlijst source types 15..20 are the
 * video-device values; HDMI4 (22) is included for completeness. TV (0) uses
 * the DVB/ATV route and has no external interface. */
static int video_audio_device(int video_type)
{
    switch (video_type) {
    case 15: return 0x23; /* CVBS, EXT index 2 */
    case 16: return 0x29; /* VGA/PC, index 9 */
    case 17: return 0x25; /* HDMI1, index 4 */
    case 18: return 0x26; /* HDMI2, index 5 */
    case 19: return 0x27; /* HDMI3, index 6 */
    case 20: return 0x29; /* YPbPr, index 8 */
    case 22: return 0x28; /* HDMI4, index 7 */
    default: return -1;
    }
}

static int pub_select_video(int video_type)
{
    if (!g_ctx)
        return -1;
    int dev = video_audio_device(video_type);
    if (dev < 0)
        return -1;
    return select_source(g_ctx, dev);
}

static int iface_current(void)
{
    return g_ctx ? g_ctx->current : -1;
}
static int iface_available(void)
{
    return g_ctx && g_ctx->h ? 1 : 0;
}
static int pub_select(int src)
{
    return g_ctx ? select_source(g_ctx, src) : -1;
}
static const oa_audio_source_iface g_iface = {
    .select_source = pub_select,
    .select_video_type = pub_select_video,
    .current_source = iface_current,
    .is_available = iface_available
};

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof(oa_host_api) || api->abi != OA_ABI_VERSION)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c)
        return -1;
    c->api = api;
    c->h = oa_audio_source_hal();
    c->current = -1;
    c->soft_last = 0;
    c->mute_mask = 0;
    c->ain_ready = 0;
    c->path_src = -1;
    c->path_port = -1;
    c->vaud_ready = 0;
    c->ain_id[0] = c->ain_id[1] = 0;
    c->ain_src[0] = c->ain_src[1] = -1;
    c->aout_in[0] = c->aout_in[1] = c->aout_in[2] = -1;
    for (int i = 0; i < SPK_COUNT; i++) {
        c->spk_out[i] = 0x7fff;
        c->spk_found[i] = 0;
        c->spk_type[i] = i;
        c->spk_mute[i] = 1;
    }
    *out = c;
    g_ctx = c;
    return 0;
}

static int cfg_int(struct ctx *c, const char *key, int def)
{
    const char *v = c->api->config_get(c->api->priv, key, NULL);
    if (!v || !*v)
        return def;
    return atoi(v);
}

static int m_start(void *v)
{
    struct ctx *c = v;
    if (!c->h)
        return -1;
    c->enabled = c->api->config_get(c->api->priv, "audio.source_enable", "0")[0] == '1';
    c->adc_delay_ms = (unsigned)cfg_int(c, "audio.source.adc_delay_ms", 200);
    if (c->adc_delay_ms > 2000)
        c->adc_delay_ms = 2000;
    c->mute_mask = cfg_int(c, "audio.source.mute_mask", 0);
    c->sound_output = cfg_int(c, "audio.source.sound_output", 1);
    if (c->sound_output < 0 || c->sound_output > 4)
        c->sound_output = 1;
    c->ep_plugged = cfg_int(c, "audio.source.ep_plugged", 0) != 0;
    c->unmute = cfg_int(c, "audio.source.unmute", 0) != 0;
    /* Stock speaker table: 0..3 exist with types 0..3; 4..8 do not. */
    for (int i = 0; i < 4; i++) {
        char key[32];
        snprintf(key, sizeof key, "audio.source.spk%d_type", i);
        c->spk_type[i] = cfg_int(c, key, i);
        if (c->spk_type[i] < 0 || c->spk_type[i] > 3)
            c->spk_type[i] = i;
        c->spk_found[i] = 1;
    }
    c->current = -1;
    c->soft_last = 0;
    c->ain_src[0] = c->ain_src[1] = -1;
    c->ain_ready = 0;
    c->path_src = -1;
    c->path_port = -1;
    c->vaud_ready = 0;
    if (c->api->publish(c->api->priv, OA_SVC_AUDIO_SOURCE, 1, &g_iface) != 0)
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "audio_source service already published");
    c->api->log(c->api->priv, OA_LOG_INFO,
                "audio_source: ICE_AudioStart external branch %s "
                "(unmute %d, sound %d, ep %d)",
                c->enabled ? "enabled" : "disabled (audio.source_enable=1 to allow)",
                c->unmute, c->sound_output, c->ep_plugged);
    return 0;
}

static int m_quiesce(void *v)
{
    struct ctx *c = v;
    if (!c) return 0;
    c->enabled = 0;
    c->api->release(c->api->priv, OA_SVC_AUDIO_SOURCE);
    c->api->unpublish(c->api->priv, OA_SVC_AUDIO_SOURCE);
    return 0;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    c->enabled = 0;
    c->api->release(c->api->priv, OA_SVC_AUDIO_SOURCE);
    c->api->unpublish(c->api->priv, OA_SVC_AUDIO_SOURCE);
    c->current = -1;
}

static void m_destroy(void *v)
{
    if (g_ctx == v)
        g_ctx = NULL;
    free(v);
}

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC,
    .abi = OA_ABI_VERSION,
    .size = sizeof(oa_module),
    .name = "audio_source",
    .version = "0.1",
    .requires = { "", 0 },
    .init = m_init,
    .start = m_start,
    .stop = m_stop,
    .destroy = m_destroy,
    .quiesce = m_quiesce,
};

OA_MODULE_EXPORT(g_module)
