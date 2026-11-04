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

## Independent startup candidate — blocked on the watchdog/recovery decision

An independent run without vendor Aurora is staged by the same bundle but
requires:

1. stopping the vendor `aurora.elf` owner;
2. disabling the MCU watchdog with `/buffer/wdtctl` (or letting
   `oa-supervise` do it) because `mod_plat` measured that
   `NDAL_VideoExt_STBC_FreshWDT()` refuses a kick from a plain process, so
   openaurora cannot maintain the watchdog;
3. being ready to recover the set if the child fails (vendor fallback /
   power cycle / USB recovery).

The standing project constraints mark the `wdtctl disable` takeover as
currently unauthorized, and a failed takeover can leave the UI lost until
recovery. This step needs the owner’s explicit go-ahead and physical recovery
readiness; it was **not** executed. The no-`plat` binding and candidate
bundle are ready for that run.

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
