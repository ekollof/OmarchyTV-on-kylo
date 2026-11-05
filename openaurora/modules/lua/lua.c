/* mod_lua — one embedded Lua 5.4 state, shared by every skin as ui.lua.
 * This module is not a skin. core/skin.c would treat a module named ui_lua
 * as the skin "lua", so the module name stays "lua". */
#include "openaurora/lua_skin.h"
#include "openaurora/module.h"

struct oa_lua_runtime;
struct oa_lua_runtime *oa_lua_create(const oa_host_api *api);
void oa_lua_destroy(struct oa_lua_runtime *wrap);
int oa_lua_start(struct oa_lua_runtime *wrap);
void oa_lua_stop(struct oa_lua_runtime *wrap);
int oa_lua_suspend(struct oa_lua_runtime *wrap);
int oa_lua_resume(struct oa_lua_runtime *wrap);
size_t oa_lua_save(struct oa_lua_runtime *wrap, void *buf, size_t cap);
int oa_lua_restore(struct oa_lua_runtime *wrap, const void *buf, size_t len);
int oa_lua_quiesce(struct oa_lua_runtime *wrap);

static int m_init(const oa_host_api *api, void **out)
{
    struct oa_lua_runtime *rt;
    if (!api || api->size < sizeof *api || api->abi != OA_ABI_VERSION || !out)
        return -1;
    rt = oa_lua_create(api);
    if (!rt)
        return -1;
    *out = rt;
    return 0;
}

static int m_start(void *v) { return oa_lua_start(v); }
static void m_stop(void *v) { oa_lua_stop(v); }
static void m_destroy(void *v) { oa_lua_destroy(v); }
static int m_suspend(void *v) { return oa_lua_suspend(v); }
static int m_resume(void *v) { return oa_lua_resume(v); }
static size_t m_save(void *v, void *buf, size_t cap) { return oa_lua_save(v, buf, cap); }
static int m_restore(void *v, const void *buf, size_t len) { return oa_lua_restore(v, buf, len); }
static int m_quiesce(void *v) { return oa_lua_quiesce(v); }

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC,
    .abi = OA_ABI_VERSION,
    .size = sizeof(oa_module),
    .name = "lua",
    .version = "1",
    .requires = { "" },
    .init = m_init,
    .start = m_start,
    .stop = m_stop,
    .destroy = m_destroy,
    .suspend = m_suspend,
    .resume = m_resume,
    .save = m_save,
    .restore = m_restore,
    .quiesce = m_quiesce,
};

OA_MODULE_EXPORT(g_module)
