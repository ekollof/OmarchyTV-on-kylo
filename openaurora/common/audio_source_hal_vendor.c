/* Production audio-source HAL: direct libNDAL3 calls. */
#include "openaurora/audio_source_hal.h"

#include <stdint.h>

#ifdef OA_NO_VENDOR
static int no_ctrl(int op, void *params)
{
    (void)op; (void)params;
    return -1;
}
static int no_select(int path, int id, const void *records)
{
    (void)path; (void)id; (void)records;
    return -1;
}
static int no_set(int item, const void *value, int zero)
{
    (void)item; (void)value; (void)zero;
    return -1;
}
static int no_kitem(int code, const void *value)
{
    (void)code; (void)value;
    return -1;
}
static int no_send(const char *name, void *params, int len)
{
    (void)name; (void)params; (void)len;
    return -1;
}
static int no_dual(int op, void *params)
{
    (void)op; (void)params;
    return -1;
}
static void no_delay(unsigned ms) { (void)ms; }
static const struct oa_audio_source_hal g_hal = {
    no_ctrl, no_select, no_set, no_kitem, no_send, no_dual, no_delay
};
#else
extern int Hal_Audio_Ctrl(int op, void *params);
extern int Hal_Audio_EasySelect(int path, int id, const void *records);
extern int Hal_Audio_EasySet(int item, const void *value, int zero);
extern void *Hal_Audio_KItem(int code);
extern int Hal_Audio_SetControl(void *kitem, void *params);
extern int Hal_Audio_Send(const char *name, void *params, int len);
extern int Hal_Audio_DualCmd(int op, void *params);
extern int usleep(unsigned usec);

static int hal_ctrl(int op, void *params) { return Hal_Audio_Ctrl(op, params); }
static int hal_select(int path, int id, const void *records)
{
    return Hal_Audio_EasySelect(path, id, records);
}
static int hal_set(int item, const void *value, int zero)
{
    return Hal_Audio_EasySet(item, value, zero);
}
/* PLAT_AOUT_SetMute builds {code, &value, 0} on the stack and passes it to
 * Hal_Audio_SetControl, which reads the value through param_2+4. KItem maps
 * the code to the control entry (0x43 main speaker, 0x52/0x5b output). */
static int hal_kitem(int code, const void *value)
{
    void *kitem = Hal_Audio_KItem(code);
    if (!kitem)
        return -1;
    uint32_t params[3] = { (uint32_t)code, (uint32_t)(uintptr_t)value, 0 };
    return Hal_Audio_SetControl(kitem, params);
}
static void hal_delay(unsigned ms)
{
    if (ms)
        (void)usleep(ms * 1000u);
}
static int hal_send(const char *name, void *params, int len)
{
    return Hal_Audio_Send(name, params, len);
}
static int hal_dual(int op, void *params)
{
    return Hal_Audio_DualCmd(op, params);
}
static const struct oa_audio_source_hal g_hal = {
    hal_ctrl, hal_select, hal_set, hal_kitem, hal_send, hal_dual, hal_delay
};
#endif

const struct oa_audio_source_hal *oa_audio_source_hal(void)
{
    return &g_hal;
}
