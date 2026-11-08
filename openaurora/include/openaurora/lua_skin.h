/* ui.lua v1 — one embedded Lua 5.4 state shared by every skin.
 *
 * Skins look the service up. They never receive a lua_State, a display, or a
 * device. Rendering stays in the skin: hooks may only change this module's
 * layout, navigation, theme, key-binding and animation records, plus oa.state.
 * Instruction counts do not bound a C function that blocks. io and os are
 * open for scripts that show stats or the clock: regular-file reads (including
 * /proc), os.date/time/clock/getenv, and writes only under ui.lua.data.
 * os.execute, os.exit, os.remove, os.rename, os.tmpname, io.popen and the
 * process stdin/stdout helpers are removed so a script cannot block the host
 * loop or seize the framebuffer, CEC or the amplifier. Weather and RSS use
 * oa.fetch (http/https only, size and time capped, nonblocking). No
 * debug/package/coroutine, no binary chunks, no telemetry or advertising.
 * oa.spawn runs a function as a host scheduler task. oa.yield pauses that
 * task until a later loop turn. The coroutine library stays closed, and a
 * task does not run while a hook holds the state.
 *
 * oa.aurora is an optional control-plane bridge. ui.lua.control=0 disables it
 * (default), 1 permits read-only control queries, and 2 permits deferred
 * commands through the same loop-thread dispatcher as the socket/CLI. It does
 * not expose pointers or grant device ownership; exit/quit/standby and
 * lifecycle commands targeting the lua module are refused. The bridge is a
 * suitable MCP tool surface, but no MCP server is started here.
 *
 * reload_script() validates a candidate and keeps the running state if the
 * candidate fails. Module reload (CLI `reload lua`) does the same check in
 * save(), so a bad file cancels before the host unloads the module. A consumer
 * that still holds ui.lua must use reload_script(); the host will not unload
 * a held service.
 *
 * Script timers are deleted by this module before the state is closed or
 * suspended. Their host owner is whichever skin called the hook, because the
 * host stamps cur_module. Deleting by id still drops them.
 */
#ifndef OPENAURORA_LUA_SKIN_H
#define OPENAURORA_LUA_SKIN_H

#include <stddef.h>
#include <stdint.h>

#define OA_SVC_UI_LUA "ui.lua"
#define OA_LUA_API_VERSION 2u
#define OA_LUA_NAME_MAX 32
#define OA_LUA_PANES 3
#define OA_LUA_PANE_LINES 8
#define OA_LUA_PANE_TITLE 24
#define OA_LUA_PANE_LABEL 16
#define OA_LUA_PANE_VALUE 64

typedef struct {
    char label[OA_LUA_PANE_LABEL];
    char value[OA_LUA_PANE_VALUE];
    int tone; /* 0 normal, 1 up/green, 2 down/red */
} oa_lua_pane_line;

typedef struct {
    char id[OA_LUA_NAME_MAX];
    char title[OA_LUA_PANE_TITLE];
    int line_count;
    oa_lua_pane_line lines[OA_LUA_PANE_LINES];
} oa_lua_pane;

typedef struct {
    char name[OA_LUA_NAME_MAX];
    int x, y, w, h;
} oa_lua_layout;

typedef struct {
    int loaded;     /* a script is installed */
    int error;      /* 0 or an errno from the last failed call */
    int timers;     /* script timers still armed */
    int has_theme;
    int has_nav;
    uint32_t generation;
    uint32_t api;   /* OA_LUA_API_VERSION while loaded, else 0 */
    char message[160];
} oa_lua_status;

typedef struct {
    int (*status)(oa_lua_status *out);
    /* NULL path re-reads ui.lua.script. 0 swaps. -1 keeps the previous script. */
    int (*reload_script)(const char *path);
    int (*layout_count)(void);
    int (*layout_at)(int index, oa_lua_layout *out);
    int (*nav)(int *band, int *index);
    /* bg and accent are four 0..255 channels, RGBA. */
    int (*theme)(int bg[4], int accent[4]);
    /* 0 copied, 1 no binding, -1 bad argument. */
    int (*binding)(int key, char *dst, size_t cap);
    /* easing 0 linear, 1 cubic. 0 found, 1 absent, -1 bad argument. */
    int (*anim)(const char *id, int *duration_ms, int *easing);
    /* hook is paint, key, enter, or leave. Missing hook returns 0 and
     * *handled 0. A Lua error returns -1 and leaves prior config in place. */
    int (*call)(const char *hook, const char *skin, int arg, int *handled);
    /* Widget panes published by oa.pane{...} during a hook. Bounded copies;
     * the skin renders them and never receives a lua_State. */
    int (*pane_count)(void);
    int (*pane_at)(int index, oa_lua_pane *out);
} oa_lua_iface;

#endif
