# Supervised display binding and live apps-grid capture — 2026-11-04

`modules/display/display.c` gained a no-`plat` build (`-DOA_DISPLAY_NO_PLAT`,
`mod_display_fb.so`) that binds to the **vendor-initialized fb1** without
platform bring-up. A candidate `openaurora` host with
`settings,display_fb,ui_engine,ui_vendor` painted the live apps grid on the
real TV and the framebuffer capture shows it. Vendor Aurora was still running
and was left as the owner; the candidate was stopped and all staging removed.

## What was run

- Candidate bundle: current ARM `openaurora` host, the no-`plat` display
  binding (`mod_display_fb.so`, 90460 bytes), settings/ui_engine/ui_vendor
  modules, the apps UI assets and this config:

```
autoload = settings,display_fb,ui_engine,ui_vendor
settings.db = /conf/sqlite_db/settingsDB.db
settings.readonly = 1
display.fb = /dev/fb1
ui.dir = /buffer/oa_candidate/ui
ui.screen = apps
ui.grid_sel = 0
ui.fps = 2
ui.ok_file = /tmp/oa_candidate.ready
```

- The host logged `painted apps grid: 18 items, first 0, scrollbar 0/600`
  repeatedly from the **live** `/conf/sqlite_db/settingsDB.db` (see
  `candidate-run.log`).
- `candidate-apps_1.png` is the 1920x1080 fb1 capture: the nine known card
  images in the apps grid plus the synthetic placeholders for the other
  apps, with the #811924 track and white 600 px scrollbar at
  `1750,180,8x720`.
- Stopping the candidate left the vendor `aurora.elf` (PIDs 1967/1785)
  running; `after-candidate-stock2_1.png` is the stock “Geen signaal” screen.
  `/buffer/oa_candidate`, `/tmp/oa_candidate.*` and all helper files were
  removed; no candidate/scanout helper remains.

This is a **supervised display/scanout binding** to an existing vendor-
initialized fb1. It is not a cold independent startup: the display plane,
fb1 mapping and kernel state were initialized by the vendor boot/Aurora, and
the vendor process remained the owner throughout.

## Independent startup — unverified, not authorized by this test

**Correction — WORK-REVIEW:** the historical FreshWDT return1 does not
establish kick rejection or prove that native watchdog maintenance is impossible.
The earlier inference from that scalar, and the claim that this bundle was
ready for independent takeover, are withdrawn. The MCU authority/lifecycle and
actual return contract need firmware investigation; do not repeat the cold
probe, which previously reset the set.

No vendor stop/watchdog disable was executed. Those operations remain outside
standing authorization. Any later independent-startup test needs an evidenced
ownership/watchdog/fallback plan and the authorization specified in AGENTS.md.
This shared-display run supplies neither cold-startup proof nor that plan.
Ordinary module stop does not restore the whole framebuffer/region/MAU/pan:
the stock screenshot recorded return to stock output, not full state equality.
A future display test needs an explicit restoration owner.

The reviewed no-plat binding now rejects display.drm_init=1 before open, cannot
run global mdev discovery and has no vendor-HAL dynamic dependencies. Its
metadata name is display_fb. This hardening was checked offline plus a real-TV
loader-only dependency listing; the revised binding has not drawn on the TV.

## Limits

- Real downloaded-PNG decoding is still not implemented: the apps grid uses
  the OAI1 assets and synthetic placeholders; the live DB rows/scrollbar are
  live, but the missing app images are not decoded from
  `/buffer/downloaded_images`.
- The visible capture is a reconstruction of the apps chrome (no stock
  apps-grid capture exists); the scrollbar colors/cell fitting are recovered
  geometry, not pixel parity.
- The vendor was not stopped; no watchdog, boot-hook or persistent change was
  made.
