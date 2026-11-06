# Lua scripting (`ui.lua`)

`mod_lua.so` embeds Lua 5.4.9 and publishes one service, `ui.lua` version 1.
Any skin looks that service up. The module name is `lua`, not `ui_lua`: the
skin selector loads a skin named `NAME` as the module `ui_NAME`, so a module
named `ui_lua` would be treated as a skin called `lua`. This module does not
register a skin and does not draw.

There is one Lua state per process. A second `lua` module in the same process
fails its init. Skins never receive a `lua_State`, a display handle, or a
device fd. Rendering stays in the skin. The script's job is configuration,
small amounts of text, and requests that C then runs without blocking the
host loop any longer than a bounded file read.

This is not the OmarchyTV product UI. It is the scripting surface that skin
can use while that UI is still the skeleton in `ui_omarchy`.

## Turn one on

| key | default | meaning |
|---|---|---|
| `ui.lua.script` | empty | Path of the script. Empty means the service is up and `loaded` is 0. The file must be a regular file, non-empty, and no larger than `ui.lua.max_bytes`. |
| `ui.lua.memory_kb` | 1024 | Lua allocator cap, clamped to 128..4096. Includes the state itself, so a few hundred kilobytes are already used before the script runs. |
| `ui.lua.instructions` | 100000 | Instruction budget for one load or one callback, clamped to 1000..1000000. The hook counts in batches of at most 1000. |
| `ui.lua.max_bytes` | 32768 | Maximum script file size, clamped to 256..49152. The saved blob must stay under the host's 64 KiB reload cap. |
| `ui.lua.data` | empty | Directory for `io.open` writes. Empty means `io.open` is read-only. |
| `ui.lua.control` | 0 | Internal control bridge: 0 off, 1 read-only queries, 2 deferred socket/CLI commands. See `oa.aurora` below. |
| `ui.omarchy.lua` | 0 | When `1`, `ui_omarchy` start fails unless `ui.lua` is up and a script is loaded. The skin selector then falls back to the vendor skin. |

Load and start `lua` before the skin if the skin should see the service at
start. A skin that is already running looks the service up again when its
own provider rebind runs. `ui.omarchy.lua` defaults off, so existing Omarchy
and vendor tests keep their current drawing when `lua` is not loaded.

```
load lua
start lua
```

`start` publishes `ui.lua`. `stop` drops timers and fetches, closes the
state, and unpublishes. `suspend` keeps the state and the service, cancels
timers and in-flight fetches, and makes hook calls a no-op until `resume`.

## What a skin calls

The C service is `oa_lua_iface` (`include/openaurora/lua_skin.h`).

| call | result |
|---|---|
| `status` | `loaded`, last errno, armed script-timer count, theme/nav flags, generation, api version, and the last error text. |
| `reload_script(path)` | `NULL` re-reads `ui.lua.script`. Compiles a candidate. On failure the running script stays. On success the new script runs, then the previous model and `oa.state` are applied, then `oa_migrate`. |
| `layout_count` / `layout_at` | Up to 16 named rectangles. |
| `nav` | Band and index, or an error if the script never set them. |
| `theme` | Eight channels, background RGBA then accent RGBA, each 0..255. |
| `binding` | `0` copied, `1` none, `-1` bad argument. Action strings are shorter than 32 bytes. |
| `anim` | `0` found, `1` absent, `-1` bad argument. Easing is `0` linear or `1` cubic. |
| `call(hook, skin, arg, handled)` | `hook` is `paint`, `key`, `enter`, or `leave`. A missing hook returns 0 and `handled` 0. A Lua error returns -1 and restores the model from before the call. |

Skin names passed into hooks are `[A-Za-z0-9_]`, shorter than 32 characters.
Re-entering `call` from inside a hook returns `EBUSY`. A hook that returns
anything other than nil, a boolean, or an integer is an error and the model
is restored.

`ui_omarchy` calls `paint` / `key` / `enter` / `leave` with the skin name
`omarchy`. After a successful `paint` hook it draws its clear colour from
the script background and its rail from the accent. Layout slot 0 replaces
the rail rectangle. If the hook fails, the last accepted colours stay.
Tiles stay the skeleton grey. With no service, the skeleton is still
background `(12,14,18)` and accent `(0,122,204)`.

`ui_vendor` calls `paint` and `key` with the skin name `vendor`. It does not
read theme or layout. Vendor pixels stay on the vendor path. A key hook that
returns 1 consumes the key; otherwise the vendor screen handles it. A hook
error does not stop the vendor frame.

While a skin holds `ui.lua`, `reload lua` is refused. Use `reload_script`.
After the skin stops, `reload lua` runs the host save/restore path: the
candidate is compiled first, and a bad file returns a size the host treats
as "do not unload". A good file hands the model and `oa.state` to the new
instance, which then runs `oa_migrate`.

## Script API (`oa`)

The host owns the global table `oa`. Replacing it fails the load
(`oa table replaced`). `oa.api` is `1`. An optional global `oa_api()` must
return `1` or the load fails.

Scripts are source text only (`luaL_loadbufferx` mode `"t"`). Binary chunks
are rejected.

| function | limits |
|---|---|
| `oa.layout(name, x, y, w, h)` | 16 entries. `name` length 1..31. `x` and `y` are -8192..8192. `w` and `h` are 1..8192. The same name replaces its row. |
| `oa.nav(band, index)` | `band` 0..15, `index` 0..255. |
| `oa.theme(br, bg, bb, ba, ar, ag, ab, aa)` | Eight integers, each 0..255. |
| `oa.bind(key, action)` | 16 entries. `key` 0..255. `action` length 1..31. |
| `oa.anim(id, duration_ms, easing)` | 8 entries. `id` length 1..31. `duration_ms` 1..600000. `easing` is `"linear"` or `"cubic"`. |
| `oa.nav_band()` / `oa.nav_index()` | Current value, or `-1` if unset. |
| `oa.bound(key)` | Action string, or nil. |
| `oa.anim_ms(id)` | Duration, or nil. |
| `oa.after(ms, fn)` | One-shot. `ms` is 0..60000. At most 4 armed at once. Not available while a candidate is being validated. The host timer is owned by the skin that invoked the hook, because the host stamps the caller. This module also deletes the timer by id before the state is closed or suspended, so a stale callback cannot run. |
| `oa.fetch(url, fn)` | See below. At most 2 queued or in flight per state. |
| `oa.state` | A table the host persists. Keys are strings of length 1..32. Values are booleans, integers, or strings of length 0..64. At most 32 keys. Any other value makes save, migrate, or reload fail. |

Hooks, all optional:

```lua
function oa_paint(skin, arg) end   -- arg is 0 today
function oa_key(skin, key) end     -- return 1 to consume the key
function oa_enter(skin, arg) end
function oa_leave(skin, arg) end
function oa_migrate(prev)          -- prev is the saved oa.state; return a table or nil
  return prev
end
```

Top-level code runs at load, before the saved model is copied back by a
reload. Put behaviour that must change every frame in `oa_paint`. A reload
keeps the previous layout, theme, nav, bindings and animation records, then
calls `oa_migrate`.

A failed hook restores the model snapshot taken before the call. A partial
`oa.theme` that then `error()`s does not stick. `status.error` is the errno
from the last failure: `EAGAIN` when the instruction budget trips, `ENOMEM`
when the Lua allocator refuses, `EINVAL` for syntax and `oa_` contract
errors, otherwise `EIO`. The message is also logged at error level.

The instruction hook does not bound a C function that blocks. The C
functions exposed here copy bounded values or queue work. `file:read` of a
large regular file still runs inside the hook and can stall the host loop
until the read returns or the Lua memory cap refuses the resulting string.
Do not read large files from a paint or key hook. `/proc` stats are small
enough for this.

## `oa.aurora` internal control bridge

`ui.lua.control` is **0** (off) by default. It gates an optional bridge to the
same loop-thread control dispatcher used by the Unix socket and CLI:

| value | capability |
|---|---|
| `0` | disabled; `oa.aurora.query`/`request` raise `aurora control is disabled` |
| `1` | read-only queries only |
| `2` | read-only queries plus deferred control commands |

The table is available as both `oa.aurora` and the global `aurora`.

| call | result |
|---|---|
| `aurora.query(command)` | synchronous read-only command; returns `reply, ok` |
| `aurora.request(command)` | queues one command for after the current hook; returns an integer request id |
| `aurora.result()` | returns and clears `{ id, ok, reply }`, or nil |
| `aurora.mode()` | current `ui.lua.control` value |

Read-only queries are an explicit allowlist (`list`, `status`, `service-list`,
`channel-list`, `channel-state`, `epg-count`, `epg-query`, `tuner-state`,
`pvr-list`, `ui-state`, `nav-state`, `list-state`, `list-key`,
`mixer-query`, `mixer-result`, `hotel-state`, `launcher-items`). They run only
in the live state, never while a candidate is being validated.

`request` is deferred through the `lua` module's host post queue (four slots),
so a load/start/stop/reload cannot re-enter the skin hook that called it.
Commands are bounded to 191 printable ASCII bytes. `exit`, `quit`, `standby`,
lifecycle commands targeting `lua` itself, and `load`/`reload` paths (only
module names are allowed) are refused. The target module's own
start/stop/permission checks still apply; this does not grant device ownership
or bypass the skin/service hold rules. A command that stops the `lua` module is
not permitted, so the result slot cannot outlive its runtime.

An MCP server can expose `aurora.query` and `aurora.request` as tools, or use
the existing Unix control socket directly. This module does not start a
JSON-RPC listener, and `os.execute`/`io.popen` remain removed: the bridge is
the controlled automation path, not a shell.

## `io` and `os`

`io` and `os` are opened so a skin can show a clock or a `/proc` stat and
can write a small file under a directory the config names. They are not a
shell and they are not device ownership.

Kept:

| name | use |
|---|---|
| `io.open` | See the wrapper below. File methods on the handle (`read`, `write`, `lines`, `close`, `seek`, `flush`) stay. |
| `io.type` | Type check of a file handle. |
| `os.date`, `os.time`, `os.clock`, `os.difftime` | Clock and CPU time. |
| `os.getenv` | Environment of the host process. |
| `os.setlocale` | Process-wide C locale. A script that calls it changes the process, not a private locale. |

Removed, each set to nil:

| name | why |
|---|---|
| `os.execute`, `io.popen` | A shell or a pipe would block the single host thread and could open the framebuffer, CEC, the amplifier, or anything else the process can open. |
| `os.exit` | Must not terminate the UI process. |
| `os.remove`, `os.rename`, `os.tmpname` | No unconfined filesystem changes or temp-name races. |
| `io.tmpfile`, `io.stdin`, `io.stdout`, `io.stderr` | No process stdio. The control socket and logs stay with the host. |
| `io.input`, `io.output`, `io.read`, `io.write`, `io.lines`, `io.flush`, `io.close` | Those operate on the default process files. Use the handle returned by `io.open`. |
| `dofile`, `loadfile`, `load`, `require` | No second source of code. The only loaded chunk is the configured script, as text. |
| `debug`, `package`, `coroutine` | No debugger, no module search path, no yields the host loop does not schedule. |

`io.open(path, mode)`:

- A mode containing `w`, `a`, or `+` is a write. It succeeds only when `path`
  is `ui.lua.data .. "/" .. relative` and the path does not contain `..`.
  An empty `ui.lua.data` makes every write return `nil, "read only"`.
- Any other mode is a read. The file is opened and `fstat`ed. Only a regular
  file is accepted, which is what `/proc/meminfo` and similar stat files are
  on Linux. `/dev/null`, directories, and other device nodes return
  `nil, "not a regular file"`. A missing path is reported by Lua's own
  `io.open`.
- The check and the open are not atomic. A path that changes from a regular
  file to a device between them is still not something the script can hold
  across the call: the returned handle is Lua's file object for the path it
  opened, and device opens are rejected when the check sees them.

Nothing here sends telemetry, advertising identifiers, or ACR. A script that
wants weather or an RSS document uses `oa.fetch` and shows the result
locally. Do not add a module that reports viewing data.

## `oa.fetch(url, function(body, err) end)`

Stock `io` does not speak HTTP. `oa.fetch` is the only network path.

- The URL must be `http://` or `https://`, shorter than 480 bytes, with no
  spaces, controls, or non-ASCII. `file://` and every other scheme raise a
  Lua error immediately, so a paint hook that calls it fails and the model
  rolls back.
- The transfer is `execv` of `/usr/bin/curl` with no shell. Protocols and
  redirects are limited to http and https. `--max-time 5` and
  `--max-filesize 65536`. The body kept for the callback is at most 64 KiB.
- The child is non-blocking. A timer owned by the `lua` module (not by the
  skin) pumps it, and a host watch reads the pipe. The host loop is not
  joined to curl. If curl is missing, the callback runs with
  `body == nil` and `err == "curl missing"`.
- The callback is `function(body, err)`. Success is a string and a nil error.
  Failure is nil and a short error string. It runs on the host loop, under
  the same instruction budget. A callback error restores the model.
- At most two fetches are queued or running. Suspend, stop, script swap, and
  a rejected candidate kill the child and drop the callback. A callback whose
  generation no longer matches does not touch a closed state.
- Quiesce is non-blocking. It unpublishes `ui.lua` and drops the pump
  first. A live child is signalled and, if it has not been reaped, quiesce
  returns busy so the module is not `dlclose`d. The host retries. The first
  pass sends `SIGTERM`; a later pass sends `SIGKILL`. While that is pending,
  `reload_script` fails and a paint/key/enter/leave hook is not called
  (the hook reports not handled). Read-only status still answers.

```lua
function oa_paint(skin)
  if oa.state.weather ~= nil then return end
  oa.state.weather = ""
  oa.fetch("https://example.invalid/weather", function(body, err)
    if body then oa.state.weather = body:sub(1, 64) end
  end)
end
```

`oa.state` strings are at most 64 bytes. Keep a short summary there. The
fetch body itself may be up to 64 KiB for the duration of the callback, and
it counts against the Lua memory cap when it is pushed.

## Reload, suspend, unload

1. Edit the script file (or pass another path to `reload_script`).
2. The candidate is compiled in a second state with the same memory and
   instruction caps. `oa.after` is rejected there. `oa.fetch` may queue on
   that candidate and is cancelled if the candidate is thrown away; it is
   started only after that state becomes the live one.
3. Syntax errors, memory exhaustion (`string.rep` of many megabytes), and
   instruction loops fail the candidate. The previous script, model, and
   timers stay.
4. On success, timers and fetches of the old state are cancelled, the saved
   model and `oa.state` are applied, and `oa_migrate(prev)` runs. Return a
   table to replace `oa.state`, or nil to keep it. The table must still meet
   the `oa.state` rules after the call.

`suspend` / `resume` of the `lua` module keep the state. Timers are zero
across suspend. Resume does not replay them; the script arms them again from
a later hook. In-flight fetches are cancelled, not paused. Resume stays
suspended when the 50 ms fetch pump cannot be allocated, and start fails
the same way. Queued `oa.fetch` work is started by that pump, so the service
is not left published without it.

`stop` / `unload` leave no script timers and no watches. Unload is refused
while a skin still holds the service, unless the operator forces it. Do not
force-unload `lua` out from under a skin: the skin drops its pointer when it
sees `service/removed`, but a forced removal is still a stale-pointer window
the hold is there to prevent.

## What this does not do

- It does not draw. Omarchy uses the records; vendor ignores them for pixels.
- It does not open framebuffer, CEC, audio, GPIO, I2C, or the vendor HAL.
- It does not start a worker thread. Curl is a short-lived child. Quiesce
  will not return success while that child is unreaped.
- It does not bound every C call. A hostile or accidental `file:read("*a")`
  of a huge regular file can block the loop. The memory cap limits the
  string that results, not the time spent in `read`.
- It does not provide `debug`, coroutines, `require`, or binary chunks.
- It does not provide shell, process stdio, or device files.
- It does not implement telemetry, advertising, ACR, analytics, tracking,
  profiling, or ad delivery. Do not script those.
- It is not on-set acceptance. Host, sanitizer, and ARM/qemu runs of
  `tests/ui_lua.c` do not show scanout on the television.
