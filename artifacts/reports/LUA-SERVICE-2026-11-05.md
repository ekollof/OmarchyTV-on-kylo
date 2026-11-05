# Shared `ui.lua` scripting service — 2026-11-05

Status: implemented and verified on the host, sanitizer build and ARM/qemu.
Not television startup or scanout. No TV access; the owner's offline-only
decision remains in force.

## Design

`mod_lua.so` embeds Lua 5.4.9 and publishes one process-wide `ui.lua` v1
service. It is deliberately not a skin (`ui_lua` would select the `lua` skin
slot). Skins look the service up; they never receive a `lua_State`, display
handle or device fd. Rendering remains in the skin.

The API covers named layouts, navigation, theme colours, key bindings and
animation records. Hooks are `oa_paint`, `oa_key`, `oa_enter`, `oa_leave` and
`oa_migrate`. `ui_omarchy` uses the theme and first layout rect when a script
is loaded; `ui_vendor` runs paint/key hooks but keeps its vendor pixels.
`ui.omarchy.lua=1` makes the Omarchy skin require a loaded script, otherwise
the selector falls back to the vendor skin.

## Safety and lifecycle

- No `debug`, `package`, `coroutine`, `require`, `dofile`, `load`, binary
  chunks, shell, process stdio or device opens.
- `io.open` keeps regular files and the clock; writes are confined to
  `ui.lua.data`, reads reject non-regular files.
- `oa.fetch` is a bounded nonblocking curl child (http/https, 5 s, 64 KiB,
  two outstanding); suspend/stop/reload cancels it and stale generations are
  ignored.
- Load/reload compiles a candidate first; the running script and model stay on
  failure. Success migrates `oa.state` with `oa_migrate`.
- Memory (128–4096 KiB), instruction and script-size caps are enforced and
  reported through `status`.
- Timers are module-owned and deleted before state close/suspend; a held
  service refuses `reload lua` and uses `reload_script` instead.
- No telemetry, advertising, ACR, analytics or device ownership.

## Verification

```
full host make check: 433 ok
verify.cjs: 45 groups
check-ui-lua: 8 ok groups
check-ui-skin: 6 ok groups
lua IPC: load/start/reload keeps a good script and rejects a bad candidate
leak-enabled ASan/UBSan check-ui-lua: 8 ok groups
ARM/qemu tests/ui_lua: 8 ok groups
```

The sanitizer run used `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`. The ARM run uses the TV glibc rootfs; the
http success path is host-only because qemu-user cannot execute the host curl,
so ARM takes the failed-transfer callback. The socket test loads and starts
`lua`, reloads a valid and an invalid script over the real control socket, and
verifies the bad candidate leaves the service started.

Evidence: `artifacts/analysis/lua-service-2026-11-05/`.

## Limits

- No on-TV startup, scanout, input injection or hardware acceptance.
- The API is not the vendor Lua engine and does not claim vendor bytecode,
  module search paths or script parity.
- Instruction hooks do not bound blocking C calls such as a large regular-file
  `file:read("*a")`; the memory cap bounds the resulting string, not the read
  time.
- `oa.fetch` requires `/usr/bin/curl`; the success body is proven only on the
  host.
- Vendor pixels are unchanged and the native OmarchyTV screens remain pending.
