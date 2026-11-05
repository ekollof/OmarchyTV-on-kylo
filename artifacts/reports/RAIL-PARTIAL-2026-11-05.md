# Firmware-shaped partial rail animation — 2026-11-05

The rail highlight is the vendor APNG `mainlauncher_item_highlight_animated`
(11 frames at 30 ms, one-shot, last frame held) drawn into the 160x160 button
window. The full portal repaint used for each frame measured a median **617 ms**
on the TV, so the 330 ms sequence stretched to about 6.8 s. This change makes
the animation update only the rail surface, as the vendor does, instead of
clearing and converting the whole 1920x1080 scene.

## API and implementation

- New optional `display.patch` v1 service with
  `fill_rgba(x,y,w,h,r,g,b,a)`: replacement (not source-over) straight RGBA.
  It restores the uniform dialog background behind the rail highlight.
- `display_fb` tracks a dirty scene rectangle from `fill`/`blit`/`blit_scaled`/
  `clear_rgba`/`patch_fill_rgba`, converts only that rectangle at present, and
  syncs page-rounded dirty ranges (mirroring only the dirty rows when enabled).
  Runtime frame rotation without mirroring forces a full render to avoid stale
  pixels outside the dirty rectangle.
- `scene_display` publishes the same patch service for the GPU path.
- `ui_vendor` extracts `paint_rail`; the 30 ms highlight tick uses
  `paint_rail_hl`, which patches the portal rail strip `0,211 209x670` with
  the background and redraws the five rail slots/highlight, then presents.
  It falls back to the full paint when `display.patch` is absent or the screen
  is not the portal.

The static full-paint composition is unchanged; the patch region is the rail
strip, which contains no cards or title. The APNG frame index/timing remains
the recovered one-shot 30 ms sequence.

## Verification

Focused tests:

```
ok display.patch replaces a widget rectangle and presents only the dirty range
ok rail highlight plays 11 frames at 30 ms in the vendor button window
ok actual fake-fbdev display_fb IPC: start/suspend/resume/reload/stop/reuse and full snapshot restoration
ok actual ui_vendor/display Unix IPC: start/use/stop/reload/unload lifecycle
```

`tests/ui_rail_hl.c` loads a test `display.patch` provider and asserts that the
first tick uses the `0,211 209x670` patch with **no full-screen clear**, then
continues through all 11 frames.

## On-TV measurement

Staged the ABI 3 CPU stack (`plat_stub`, `display_fb`, `ui_engine`, `settings`,
`service_db`, `image_decode`, `ir`, `ui_vendor`) with `display.mirror=0` and
`display.profile=1`; stock was stopped and fb1 was snapshotted. A socket
`key 22` drove the portal.

```
initial full frame: scene=56ms pan=7ms rect=0,0 1920x1080
highlight frames:   10 x (scene=3..5ms, pan=5..8ms) rect=0,211 209x670
highlight cadence:  ~50ms per frame over ~450ms
next full frame:    scene=52ms pan=11ms
```

That is the intended firmware-shaped update: the 160x160 widget animation no
longer converts the whole screen. The vendor 30 ms frame is still not met
exactly (~50 ms measured), and a key-driven full portal repaint remains ~63 ms
scene+pan.

Capture: `artifacts/analysis/rail-partial-2026-11-05/portal_0.png` (the
post-navigation portal with Netflix card focus). Log:
`artifacts/analysis/rail-partial-2026-11-05/run.log`.

## Restore

The TV stack was killed after the measurement, fb1 was restored from the
16,588,800-byte snapshot (`fb-restore restore`), staging removed and
`/buffer/wdtctl set 10` rebooted. After boot stock returned: `aurora.elf`
PID 1768, appmgr 1568, fb1 pan 0,0, no staging. The staged run used the pre-fix
IR binary (UART opened because `ir.blast=0` and `ir.device=`); the committed
IR default now treats an explicit empty `ir.device` as "no UART".

## Limits

- The partial path covers the portal rail strip only. Other screens and the
  title-slide animation still use full paints.
- The 30 ms vendor cadence is not exact; measured ~50 ms per highlight frame.
- No AE comparison of the final partial frame against a fresh full paint; the
  static assets and full-paint path are unchanged and the host patch test
  verifies replacement/dirty-range behavior.
- No GPU-path partial upload yet; `scene_display` accepts the patch but still
  uploads the canvas as before.
