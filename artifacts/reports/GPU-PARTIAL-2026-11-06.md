# Partial GPU scene uploads — 2026-11-06

The GPU scene path previously uploaded the whole 1920x1080 canvas for every
present. The rail highlight already reduced the CPU paint to `0,211 209x670`;
this change carries that rectangle through the scene protocol, broker and
renderer so the GPU also receives only the changed 209x670 region.

## Protocol and implementation

- OGS1 FRAME requests now carry `x,y,w,h` in the 64-byte record (offsets
  36/40/44/48; bytes 52..63 remain reserved). Uploads must be inside the fixed
  1920x1080 scene; cached frames carry all four as zero.
- `oa_scene_image_create` accepts any 1..16,588,800-byte RGBA payload; the
  broker validates that the sealed image size is exactly `w*h*4`.
- The renderer backend ABI is now 2. `scene-renderer` calls
  `oa_scene_gles_upload`, which does `glTexSubImage2D` for the rectangle on
  the retained texture, then draws the cached texture. A partial region
  requires a prior full upload; the first frame after OPEN/resume is full.
- `display.gpu` is now v2 with `frame_rect`; `graphics` packs the rectangle
  into the OGS1 request. `scene_display` looks up v2 with a v1 fallback,
  tracks a dirty scene rectangle from `fill`/`clear_rgba`/`blit_scaled`/
  `patch_fill_rgba`, creates `w*h*4` images for changed regions, and clears
  the dirty rectangle only after the image is created. `scene.profile=1` logs
  request/complete revision and rectangle.

## Verification

Host tests passed:

```
ok   OGS1 endian/float records, reserved fields, atomic rejection and bounds
ok   upload/cached revisions and completion held until GPU finish; caller FD released safely
ok   scene_display uploads only the patch rectangle; the GPU fixture keeps the surrounding scene
ok actual graphics/scene_display IPC: gpu/display services, hold refusal, dependency-ordered reload, child reap and reuse
```

The broker test now submits a 2x2 image at `(1,1)` and asserts the backend
receives exactly `size=16, x=1,y=1,w=2,h=2`. `scene_display` patches a 1x1
pixel and the fixture shadow keeps neighboring pixels unchanged.

## On-TV measurement

Staged the ABI 3 GPU stack (`plat_stub`, `graphics`, `scene_display`,
`settings`, `service_db`, `image_decode`, `ir`, `ui_engine`, `ui_vendor`,
`oa-scene-broker`, `scene-renderer.so`) with the real fb1 snapshot. The log:

```
[701511] scene request rev=... rect=0,0 1920x1080 upload=1
[701584] scene done    rev=... serial=...
[701632] scene request rev=... rect=0,211 209x670 upload=1
[701663] scene done    ...
...
[702273] scene request rev=... rect=0,211 209x670 upload=1
[702293] scene done    ...
```

The first full upload took ~73 ms request-to-done. Ten rail frames each
uploaded only `0,211 209x670`, completing ~20 ms after request and pacing at
~50 ms per frame (~500 ms for the ten-frame sequence). This is the GPU
equivalent of the CPU partial path; no full-canvas upload occurred during the
highlight.

Capture: `artifacts/analysis/gpu-partial-2026-11-05/portal_0.png`. Log:
`run.log`.

## Restore

The staged core/broker were killed after capture; `fb-restore` restored the
16,588,800-byte fb1 snapshot, staging removed, and `/buffer/wdtctl set 10`
rebooted. Stock returned: `aurora.elf` PID 1781, appmgr 1579, fb1 pan 0,0,
staging absent.

## Limits

- Request-to-done is ~20 ms and the animation cadence ~50 ms, not the vendor
  30 ms; further controller/compositor work remains.
- The CPU canvas still redraws the rail strip into scene_display's canvas;
  only the GPU upload and conversion are partial.
- The full-frame request remains ~73 ms.
- The test used SIGKILL plus the snapshot/reboot recovery path; graceful GPU
  teardown is still unverified.
- No AE comparison of the GPU final frame against a fresh full upload; the
  fixture shadow and capture demonstrate the rectangle update.
