# GPU scene-display probe on fb1 — 2026-11-05

First production-module run of the existing EGL/GPU scene path on the real
fb1, with the vendor-shaped portal and the physical remote. This is a probe,
not animation or cold-start acceptance.

## Stack

Staged under `/buffer/oa-gpu-20261105`:

```
plat_stub  graphics  scene_display  settings  service_db
image_decode  ir  ui_engine  ui_vendor
oa-scene-broker  scene-renderer.so  fb-restore  oa-client
```

`graphics` spawned the broker, which loaded `scene-renderer.so`; `scene_display`
published `display` v2, `display.alpha` v1 and `display.async` v1. The core ran
with `FRAMEBUFFER_2=/dev/fb1` and `MALI_MAU_SWITCH=0,0,0,0,0`, after saving the
complete fb1 state with `fb-restore save` (16,588,800 bytes).

`scene_display`'s `blit_scaled` now uses the vendor mipmap sampler
(`oa_canvas_blit_mipmap`) instead of nearest sampling, matching the
`display_fb` card/rail path.

## Result

- `ui_vendor` painted the 5-rail/12-card Startpagina portal through the GPU
  scene path; the fb1 capture is `portal-stable_0.png`.
- The physical remote produced 14 BLAST presses (Right/Left/Down/Up/Select/
  Exit). The portal painted card/rail changes and opened the source dialog.
- Excluding the reload stall, 23 paint intervals had median **260 ms**, minimum
  **129 ms**, and 11 of 23 at or below 200 ms. The previous `display_fb`
  full-frame path measured a median **617 ms**. The GPU path is therefore
  materially faster, but still far from the vendor's 30 ms highlight cadence.
- The EGL renderer was visibly driving fb1; the capture shows the portal drawn
  by the broker/renderer process (PID 6410), not `display_fb`.

## Reload stall

`reload ir` was attempted with both BLAST and UART open. The host loop stalled
for about 64 s and completed the reload at 916756; the `oa-client` process was
killed before it could receive the reply. The IR module now keeps the UART
closed when BLAST succeeds unless `ir.device` is explicit; see
[OA-CLIENT-ASYNC](OA-CLIENT-ASYNC-2026-11-05.md). A `--no-wait` client and a
timeout are available, but the host reload remains synchronous.

## Restore

The host loop was terminated (SIGKILL), the broker child was terminated, and
`fb-restore restore /dev/fb1 fb1-pre.snap` restored all pixels, offset, MAU and
region:

```
restored /dev/fb1: bytes 16588800 offset 0,0 mau 0
```

Staging and the tar were removed, then `/buffer/wdtctl set 10` rebooted. Stock
returned: `aurora.elf` PID 1777, appmgr 1572, fb1 pan 0,0, staging absent.

## Limits

- The GPU path was not shut down gracefully in this run (SIGKILL); broker/renderer
  quiesce and EGL teardown on module stop are unverified.
- The portal was full-frame uploaded each paint; 129-260 ms is still above the
  vendor 30 ms widget cadence. Dirty widget surfaces / partial uploads are the
  next step.
- No timed animation capture, exact AE comparison, settings/apps/source
  remote run, cold startup, native scanout or audio acceptance.
- The source-dialog frame in the first capture was a transient during physical
  key navigation, not a stable screen oracle.
