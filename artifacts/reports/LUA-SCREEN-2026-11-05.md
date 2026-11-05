# Lua skin flash on fb1 — 2026-11-05

Owner: full control of the set, temporary drawing allowed. Stock Aurora was
not stopped. Staging was `/buffer/oa-lua-screen` and was removed afterward.

## What ran

ABI 3 ARM `openaurora` with `mod_display_fb`, `mod_ui_engine`, `mod_lua`,
`mod_ui_omarchy` and `mod_ui_vendor` (fallback only). SHA256 of each file
matched the local stage before execution. Config: `ui.skin=omarchy`,
`ui.omarchy.lua=1`, script theme `20,180,40` with a magenta rail, 
`display.restore_snapshot=1`, `display.drm_init=0`, `display.test=0`.

Control replies while it was up:

```
display_fb started
ui_engine  started
lua        started
ui_omarchy started
requested omarchy active omarchy state ready fallback 0 error 0
```

`ui.lua` v1 was published. `ui.omarchy.lua=1` would have failed start if the
script had not loaded.

## What the panel showed

The owner saw one green flash. `before_0.png` and `during_0.png` are fb1
frame 0. The during capture, taken after the flash, is the stock
"Geen signaal" banner: 0 green samples, 13884 red samples. Aurora repaints
this layer, so one `present()` does not stay on screen. `after_0.png` is the
same stock banner after the process exited.

## Shutdown

`stop` of omarchy, lua and ui_engine returned `ok`. `stop display_fb` and
`unload display_fb` returned `failed` (`cleanup pending`). `quit` then exited
the process while that worker was still pending, so the in-memory snapshot
restore is not proven. The visible frame after exit is stock's no-signal
banner. No `openaurora` process remained. Aurora PIDs stayed 1967 and 1785.
`/buffer` free space returned to 191.9 MB.

## Limits

Not independent startup, not owned scanout, not a byte-for-byte framebuffer
restore. The physical remote was not used. Evidence images and `host.log`
are under `artifacts/analysis/lua-screen-2026-11-05/`.
