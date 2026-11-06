# ui_vendor async quiesce — 2026-11-06

TV was off limits. Offline host control-socket test with the real ARM-style
module build, real font pack, real langprofile and the shipped sub-launcher
JSON. No display paint, TV, fbdev or stock process was touched.

## State held by ui_vendor

`ui_vendor` is the highest-state skin module: repaint/title/sub-launcher/rail
timers, `ui.engine` skin registration, held `ui.engine`/`ui.anim`/`ui.clock`/
`ui.theme`/`i18n`/`settings`/`picture.settings`/`ui.lua` services, the portal
service, module/service subscriptions, and the large font pack + settings/
sub-launcher text caches.

## Gate

`m_quiesce` does the logical teardown synchronously:

1. cancel the dialog, every timer and the slide sampler;
2. unsubscribe module/service events;
3. unbind the animation callback, release every held provider, unpublish
   `ui.portal`, unregister the engine skin and release `ui.engine`;
4. detach the font pack and text/font/scratch buffers into a separate cleanup
   object;
5. with `ui_vendor.cleanup_delay_ms=0` free them synchronously; with a
   diagnostic delay, hand the object to a worker and return `-EAGAIN`.

The worker calls only `free`/`oa_font_pack_free` and never touches the host,
ctx or paint path. `m_destroy` joins a leftover worker.

`heavy_detach` moves the font pack, per-row/title text canvases, sub-launcher
blobs, font bytes and slide scratch out of the ctx. A second quiesce (the host
calls the gate again after a successful stop during unload/reload) detects an
empty cleanup object and returns 0 without starting another delay. This
idempotence is required by `oa_module_reload`/`oa_module_unload`.

## Reload bug fixed

`m_restore` rejected saved screen values above `SCREEN_CHANNELS` (6), but the
sub-launcher screen is 7. Reloading while on `ui.screen=sub_launcher` therefore
failed with “restore() failed or absent (-1)”. It now accepts
`SCREEN_SUBLAUNCHER` (7).

## Verification

```text
ok ui_vendor quiesce: reload fails fast, portal republishes, retry completes
```

The control test loads `nav_display`, `ui_engine`, `fake_clock`, `ui_anim`,
`ui_menu`, `i18n` and `ui_vendor` with the real font pack/langprofile/menu and
`ui_vendor.cleanup_delay_ms=400`:

```text
reload ui_vendor -> failed (<1.5 s)
status ui_vendor -> responsive (<1 s)
retry reload -> ok
ui-state -> portal screen 7 (portal republished)
stop ui_vendor -> failed; retry -> ok; unload -> ok
```

The font-load log appears, so the heavy font/text path was exercised before
the detached free.

Full suite and QEMU checks:

```text
full_check=0
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- Only `ir`, `i18n`, `settings` and now `ui_vendor` have the asynchronous gate;
  other modules still stop synchronously.
- The diagnostic delay exercises the gate; the default is a synchronous free.
- The host control test uses no display paint; it does not prove a live
  scanout/GPU teardown or stock-Aurora coexistence.
- No TV reload or frame-capture parity was performed.
