# Coalescing scene display and completion receipts — 2026-11-01

Follow-up: UI-ASYNC-2026-11-01 now implements/tests ui_fb receipt/readiness and
actual GPU UI/lifecycle. The status below records this earlier adapter milestone;
full visual/input/cold acceptance remains open.

Status: optional `scene_display` supplies an RGBA canvas, bounded coalescing and
`display.async` receipts over the actual `display.gpu` provider. Four groups pass
host/sanitizers/TV. A real TV burst retained its final identity frame and the
full diagnostic observed412 completed GPU frames with complete restoration.
ui_fb still uses synchronous present; its async receipt/readiness integration
is the next step. Do not interpret this provider as an already working GPU UI.

## Firmware scope

No new hardware API was introduced. Native renderer/driver calls remain the
ARM-verified EGL-WINDOW/GRAPHICS-MODULE path. Live Mali hash still matches the
offline source: d1aa5c1679a9467ec51865d4f2120112. Firmware controller evidence
was rechecked before choosing completion behavior: `oa-animation-controller.txt`
under analysis/ui-animation-2026-10-31 decompiles cancel11399e0 (erase without
forcing progress), finalise1139a30 (apply1 then erase) and onAnimationTimer1139aa8
(clamp1, call target, remove completed entry, only rearm10ms while work remains).
ARM1139a76 loads1.0 for finalise;1139aee/1139b26 establish the timer clamp.
UI-ANIMATIONS records dialog cubic easing and full-screen HAL dirty state.

The queue is our transport design, not a claim that vendor firmware used this
IPC/coalescer. It retains the final requested state after controller timers stop;
it does not change cancel into finalise. Full vendor sequence/cadence acceptance
still requires an integrated UI. The existing controller/UI semantics are not
changed by this milestone.

## API and ownership

`scene_display` is an explicit alternative to the CPU `display` module. It
requires/holds display.gpu and publishes drawing-compatible display v2 plus
display.async v1. It has no plat/NDAL dependency, device mapping or hardware
setter. Fixed canvas profile1920x1080 matches the native provider, which refuses
other geometry before opening its EGL window. Do not load both display providers.

Drawing uses common/canvas.c: top-down RGBA, clipped fill/scaled blit and the
existing opaque-canvas alpha compositing rule. This is not raw framebuffer BGRA;
the established GL/native path supplies that output format. A new image revision
is assigned after drawing. Private sealed image files preserve requested pixels
even if later drawing changes the mutable canvas.

At most one provider request and one latest desired frame are retained. Replacing
queued content closes the obsolete queued file. Transform-only requests reuse a
queued snapshot or the provider's accepted image/cache revision. The owned10ms
timer continues draining independent of UI animation timers. Native completion
advances the cached revision and submits the retained final request.

`request(serial*)` acknowledges queue acceptance only. `latest(result*)` returns
a copied retained completion with serial/revision/type/progress/error. Repeated
reads return that value; consumers compare serials. Coalesced serials receive no
success claim; only actual successful GPU work produces a successful receipt.
Errors do not acknowledge frames. Legacy display.present returns ENOTSUP on this
provider, preserving its existing synchronous-commit meaning. A consumer must
hold both display and display.async and must not acknowledge readiness on request.

Receipt and image revision namespaces include the host owner id. Activation
also increments image revision: a stopped/restarted or reloaded consumer can
reuse an already-open GPU provider, whose cache must not receive an old upload
revision. OPEN is reused if the provider reports initialized; otherwise it is
submitted asynchronously after prior canceled work drains.

Suspend/stop deletes the consumer timer, cancels its pending receipt and closes
queued files. Already transferred data belongs to the GPU provider (SCM_RIGHTS),
so no consumer code/canvas pointer remains borrowed by the child. The provider
still owns hardware drain; cancel does not interrupt it. Resume retains canvas,
reopens/reuses the provider and requires a new uploaded revision. The normal host
dependency/hold order releases the consumer before the GPU provider.

## Tests and hardware

Four strict host, ASan/UBSan and actual-TV groups cover:

1. RGBA clipping/scale/alpha and INT_MIN/INT_MAX coordinate safety.
2. Delayed completion, superseded requests, final-frame drain without another
   request, immutable inflight/replaced snapshots and cached transform uploads.
3. Consumer reload namespaces and suspend/resume reopen/reupload behavior.
4. Unload while the provider retains inflight data, queued-file release and
   error receipts that cannot acknowledge a frame.

The injected fixture records completions and retained descriptors; it does not
emulate a display. The actual display test uses the real modules/helper/Mali.
Full strict host make check passes222 checks. Source now builds18 modules;
matching16-module production ARM stage and older diagnostics remain preserved.

Actual native `scene-module-probe`, with explicit OA_SCENE_PROBE_ASYNC=1 and
OA_SCENE_PROBE_MODULES staging path:

    scene display module id=4 service=display.async
    identity raw BGRA top=80,0,0,255 bottom=80,224,0,255 right=80,0,224,255
    coalesced101 desired frames first=17179869187 final=17179869287
    final identity receipt=17179869287
    scene display observed frame completions=412
    graphics host drained and unloaded provider
    restored bytes=16588800 yoffset=0 MAU=0 region_match=1 snapshot_match=1
    probe_exit=0

The burst submits without pumping completions, retaining one inflight and one
final desired frame. Its101 requests become2 observed completions. Including
two initial identities and8x51 animation frames yields412 actual completions.
Each effect took1009.79..1031.77ms at the diagnostic's50Hz target; do not label
those measurements exact vendor cadence or a new independent visual acceptance.
Normal identity corners and the full restored backing were checked independently.

ARM artifacts preserved under `openaurora/build-tv-v3/scene-display`:

| artifact | bytes | MD5 | maximum glibc |
|---|---:|---|---|
| modules/mod_scene_display.so |53236|9293d421420b1ad0d7b03e41f3a57f1a|2.4|
| tests/scene_display |469528|7f6f98030dcd8f40631713f2e369435a|2.17|
| scene-module-probe |487196|a62146c477759bfb3d3e3d94d2dd055e|2.17|

Foreground upload, separate size/hash verification, native tests exit0. Staging,
backup and helpers removed after the independent after-state check. Stock boot
71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live Aurora1785/zombie1967 unchanged.
Full backing MD5before/after41735daa69dc137743b59efe12db4a19, pan0,0; hook/hosts,
SSH22/telnet2323, Samba REJECT and safety markers retained. No persistent
deployment, reboot, stock stop or audio/CEC/input operation. Evidence is in
`artifacts/analysis/scene-display-2026-11-01/`.

## Next

Wire ui_fb to hold/release display.async with display, use request/latest, retain
an owned completion sampler, and write ui.ok_file only after an actual successful
receipt for its requested generation. Preserve CPU fallback behavior. Current
on_anim repaints the whole scene for each dialog progress; use cached scene
presentation for transform-only updates, while retaining genuine rail/title
updates. Recheck cancel/finalise/teardown firmware paths before changing behavior.
Test service removal, UI reload/suspend/resume, failed receipts and then actual
UI on the TV. No display.renderer option or automatic provider selection exists.
