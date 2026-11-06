# mod_ui_omarchy async quiesce — 2026-11-06

Offline host control-socket and focused ASan/UBSan tests with the real
`mod_ui_omarchy` (engine + Lua + recorder display), followed by a staged
CPU-only ARM run on the TV. No fb, input, audio, tuner or hardware module was
loaded on the TV.

## Ownership and gate

`ui_omarchy` publishes no service. It registers its `omarchy` skin with
`ui.engine`, holds `ui.engine` v2 and `ui.lua` v1 references, subscribes to
SERVICE_REMOVED/MODULE_CHANGED, may arm a rebind timer, and caches the
script-derived background/accent colors, layout rectangle and last painted
screen size.

Quiesce now:

1. fails fast with `-EAGAIN` while a cleanup is pending;
2. clears `active` first, so queued engine dispatches, paint/key/enter/leave
   and the rebind callback become inert;
3. removes the rebind timer and both subscriptions synchronously;
4. unregisters the skin and releases the engine/Lua references synchronously
   (`unregister_skin` clears the engine's active skin before it calls leave);
5. snapshots the cached skin state and detaches the heap object;
6. frees it synchronously at delay 0, else from one nonblocking `sched` slice
   after `ui_omarchy.cleanup_delay_ms`.

`save()` falls back to the snapshot after detach; `restore()` validates the
72-byte magic/versioned blob and applies it to the replacement. `m_start`
refuses while active or pending, reallocates state after stop/unload and
reapplies the saved values before registering and painting. `stop`/`destroy`
cancel the task and free leftovers.

A diagnostic `omarchy-state` control probe decodes the same blob:

```text
omarchy colors <0|1> bg r g b a accent r g b layout <0|1> x y w h screen <0|1> w h
```

It is project diagnostic access, not a service contract.

## Verification

`check-ui-omarchy-quiesce` stages `nav_display` (recorder display), `lua`,
`ui_engine` and `ui_omarchy`, with `ui.skin=omarchy`, a script setting
`oa.theme(9,8,7,255, 6,5,4,255)` and `oa.layout("rail",0,0,40,100)`, and
`ui_omarchy.cleanup_delay_ms=400`:

```text
after start: skin -> active omarchy
omarchy-state -> colors 1 bg 9 8 7 255 accent 6 5 4
                 layout 1 0 0 40 100 screen 1 1920 1080
reload ui_omarchy -> failed             (fast; 0-1 ms in the final runs)
status ui_omarchy -> responsive (< 1 s), cleanup_pending 1
skin -> active -                        (old skin unregistered synchronously)
omarchy-state -> same snapshot          (readable after detach)
wait 600 ms
reload ui_omarchy -> ok
ui_omarchy: restored 72 bytes of state
skin -> active omarchy; omarchy-state -> same
stop ui_omarchy -> failed; skin -> active -
wait 600 ms; stop -> ok; unload -> ok; quit -> bye
```

```text
ok ui_omarchy quiesce: reload fails fast (0ms), skin unregistered, state handed off, retry re-registers
```

A focused ASan+UBSan build of the core, engine, Lua and omarchy modules
(`-fsanitize=address,undefined`, `detect_leaks=1`, `halt_on_error=1`) ran the
same test clean.

Existing skin/Lua suites still pass: `check-ui-skin`, `check-ui-lua` and
`check-ui-skin-socket` (socket startup fallback, runtime switching, enter
failure, queued-input reload, suspend/resume and engine replacement).

The ARM modules link only base glibc:

```text
mod_ui_omarchy.so   libc.so.6
mod_ui_engine.so    libpthread.so.0 libc.so.6
mod_lua.so          libm.so.6 libc.so.6
ui_nav_display.so   libm.so.6 libc.so.6
```

Full suite ran visibly and ended `all tests passed` (318 `ok` lines, rc 0).
QEMU oracle checks still pass:

```text
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03; key 20 timer card bottom 405 -> 450
ok qemu sub-launcher: static AE=0; 300 ms card2 701,276 235x130 -> 701,276 317x175
```

## On-TV ARM CPU-only gate — 2026-11-06

Staging was under `/buffer/oa-omarchy-verify` (writable 232 MB ubi volume), not
`/tmp`; the TV was reached through `tools/tvssh.sh`. Upload and execution were
separate connections.

Hash-verified ARM inputs (remote hashes match the local build):

```text
openaurora            sha256 43c7c7be1e0f9a67c07d208ead14f044b8d1a56f5f03746c4571115b7c8da83b
mod_ui_omarchy.so     sha256 5f781c1d4ee57befbd560018aa90fb9099b3af71dd956f6f89efd89f8e96f1b0
mod_ui_engine.so      sha256 ad970b03fca532f0e471364b7a291ba4c46ce4063ee90e633af93094080d4249
mod_lua.so            sha256 d7466300c36c9d6812ef3dac8c6c63b73f055f1f6ceed65bbd85d14c1fc8e5f1
ui_nav_display.so     sha256 36c845ff2275262c51c4aa5faf5590b28c81acaf29be600962fa5f9664a0aee6
```

The staged recorder-display stack was loaded and started on the TV with the
same script and 400 ms delay:

```text
omarchy colors 1 bg 9 8 7 255 accent 6 5 4 layout 1 0 0 40 100 screen 1 1920 1080
skin -> requested - active omarchy state idle fallback 0 error 0
reload ui_omarchy -> failed
skin -> requested - active - state idle fallback 0 error 0
omarchy-state -> same snapshot
status -> cleanup_pending 1, path /buffer/oa-omarchy-verify/...
wait 0.9 s
reload ui_omarchy -> ok; ui_omarchy: restored 72 bytes of state
skin -> active omarchy; omarchy-state -> same
stop ui_omarchy -> failed; skin -> active -
wait 0.9 s
stop ui_omarchy -> ok; unload ui_omarchy -> ok; quit -> bye
```

Afterward `/buffer/oa-omarchy-verify` was removed, no `openaurora` helper
remained, and stock Aurora was still PID 1781. Only the staged DSOs, a Unix
control socket and stdin were opened; no `/dev/fb*`, `/dev/ntk*`, `/dev/dvb`,
`/dev/snd`, `/dev/i2c*`, fb1, scanout, input or tuner/HAL path was opened. This
is a CPU-only ARM/glibc gate check with a recorder display module, not
display/HAL, scanout, real key input or broad UI acceptance.

## Limits

- `ui_omarchy` is still the skeleton skin over a recorder display; the state
  blob covers cached colors/layout/screen only, not real OmarchyTV screens.
- The test drives a script theme; it does not compare rendering against the
  vendor or exercise real input.
- `omarchy-state` is project diagnostic access, not a public service.
- The focused ASan/UBSan run covers this module and test only; it is not a full
  sanitizer suite. The on-TV run is CPU-only.
- Remaining synchronous modules (2): `plat`, `power`.
- Cold startup, scanout, real audio/CEC/tuner/PVR/VIN hardware, real VIN
  switching, the vendor `UserPreferences`/PQ object and broad screen/
  interaction parity remain unaccepted.
