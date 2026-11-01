/* Real platform callbacks; every HAL entry is a local fake. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/vendor_audio.h"
extern const oa_module *oa_module_entry(void);
static int calls, failed, later;
static const oa_plat_iface *service;
static int prelude(int stage) { assert(calls++ == stage); return failed == stage ? -9 : 0; }
int Hal_Audio_Protect_Init(void) { return prelude(0); }
int Hal_Audio_Mixer_Ctrl_Init(void) { return prelude(1); }
int Hal_Audio_Proxy_Init(void) { return prelude(2); }
void Hal_Video_Vpltio_Init(void) { later++; }
void Hal_Video_V4l2InfoInit(void) { later++; }
void Hal_Video_DrmShareMemCreate(void) { later++; }
void Hal_Video_DrmInit(int enable) { assert(enable == 1); later++; }
int Hal_Gpio_Init(void) { later++; return 0; }
int Hal_Iic_Init(void) { later++; return 0; }
void NDAL_VideoExt_OpenDevice(int flag, unsigned index) { assert(flag == 0 && index == 10); later++; }
int NDAL_VideoExt_STBC_FreshWDT(void) { assert(0); return -1; }
int Hal_Video_Vpltio_SendCmd(int cmd, int a, void *buf, int b, int c, int d, int e, int f, int g)
{
    assert(cmd == 0x302 && *(int *)buf == 1 && !(a|b|c|d|e|f|g)); later++; return 0;
}
static void log_msg(void *p, int level, const char *fmt, ...) { (void)p; (void)level; (void)fmt; }
static const char *config(void *p, const char *k, const char *def) { (void)p; (void)k; return def; }
static int publish(void *p, const char *name, uint32_t version, const void *iface)
{
    (void)p; assert(!strcmp(name, OA_SVC_PLAT) && version == 1); service = iface; return 0;
}
static int unpublish(void *p, const char *name) { (void)p; assert(!strcmp(name, OA_SVC_PLAT)); service = NULL; return 0; }
int main(void)
{
    const oa_host_api api = { .size = sizeof api, .abi = OA_ABI_VERSION,
        .log = log_msg, .config_get = config, .publish = publish, .unpublish = unpublish };
    const oa_module *m = oa_module_entry();
    for (failed = 0; failed < 3; failed++) {
        void *c; calls = later = 0;
        assert(!m->init(&api, &c) && m->start(c) == -1);
        assert(calls == 3 && !later && !service); m->stop(c); m->destroy(c);
    }
    failed = -1; calls = later = 0; void *c;
    assert(!m->init(&api, &c) && !m->start(c));
    assert(calls == 3 && later == 8 && service && service->is_ready() == 1 && !service->stage_errors());
    m->stop(c); m->destroy(c); assert(!service);
    puts("  ok   each audio-prelude failure gates later hardware stages; success calls prelude once before platform stages");
    return 0;
}
