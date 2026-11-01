# Direct EGL presentation on the real display — 2026-11-01

Status: native-window diagnostic **visibly passed on the TV**. The owner watched
and confirmed “Upright, full-screen and smooth.” All eight recovered transforms
ran at the diagnostic's50Hz pacing, followed by verified full pixel/region/MAU/
pan restoration. This validates the existing initialized layer, not independent
startup, a production display module, complete vendor UI or cold/resume parity.

## Firmware contract

Sources and partial Ghidra-analysis limits remain those in EGL-SCENE. New logs
under ../analysis/egl-scene-2026-11-01/:

- aurora-native-window-callers.txt / aurora-window-open.txt: ICE_OsdPlaneOpen
  79a838 allocates exactly4 bytes, stores uint16 width and height, and passes
  that native-window pointer through ICE_SetNativeWindowId for OSD plane1.
  ICE_CreateEGLWindow uses native display1 and the cached pointer.
- mali-native-window.txt: generic window setup4775b6cc calls winsys slot18(hex),
  target478000b8. Cleanup slot20(hex),477ff5b8, removes the window from its
  private list, releases color buffers, destroys its semaphore/mutex and frees
  owned storage. Slot1c is a pixmap query, not native-window destruction.
- mali-window-buffers.txt:477fee00 selects the framebuffer for native display1
  using FRAMEBUFFER_2 (default /dev/fb1), reads existing variable/fixed metadata,
  and retains the native width/height pointer.477ffcc8 wraps owned exported
  DMA-buffer descriptors into EGL color buffers with offsets/stride; the native
  path avoids pbuffer readback.477fe7fa releases its fallback mmap.
- mali-swap-mau.txt:477fed74 parses MALI_MAU_SWITCH as per-fb comma-separated
  integers; only1 enables a slot.477fec64 reads variable metadata, calculates
  the next buffer yoffset and commits FBIOPAN_DISPLAY4606. Its MAU-set request
  4004c8bc is used **only** when that per-window switch is enabled. The probe
  sets process-local FRAMEBUFFER_2=/dev/fb1 and MALI_MAU_SWITCH=0,0,0,0,0, so
  it uses the existing primary two buffers without changing MAU policy.

Target driver low-byte21 exports DMA buffers, confirmed by the existing
oa-osd-nondovi-fb.txt FBIOGET_DMABUF branch. MAU GET/SET and pan behavior are
already traced in OSD-MAU-SELECTION. This did not call guessed setters, change
framebuffer modes, allocate a new OSD plane or initialize platform HAL.

## Backend and diagnostic

common/scene_gles.c adds explicit open_window and native swap to its existing
offscreen owner. The four-byte native-window dimensions live inside that owner
until cleanup. Surface dimensions are queried and checked. NULL input/size0
reuses the previously uploaded texture; an empty cache rejects with ENODATA.
Pbuffer mode still requires bottom-up RGBA output, while window mode requires
NULL output/size0 and presents through eglSwapBuffers. GL draw errors reject
before swap. Owned PID/thread/context checks and retained failed-close handles
remain in force. There is still **no production display service integration**
or display.renderer configuration switch.

tools/display/egl-window-probe.c refuses any profile except fb1 primary MAU0,
1920x1080, virtual1920x2160,32bpp, stride7680, backing16588800bytes and legal
frame offsets. Before EGL creation it saves the entire mapping and metadata to
a0600 fsynced /tmp backup. It paints two identity frames and independently checks
raw BGRA corners/orientation. It then runs all eight types with the recovered
cubic easing, one second per type, using the cached GPU texture and a20ms
absolute-clock pacing target. No framebuffer copy/readback occurs per frame.

After EGL context/surface/thread cleanup, it restores all pixels and original
pan, verifies full memory equality, current MAU and unchanged region. INT/TERM/
HUP request orderly cleanup. A separate --restore path can load the saved backup
after an abnormal exit; that emergency CLI path has not itself been exercised.
Normal full restoration **has** passed. Backup/helper files were removed only
after successful restoration and the separate state check.

## Real hardware results

Tested window binary **88780bytes**, MD5**5a66a935317e0623660151ffac96a926**,
maxGLIBC2.17; preserved under openaurora/build-tv-v3/egl-window/egl-window-probe
(the earlier egl-scene build copy has the same hash). Host/ARM strict C11
-Wall -Wextra -Werror -pedantic builds and qemu loader-only closure pass.
No host GPU/recording display supplied these results.

Native identity sample, BGRA:

    top-left 80,0,0,255; bottom-left 80,224,0,255; top-right 80,0,224,255

For types1..8, each run reported51 presents in1000.43..1000.49ms, including
the final endpoint. The owner independently reported the grid upright,
full-screen and smooth. These observations validate this diagnostic sequence,
not vendor-identical timing, all frames or every UI interaction. The vendor's
10ms controller sampling is not a100Hz physical scanout requirement: display
refresh, renderer pacing and controller sampling must be measured separately.

Full backing MD5 before/after:
**41735daa69dc137743b59efe12db4a19** (4050x4096=16588800bytes).
The probe also returned:

    restored bytes=16588800 yoffset=0 MAU=0 region_match=1 snapshot_match=1

Same stock boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197, live Aurora1785 and
zombie1967. Installed core/hook/hosts hashes, SSH/telnet, Samba REJECT and safety
markers remain intact. No reboot, service stop, input capture, source/audio/CEC
change or persistent deployment. /tmp/oa-egl-window-20261101 and its backup were
removed; no probe remains running.

The revised pbuffer diagnostic additionally passed native cached-texture,
undersized-input, no-upload and foreign-thread render/close rejection checks,
plus all prior pixel/reopen cases. Current regression binary **74504bytes**,
MD5**aed778d42cc3aa4e3ff8ddc995b9767c**, under build-tv-v3/egl-window.
Its staging was also removed and full backing hash/pan stayed unchanged.
The original70384byte pbuffer diagnostic identity in EGL-SCENE is historical;
the source is preserved by commit8a535b6, while the initial build path was
subsequently rebuilt. Do not confuse that historical hash with the current binary.

Evidence: tv-window-{upload,verify-before,runtime,after,cleanup}.txt,
window-{arm-build,host-build,loader-qemu,final-arm-build}.txt and
tv-window-regression-*.txt. Host default core/module sources remain unchanged
since the203-check EGL-SCENE run; optional GPU diagnostic builds are separate.

## Next integration gate

Keep all blocking EGL creation/render/swap/destruction on one owning worker.
ABI3 quiesce must remain nonblocking and retain the worker/library on close
failure. Do not simply put synchronous eglDestroyContext in the host's release
gate or expose a blocking swap RPC on its loop.

Design bounded/coalesced frame submission, immutable input ownership, cached
texture revisions and completion tickets. Existing display.present promises a
committed frame; async submission cannot masquerade as that success or cause
ui_fb/supervision to acknowledge readiness early. Add an explicit async service
contract/completion path, then test real module start/stop/reload/suspend/resume,
failure cleanup and first-frame receipts on TV. Normal ui_fb still uses the CPU
display backend; the diagnostic's visible success does not close those gates.
