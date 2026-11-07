# Replacement system — 2026-11-07

This is the assembled, reproducible Aurora replacement UI in takeover mode. It
is not a persistent boot replacement; that remains a separate gate. The
composition is based on firmware assets and tests, not owner visual judgment.

## Composition

Built by:

```text
make TARGET=tv BUILD=build-tv-v3 CC=<arm-gcc> replacement-bundle
```

`build-tv-v3/replacement/`:

```text
openaurora          17367e39...
oa-client           f3d20a61...
oa-supervise        fcdad6c4...
fb-restore          afa1dbe1...
oa-ir-blast         36ef3665...
openaurora.conf
menus/sub_launcher.json
modules/mod_plat.so       fb11e812...  (tests/mod_plat_stub.so, module name "plat")
modules/mod_display.so
modules/mod_panel.so
modules/mod_ir.so
modules/mod_settings.so
modules/mod_service_db.so
modules/mod_image_decode.so
modules/mod_ui_engine.so
modules/mod_ui_vendor.so  d0afa127...
modules/mod_ticker.so
modules/mod_ui_clock.so
modules/mod_ui_anim.so
manifest.sha256
```

Autoload:

```text
plat,display,panel,ir,settings,service_db,image_decode,
ui_engine,ui_vendor,ticker,ui_clock,ui_anim
```

The real `plat` DRM/KMS bring-up changes the visible plane and leaves the
vendor-enabled fb1 OSD black (owner-observed on the 32-module stage). The
takeover composition therefore uses `plat_stub`, which publishes a ready
`plat` service without touching the vendor-initialized display. Hardware
services use the real-plat composition separately until cold OSD
allocation/enable is recovered (OSD-BUFFER-CONTRACT).

## Configuration

- `display.mirror=0` (stock stopped; only the active frame), `display.pan=0`,
  `display.set_region=1`, `display.restore_snapshot=1`, `display.profile=1`,
  `display.cold_init=1` (repair an unconfigured fb1 via the vendor SetPar path;
  no effect when the var is valid).
- `ui.fps=0`: no idle full-frame repaints; input paints immediately.
- `ui.dir=/buffer/openaurora/ui` (TV-local private assets), settings DB live
  read-only.
- `ir.blast=1`, BLAST helper `oa-ir-blast`.
- `panel.start=100`.

## Verified

- Stub stack: `display region -> 1920x1080 (ok)`; portal painted; physical
  remote pressed via BLAST; `ui-state`/portal log moved `band 0 rail 2 card 0`
  to `band 1 rail 2 card 2`, then `band 1 rail 2 card 1`.
- `ui-action open:settings` -> `portal screen 3`; captured picture-settings
  painted.
- `reload ui_vendor` completed and preserved the screen.
- Performance with `mirror=0`/`fps=0`: `present: scene=46..51ms mirror=0ms
  sync=0ms pan=6..12ms`, versus the old 2 fps/617 ms cadence.
- Rail highlight: the vendor APNG is 11 frames at 30 ms, 210x210; OAI1 frames
  `rail_hl01..11` are installed under `/buffer/openaurora/ui`, and
  `check-ui-rail-hl` asserts 11 frames at 30 ms in the vendor button window.
  The live 10 ms patch cadence measured ~50 ms per frame; exact 30 ms is still
  open.
- `ticker -> ui.clock -> ui.anim` starts, enabling the animation service.
- Settings transparency: firmware says the panel is a 780x1080 rect with the
  rest transparent over the video plane (UI-TRANSPARENCY/SETTINGS-PANEL-
  CAPTURE). `paint_settings` now clears RGBA(0,0,0,0), not opaque 13,14,14;
  `check-ui-settings-panel` passes. The previous live run used the old opaque
  clear, so the transparent live composition was not re-run on hardware.

## Run / restore

```sh
# preflight (no hardware access)
cd /buffer/oa-ui-20261107 && ./openaurora --check --modules ./modules

# takeover
/buffer/wdtctl disable
kill -TERM $(pidof aurora.elf)
./openaurora --modules ./modules --config ./openaurora.conf \
  --sock /tmp/openaurora.sock --log 3

# restore
./oa-client /tmp/openaurora.sock quit
rm -rf /buffer/oa-ui-20261107 /tmp/openaurora.sock
/buffer/wdtctl set 10
```

`oa-supervise` is the intended launcher: it disables the watchdog, monitors
heartbeat/ready, relaunches on `reload-suite` (exit 75) without re-arming the
watchdog, and returns to stock only on a real failure or explicit quit. It was
not exercised as the boot path.

## Full real-platform composition

`make TARGET=tv ... full-replacement-bundle` builds a 31-module candidate:

```text
real plat,display,panel,ir,audio,source,power,tuner,settings,service_db,
image_decode,epg,pvr,channels,i18n,audio_curve,audio_mixer,
audio_amp_profile,audio_amp,cec,ticker,ui_clock,ui_anim,ui_engine,
ui_vendor,ui_theme,ui_profile,ui_menu,ui_list,ui_navigation,lua
+ oa-client, oa-supervise, fb-restore, oa-ir-blast, oa-audio-broker, menus
```

`plat.drm_init=0` skips only `Hal_Video_DrmInit(1)` so the real platform can
run the audio prelude, GPIO/IIC, VIN slots and STBC video open while preserving
the vendor-initialized OSD. This is the candidate for a one-process full
system; it has **not** been run on the TV yet (no takeover authorized).

Config fixes for modules that failed in the 32-module stage run:

```text
audio_curve.aq=/conf/AQ.bin          audio_curve.profile=0
audio_mixer.broker=./oa-audio-broker
amp_profile.hwprofile=/conf/mb230_hwprofile.bin
ui_theme.index=/vendor2/dynamic_ui/telefunken18_svg.txt
ui_theme.binary=/vendor2/dynamic_ui/telefunken18_svg.bin
ui_profile.swprofile=/conf/mb230_swprofile.bin
ui_menu.file=./menus/sub_launcher.json
panel.channel=0 panel.freq=30000 panel.mode=0
panel.curve=1 panel.duty_max=4095 (stock backlight curve/scale)
```

Read-only verification on the TV (stock stayed running; no display/fb1, no
takeover): the full bundle's manifest and `--check` passed; a CPU-only core
with those six modules started all of them, including `audio_mixer` spawning
`oa-audio-broker`, and `ui_menu` loaded 1 dialog. `quit` left no core/broker.
`ui_omarchy` is deliberately excluded: `ui_vendor` already owns the single
`ui.engine` skin; it can be loaded after stopping `ui_vendor`.

Bundle hashes: openaurora `521047b8...`, `mod_plat.so` `446ee107...`,
`mod_ui_vendor.so` `d0afa127...`, `mod_panel.so` `454220b0...`,
`mod_display.so` `130405a4...`, config `844adf2d...`; tar
`b5dc4c13d9aa6c09019f6419d3329499f07e5946fcdecae2f90e7fe01e5d2167`.
The 2026-11-07 panel-scale fix and stock backlight curve are included; the
bundle was re-uploaded to `/buffer/oa-full-20261107`, its manifest verified and
`--check` passed. It has not been started.

`check-plat-drm-gate` is a fake-HAL regression: `plat.drm_init=0` logs the skip
and does not call the DRM bring-up, while `1` keeps it. Restoring the
unconditional call fails the test. `check-display` now covers the fbdev
first-pan order from a zero/hidden region state: the region window is written
before the first PanDisplay, with zero origin, then the frame commits. This is
the offline fbdev/PanDisplay cold candidate; it does not prove visible scanout.
`display.cold_init=1` additionally repairs a registered-but-unconfigured fb1
through `FBIOPUT_VSCREENINFO`, whose `gstOSDfbOps` entries call the vendor
`_ker_osd_fb_CheckVar`/`_ker_osd_fb_SetPar`; `check-display` covers zero-var
repair, region-before-pan and first buffer 0, and fails if the repair is
skipped. `full-replacement`/`plat.drm_init=0` still needs an authorized
takeover + cold boot; OSD-KERNEL-INIT says the existing fbdev/PanDisplay
first-pan/region path is the first cold candidate, not a blind private-GEM
backend.

## Authorized-window driver

`tools/boot/takeover-full.sh` is included in the full bundle for the next
authorized window:

```text
start         preflight, watchdog disable, stop stock, launch under oa-supervise
status        module table + recent log
reload-suite  ask oa-supervise to relaunch the core (no reboot)
restore       quit; supervisor re-arms the watchdog and execs stock (no reboot)
```

It is intentionally not run by the build or tests. The supervisor path means a
failed full-system start still returns to stock, and `restore` does not require
a reboot. The window is currently not authorized.

## Suite reload (no reboot)

- The core exposes `reload-suite` over the control socket. It returns `bye`,
  performs the normal clean stop (modules quiesce/unload, display snapshot
  restored) and exits with **75**.
- `oa-supervise` treats exit 75 as a **relaunch request**, not a failure. It
  keeps the watchdog disabled, restarts the same child argv, resets the
  heartbeat/ready receipts and waits for the new first present. Stock fallback
  and watchdog re-arm happen only on a real exit/failure (or after five
  consecutive reload exits before reaching the stable window).
- A stable suite resets the reload streak, so later reloads are allowed
  indefinitely.
- Tests: `check-reload-suite` asserts the core exits 75 after the socket
  reply; `tests/recovery.py` runs a fake child that exits 75, then a second
  child, and asserts the event order `wdt disable, child1, child2,
  wdt set 10, fallback` — no watchdog re-arm between runs.
- Per-module `reload <name>` still works without restarting the core. Use
  `reload-suite` when the core binary, config autoload or uncommitted module
  set changes.

Supervised launch (the supervisor remains the parent across reloads):

```sh
./oa-supervise --heartbeat /tmp/openaurora.alive --ready /tmp/openaurora.ready \
  --wdt /buffer/wdtctl --fallback /vendor/com.nvt.idtv/aurora.elf \
  -- ./openaurora --modules ./modules --config ./openaurora.conf \
     --sock /tmp/openaurora.sock --log 3

# later, without rebooting:
./oa-client /tmp/openaurora.sock reload-suite
```

The supervisor reload path is host-tested; it has not yet been exercised on
the TV. The core's display stop restores the stock snapshot, so the screen
briefly returns to stock while the new core starts and repaints.

## Limits / next

- Boot persistence, early startup before stock and boot hooks remain a separate
  owner gate; the system currently runs after a takeover.
- Real-plat plus visible OSD (one-process full hardware system) needs a
  takeover test of `plat.drm_init=0`; the full 31-module bundle preflights and
  its asset modules start CPU-only, but the unified display/hardware path was
  not run. Cold OSD allocation/binding/enable is still required for boot.
- Exact rail-highlight geometry/cadence, settings-list/full menu assets, source
  and channel UI, EPG/PVR UI and animation parity remain.
- Amp mute gate, RF tune/lock, CEC raw ownership, DVR and cold startup remain
  unaccepted; the hardware report records those negatives.
