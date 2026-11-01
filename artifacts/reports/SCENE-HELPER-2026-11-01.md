# Separate native graphics process — 2026-11-01

Status: explicit bootstrap/renderer and real-display diagnostic pass on the TV.
Production display module integration is still open. This extends SCENE-BROKER's
tested IPC with actual EGL/GLES execution; it does not add an asynchronous host
service, first-frame readiness, reload/suspend integration or cold ownership.

## Startup and lifetime

`tools/display/oa-scene-broker.c` links libc/libdl only. It validates a connected
AF_UNIX/SOCK_SEQPACKET on stdin, closes unrelated inherited descriptors, sets
the process-local firmware-backed framebuffer/MAU environment, and only then
loads an explicit absolute renderer path with RTLD_NOW|RTLD_LOCAL. ELF NEEDED
records confirm the bootstrap has no Mali/EGL dependency. This resolves the
constructor ordering concern recorded by SCENE-BROKER.

`scene_renderer.h` defines a private version1/size-checked data export containing
the backend callback table. This is separate from host module ABI3. Using a data
symbol avoids a nonstandard dlsym object/function pointer conversion. The
renderer remains loaded until process exit: closing EGL handles does not prove
that every vendor-library thread stopped executing library code. The bootstrap
therefore does not call dlclose. A future host module owns/reaps this process
instead of loading Mali into the host.

`tools/display/scene-renderer.c` checks the known primary fb1 profile through
GET-only requests before opening a native window. It requires the same1920x1080,
virtual1920x2160,32bpp,stride7680,16588800bytes, MAU0 and legal pan established
in EGL-WINDOW. No guessed mode/MAU/region setters or HAL init were introduced.
Common scene_gles adds an owned-process/thread/context-checked blocking finish,
calling glFinish before the IPC server releases the input mapping and replies.
All EGL operations remain in the child. Mali loading itself has constructor
effects/logging; this is not a view-only runtime health check.

The parent must redirect stdout/stderr before exec. The diagnostic does so using
posix_spawn file actions. Target libc can report spawn success before exec
failure; receive/EOF and reaping remain necessary. This is already established
in MIXER-MODULE, not an emulator defect.

## Tests and real hardware

`tests/scene_renderer_fixture.c` uses loader init/fini entrypoints. Its initializer
checks the preset environment and absence of inherited fd128, then opens its own
descriptor. Its OPEN/render callbacks verify that descriptor survived. The
spawn test also uploads an immutable image, renders from cache, drains on CLOSE
and EOF, reaps, and rejects missing/wrong-ABI renderer libraries. These two
process test groups pass strict host, ASan/UBSan and actual TV. They establish
process/DSO lifetime, not hardware. Full host `make check` passes209 checks.
The ARM artifacts build strictly; no ARM/qemu subprocess acceptance is claimed
for this new exec test. The lower IPC's ARM/qemu evidence remains SCENE-BROKER.

`scene-window-probe` compiles the existing full-snapshot EGL-WINDOW diagnostic
with a separate blocking IPC client. Neither this parent nor the bootstrap links
Mali. The diagnostic saves/fsyncs a complete framebuffer backup before spawning,
uploads the top-down RGBA grid once, then sends cached transform records. The
child renders and finishes each frame before replying. On cleanup, parent closes
the connection, waits for child drain/exit, then restores pixels/pan and checks
MAU/region/full memory equality. An abnormal child death leaves cleanup unproven;
repeated close calls cannot silently clear that state and authorize restoration.
The backup stays available. A permanent failed child close intentionally blocks
the diagnostic rather than restoring under an owner still able to write.

Actual native run:

    identity raw BGRA top=80,0,0,255 bottom=80,224,0,255 right=80,0,224,255
    types1..8: 51 frames each, elapsed1004.81..1012.30ms
    broker child=21079 reaped status=0
    restored bytes=16588800 yoffset=0 MAU=0 region_match=1 snapshot_match=1
    probe_exit=0

This is410 completed frame requests including the two identity frames, with
real GPU rendering/swap/finish and cached textures. The prior direct diagnostic
had independent owner visual confirmation; this helper run has pixel/orientation,
timing and restoration evidence, without a new independent visual observation.
It does not establish all vendor frame sequences or production interaction parity.

Tested ARM binaries under `openaurora/build-tv-v3/scene-helper`:

| artifact | bytes | MD5 | maximum required glibc |
|---|---:|---|---|
| oa-scene-broker |57304|596643ed63c76231e6a7e31fd22d7efd|2.4|
| scene-renderer.so |68064|f4196fa2413cbf60efdfb136d8b56e4b|2.4|
| scene-window-probe |74588|d875eff769e4b421dd6e6d4d70bf14fb|2.17|
| tests/scene_spawn |54496|899001184ece6a2a7be41db88a9cc1bb|2.15|

Both fixture DSO identities and complete ELF imports are recorded in raw logs.
Upload was foreground; every staged artifact's size/hash was checked through a
separate connection before execution. Real RTLD_NOW renderer loading and frame
execution passed; a compiler-version guess or loader exit alone is not the proof.

The diagnostic and backup were under `/tmp/oa-scene-helper-20261101`; all files
and directories removed after independent restoration verification. Child21079
is absent and no test/helper remains. Same stock boot71f7eb63-727f-4b9a-a9c7-
2b04dc3f9197, live Aurora1785/zombie1967. Full framebuffer MD5before/after
41735daa69dc137743b59efe12db4a19; pan0,0. Hook/hosts hashes, SSH22/telnet2323,
Samba REJECT and safety markers retained. No stock service stop, restart, input
capture, audio/CEC operation or persistent deployment. The matching16-module
production stage and historical diagnostic/recovery builds are unchanged.

Evidence: `artifacts/analysis/scene-helper-2026-11-01/` contains strict builds,
sanitizer/process cases, ELF imports/versions, upload verification, native run,
independent after-state and cleanup. Build optional targets with the explicit
TARGET=tv BUILD=build-tv-v3/scene-helper and glibc cross compiler; do not use
`make tv` or `make clean`. `check-scene-spawn` runs process fixtures, not graphics.
The hardware diagnostic is never run by make check.

## Continuation

Add a nonblocking parent provider using this executable and renderer. The
blocking diagnostic client is not suitable for the host loop or ABI3 gate.
Keep one pending request/uncollected result, bounded coalescing of transforms,
immutable revision ownership, final-frame retry, completion-driven first-frame
readiness, service holds, EOF drain and WNOHANG reaping. Required provider holds
and failed cleanup must survive stop/reload/suspend; do not force-kill a busy GPU
owner or dlclose its vendor dependencies. Then test actual module lifecycle and
display behavior on hardware. CPU display remains the production path; no
display.renderer switch exists yet. Cold/resume and full UI/audio/CEC parity
remain open.
