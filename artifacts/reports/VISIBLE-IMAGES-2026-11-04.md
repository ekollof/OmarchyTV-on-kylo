# Visible on-TV PNG/JPEG apps grid — 2026-11-04

A supervised `openaurora` candidate bound to the vendor-initialized fb1 and
painted the live apps grid from the real `/buffer/downloaded_images` assets.
The capture shows 15 real cards, including two of the four downloaded JPEGs
(`foxxum_thumb_3753` → DAZN and `foxxum_thumb_1685` → DW). Vendor Aurora
remained the owner. The candidate was stopped, the pre-test fb1 pixels/offset
were restored, and the staging bundle was removed.

**Owner access note:** this run happened under the immediately preceding
"TV access allowed" instruction. A later AGENTS.md override revoked TV access
("Don't touch the TV") before this report was written. No TV command was run
after that override; the cleanup had already completed. Independent startup
and any further hardware verification are therefore blocked pending explicit
re-authorization.

## Run

Candidate bundle `/buffer/oa_visible_20261104`:

- ARM ABI3 host `openaurora` `56496e6b99bcc296bcd141f6d9c76ade`
- `mod_image_decode.so` `d2a4d9e19aeea04ea3242634d35b6870`
- `mod_ui_vendor.so` `d74caf55811da631ddb2c7108e0d356e`
- `mod_display_fb.so`, `mod_settings.so`, `mod_ui_engine.so`
- `fb-restore` `2e7f745cf09043924fea7140ce31cf40`
- Config: live read-only `/conf/sqlite_db/settingsDB.db`, `ui.screen=apps`,
  `ui.dir` empty, `ui.download_dir=/buffer/downloaded_images`,
  `display.fb=/dev/fb1`, `display.set_region=0`, `display.pan=0`.

Log (`candidate-run.log`): modules `settings`, `image_decode v2`,
`display_fb v0.1`, `ui_engine`, `ui_vendor v0.2` started; fb1
`1920x1080 32bpp, 2 frame(s), back=1`; the grid repainted 51 times with
`painted apps grid: 18 items, first 0, scrollbar 0/600`.

## Visible evidence

`candidate-visible.png` is the 1920×1080 fb1 capture. The live parent-0
type-2 order gives:

| row | visible cards |
|---|---|
| 1 | Netflix, Prime Video, YouTube, tabii, Canal Digitaal |
| 2 | TV Vlaanderen, Télésat, Digiturk Play, Gain, DAZN |
| 3 | SSIPTV, Rakuten TV, APP, DAZN, DW |

The live DB read-only copy gave orders 15 and 16 as `foxxum_thumb_3753` and
`foxxum_thumb_1685`; both files are 384×216 JFIF/JPEG (SHA256
`57e3951d…` and `0ba9d709…`). Cropping the bottom-right cards and nearest-
scaling the fetched originals gives AE 837.965/40770 and 452.06/40770 pixels
(≈2.1%/1.1%) respectively; the differences are edge/decoder rounding, not
layout or content mismatch. `stock-restored_0.png` is the post-restore fb1
capture (blank OSD frame).

No PNG or JPEG OAI1 stand-ins were used for these cards: `ui.dir` was empty,
so every card came through `image.decode` v2 from the live download directory.

## Restoration and cleanup

- Before start: `fb-restore save /dev/fb1` saved 1920×1080 virt 1920×2160
  32bpp, offset 0,1080, 16,588,800 bytes.
- After stop: `fb-restore restore` wrote the saved pixels and offset
  `0,1080` back.
- `/proc/[0-9]*/exe` scan found 0 candidate executables; `aurora.elf` PIDs
  1967/1785 remained; `/conf/coredumpnoreset` returned 0.
- `/buffer/oa_visible_20261104`, `/tmp/oa_visible.ready` and
  `/tmp/oa_visible.alive` are absent.

This proves the fb pixel/yoffset snapshot was restored, not full region/MAU/
pan ownership equality. `display.set_region=0` avoided a region change and
`display_fb` does not touch MAU, but no independent region/MAU readback was
taken before/after. Full display ownership/restoration and cold startup remain
open.

## Limits

- Two visible JPEG cards and the other 13 visible PNG cards are proven; the
  remaining JPEGs (`foxxum_thumb_3659`, `foxxum_thumb_2750`) are off-page.
- This is a supervised vendor-initialized fb1 test, not independent startup or
  vendor replacement.
- No vendor-class `PngImageDecoderImpl`/`JpegImageDecoder` differential, stock
  apps-grid pixel parity or full display state equality.
- TV access is now revoked by the AGENTS override; no further on-TV test can
  run until the owner re-authorizes it.
