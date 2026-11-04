# Full fb1 snapshot/restore and cold-start bundle — 2026-11-04

Offline implementation only; the owner's "Don't touch the TV" override was in
force throughout. No device was opened, woken or staged on the set.

## Full fb1 snapshot/restore

`common/fb_snapshot.c` / `include/openaurora/fb_snapshot.h` implement the
recovered layer contract:

- pixels: `FBIOGET_VSCREENINFO`/`FSCREENINFO`, `mmap(smem_len, PROT_READ)`, copy;
- region: `0x8020c86c` read and `0x4020c86b` write of all 8 words;
- MAU: `_IOR(0xc8,0xbd,uint32_t)` read; there is no setter, so restore reads
  it before and after and returns `-ESTALE` if it changed;
- pan/offset: `FBIOPAN_DISPLAY` with the saved x/y offset;
- failures (mmap/msync/region write/pan/MAU mismatch) return negative errno
  while attempting the remaining restoration steps.

`mod_display`/`mod_display_fb` gained `display.restore_snapshot=1`: start saves
the complete state before changing the region, stop restores it before closing,
and suspend restores before unmapping; resume takes a fresh snapshot. The boot
configuration enables it.

`tools/display/fb-restore.c` now uses the same library and writes/reads an
`FBR2` file containing var/fix, all region words, MAU and pixels. The old
pixels/offset-only format is replaced.

## Offline verification

- `check-fb-snapshot`:
  - `fb snapshot`: a fake fb verifies pixels/var/region/MAU save; restore
    writes pixels, region and pan; MAU mismatch returns `-ESTALE` before any
    write; region/pan failures propagate while pixels are still restored.
  - `fb-restore tool`: real temp file plus fake fb saves and restores the v2
    file end to end.
- `check-display` for both display variants: the actual display module starts
  with `display.restore_snapshot=1`, mutates pixels/region/pan, and restores
  them on stop; MAU is verified.
- Full strict host `make check`: **354 ok**, exit 0.
- Focused ASan/UBSan `check-fb-snapshot check-display`: clean, exit 0.
- `verify.cjs`: **43 groups** pass.

## Cold-start bundle (offline assembly only)

`make TARGET=tv BUILD=build-tv-v3/cold-bundle cold-bundle` builds an ARM ABI3
staging directory:

```
build-tv-v3/cold-bundle/cold-bundle/
  openaurora  oa-supervise  fb-restore  openaurora.conf  manifest.sha256
  modules/  (25 mod_*.so)
```

`openaurora.conf` uses `plat,settings,image_decode,display,panel,ir,ui_engine,
ui_vendor`, `display.restore_snapshot=1`, the read-only settings DB and
`/buffer/downloaded_images`. `sha256sum -c manifest.sha256` passes for all 29
entries; every binary is ARM EABI5 hard-float. The supervisor recovery suite
(`tests/recovery.py`) ran as part of `make check` with fake watchdog/fallback
and covers heartbeat, stall, SIGSTOP, startup deadline, failed present, stale
receipts, failure streak and safe markers.

This is **not** a cold-start execution. The watchdog/MCU authority, display
region/MAU equality and vendor fallback on the real set remain unverified. The
bundle is ready to stage only after explicit TV reauthorization.

## Limits

- MAU is read-only; restore can only verify it, not set it.
- The snapshot/restore path is proven with fake fbdev and wrapped device calls,
  not against the TV's live OSD driver.
- Independent startup, full display ownership/restoration and stock parity
  remain open. TV access is currently revoked by AGENTS.md; no further set
  operation can run until the owner re-authorizes it and recovery readiness is
  confirmed.
