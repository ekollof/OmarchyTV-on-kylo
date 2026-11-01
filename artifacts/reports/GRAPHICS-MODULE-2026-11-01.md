# Reloadable native graphics service — 2026-11-01

Status: `mod_graphics`/`display.gpu` v1 implements an asynchronous service with
owned watch/timer and nonblocking ABI3 quiescence. Four lifecycle groups pass
host/sanitizers/TV. The actual service also completed410 GPU frames on real fb1,
then drained/unloaded through the host reactor before full restoration. UI scene
coalescing, completion-driven readiness and GPU-backed ui_fb remain open. The
default display v2 remains CPU-based; no display.renderer option exists.

## Firmware authority

The new service/lifecycle is our IPC design, not a vendor hardware ABI. Neither
mod_graphics nor the parent client introduces a device ioctl or EGL call. The
native renderer/common scene owner are unchanged from SCENE-HELPER. Before this
hardware run the existing decompiled and ARM extracts were rechecked:

- `aurora-window-open.txt`:79a9ce..79a9d2 allocates1x4 bytes;
  79a9ec stores width as a halfword at0,79a9f6 height at2. The original caller
  register assignments at79a854/79a85a retain height/width respectively.
- `mali-swap-mau.txt`:477fec6c requests GETvar4600.477feca0..477feca6 tests the
  per-window MAU flag; its clear branch skips the MAU setter.477fed00..477fed06
  issues pan4606 with the framebuffer metadata.477fed74 parses the process-local
  comma-separated switches; only integer1 enables a slot.
- That parser calls strtok_r on getenv's returned storage. It can mutate the
  child's environment string; do not replace process isolation with an assumed
  reloadable in-process cache reset. Every new helper gets a fresh preset.

These are the extracts under `artifacts/analysis/egl-scene-2026-11-01`, not a
new full Ghidra analysis or speculative probe. Offline and freshly queried TV
libmali MD5 both **d1aa5c1679a9467ec51865d4f2120112**. Geometry/MAU checks and
full backup precede native window creation exactly as EGL-WINDOW established.
Success codes alone are not used to infer display effects.

## Service and lifetime

Configuration requires explicit absolute `graphics.broker` and
`graphics.renderer` paths. Start reserves its owned20ms reap/completion timer,
spawns the EGL-free bootstrap and registers its owned fd watch. A failed watch
registration keeps that timer/context until the child drains. Publication/start
does not open a native window; the consumer submits OPEN and collects it.
Mali loading itself has constructor effects and this provider is not view-only.

The typed interface supplies open/frame/take/cancel/is_open. One pending request
or uncollected result; no borrowed userdata/callback. Image fd stays caller-owned.
FRAME completion is render/swap/finish, not submit acceptance or proof of scanout.
Canceled work still drains. Public tickets combine the host module owner id and
sequence. This matters: the lower client's static instance counter can reset
when its DSO is unloaded/reloaded. It is insufficient as a public reload token.
The service maps its internal tickets; reload and suspend cannot alias old ones.

Only module-owned timer/watch callbacks pump and remove watches. Service calls
from consumers do not register callbacks under a consumer's owner id. Quiesce
closes IPC, preserves copied completion/cancellation state, and returns busy until
WNOHANG reap. It never waits, signals or drops a busy child. Stop removes the
timer/publication after the gate succeeds. Suspend retains the service and its
holds; resume creates a new helper and needs a new OPEN/image. Failed resume
drains partial resources while retaining the completion timer.

An abnormal child death is reaped but latches cleanup_unproven. The module stays
loaded/faulted, including under --force; no successful EGL release is inferred.
There is currently no automatic recovery/clear operation for that latch. Native
abnormal-release/recovery semantics must be established from driver/firmware
before adding one. The test exercises this with a device-free self-killed peer
inside an isolated test process, never a real GPU owner or stock Aurora.

## Evidence

Four strict host/ASan-UBSan/actual-TV lifecycle groups:

1. Owned watch/timer, bounded submit/cancel, cached frames, service holds across
   suspend/resume, failed resume and public ticket invalidation on actual DSO reload.
2. FIFO-blocked child close keeps DSO/context/timer alive; --force cannot unload.
3. Config/spawn/timer/watch failures unwind services/children with a completion path.
4. Abnormal peer exit remains faulted after reap and cannot masquerade as cleanup.

Full host `make check`:218 checks pass. Native module is loaded/used in a real
standalone host; no ARM/qemu subprocess acceptance claimed. Source now builds17
modules; the preserved matching production ARM stage is still its earlier16.
This is not an all17-module production preflight/deployment.

`scene-module-probe` reuses the complete snapshot/BGRA/restore diagnostic and
loads the actual provider via an explicit OA_SCENE_PROBE_MODULES directory. Its
blocking adapter pumps real service watches and due timers for frame collection.
Shutdown uses oa_host_run's actual reactor to retry quiescence until unload.
The diagnostic adapter is not production UI code or a cadence-equivalence test.

Actual display: OPEN ticket4294967297, two identity plus8x51 effect frames.
BGRA corners match80/0/0/255,80/224/0/255,80/0/224/255. Effects take
1005.44..1006.59ms each at the diagnostic's50Hz target. Host shutdown reports
busy cleanup across its timer iterations, then `graphics host drained and
unloaded provider`. Complete16588800-byte snapshot, pan0, MAU0, region restored;
probe exit0. No new independent owner visual confirmation is claimed.

ARM artifacts preserved under `openaurora/build-tv-v3/graphics-module`:

| artifact | bytes | MD5 | maximum glibc |
|---|---:|---|---|
| modules/mod_graphics.so |69900|2afefd7ffaedc6bff9e411561e05ddc0|2.15|
| tests/graphics_module |478980|f3ff7b116074fe7cee3f7440cb2fc065|2.17|
| scene-module-probe |483724|3e091ecdf8ef747c6c949d70b24d6415|2.17|

Helper/renderer identities remain596643ed63c76231e6a7e31fd22d7efd and
f4196fa2413cbf60efdfb136d8b56e4b. Foreground upload, separate size/hash
verification before execution. All staging/backup/FIFOs/helpers removed after
independent framebuffer hash check. Same stock boot71f7eb63-727f-4b9a-a9c7-
2b04dc3f9197/live Aurora1785/zombie1967, full framebuffer hash
41735daa69dc137743b59efe12db4a19 and pan0,0. Hook/hosts hashes, SSH22/telnet2323,
Samba REJECT and safety markers unchanged. No persistent deployment, stock stop,
restart, audio/CEC operation or input capture. Raw evidence is in
`artifacts/analysis/graphics-module-2026-11-01/`.

Follow-up: SCENE-DISPLAY-2026-11-01 adds/tests the canvas/coalescing consumer and
async receipts, including a real burst/final GPU receipt. ui_fb/readiness and
actual UI lifecycle below remain open.

Next: integrate a GPU-backed display consumer with bounded transform coalescing,
immutable scene revisions, final-frame retry and completion-driven first-frame
readiness. Preserve service holds through actual ui_fb reload/suspend/resume.
Then verify full vendor frame sequences/interactions and cold/resume ownership.
New hardware behavior requires firmware/decompilation first, as owner reaffirmed.
