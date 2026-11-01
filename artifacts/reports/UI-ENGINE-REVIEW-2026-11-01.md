# UI engine / skin review — 2026-11-01

Reviewed the split and vendor/settings/Omarchy work through670ce3c. The engine/
skin separation is retained. Captured/reconstructed screens remain limited to
the states in MEMORY; this review does not certify full UI or focused text parity.

## Corrected gaps

The old bridge allocated each event on the heap and queued engine code under
the skin owner. Host disable_posts purges callbacks without a payload destructor:
skin unload leaked payloads, and forced engine unload could dlclose queued code.
Host source confirmed this lifetime contract before implementation. No new
hardware API was introduced.

ui.engine now publishes **v2**. register_skin_dispatch accepts a skin-owned
trampoline with module-lifetime userdata. Only skin code/data is posted under
its owner. The engine retains a bounded64-event value queue, cleared on
unregister/suspend/stop; dispatch checks owner/suspend/drain state. A nonblocking
quiescence gate refuses engine unload inside a running callback. enter failures
do not mark entry successful; unregister invokes leave in the skin context.
The v1 iface prefix is retained but old register_skin fails ENOTSUP. New skins
require v2 and use appended methods; build matching engine/skins together.

Other fixes:

- Vendor re-registers key dispatch after engine replacement; previously it
  reacquired only drawing access. Omarchy now drops stale engine pointers and
  rebinds through its own deferred timer, including forced engine dlclose.
- Rebind checks async/alpha companions even when base display is still bound.
  Actual provider changes repaint; unrelated lifecycle events leave cached
  dialog pixels alone.
- present_pending reports -1/errno for failed GPU completion, preventing rail
  playback from treating it as success. Accepted registration resets readiness;
  the test explicitly rejects same-PID old receipts before any new completion.
- Repaint-timer allocation failures reject start/resume and clean partial
  holds. Asset loading checks exact regular-file size before allocation and
  rejects truncated/trailing payloads and ambiguous/overlong names.
- New skin registration resets the scene transform. The initial hardware run
  timed out because the direction subtest left type4/p1 active: identity pixels
  were correct, but no type0 receipt arrived. It drained and restored fully.
  The fix also prevents skin switches from inheriting unfinished transforms.

The diagnostic itself still demanded an animated Select despite ef2dc4c's
firmware correction: onSelectPress e48405 and focusLauncher e4831c do not start
that animation. It now accepts a newer successful identity receipt by default;
OA_SCENE_PROBE_LAUNCH_ANIM=1 explicitly enables the existing lab-only path.

## Verification and TV state

Full strict host make check passes242 checks. Focused ASan/UBSan engine/skin,
UI async and rebind groups pass. Coverage includes40 queued-input skin reloads,
forced engine removal with queued input, both skins' re-registration, holds,
suspend/resume, timer failures, identity reset and OAI1 validation. Final ARM
skin-contract tests also pass on TV using a recorder display, separately from
the actual hardware run.

Actual GPU run PID4365, after foreground upload and separate hash/size checks:

    UI rail completed image revisions=11 over2000ms (completion-paced; not a frame-order measurement)
    UI selection mode=vendor-instant animation receipt=0 completed screen=1 identity receipt=17179869200
    UI readiness after GPU completion serial=17179869204 revision=17179869544 pid=4365
    UI reload id=24 completed with fresh receipt
    UI host-only suspend/resume completed receipt=17179869206
    UI backdrop raw BGRA=24,23,22,153 expected24,23,22,153
    graphics host drained and unloaded provider
    restored bytes=16588800 yoffset=1080 MAU=0 region_match=1 snapshot_match=1
    probe_exit=0

The direction subtest observes rows0/45/0. Its three grid-mode completions are
not a UI frame count. Eleven rail revisions do not certify30ms scanout, panel
appearance, native IR, settings writes, app execution or TV standby/cold boot.
No new independent visual confirmation was obtained. The lab animation remains
covered by the focused async test.

New isolated ARM artifacts are in build-tv-v3/ui-engine-review. All recorded
symbol requirements are <=GLIBC2.17 (target2.23). final-tested.md5 identifies
engine/skins/unit/probe; final-stage.md5 owns the reusable ignored bundle at
artifacts/workbench/ui-engine-review-stage-20261101. Initial stage.md5/tv-ui.txt
describe the pre-transform-fix run, superseded by tv-engine-verify.txt and
tv-ui-final.txt. Source20 modules and preserved matching production16 are
separate inventories. Installed ABI1/core/assets/recovery remain untouched.

Same boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197, live Aurora1785/zombie1967.
Full framebuffer MD5 before/after41735daa69dc137743b59efe12db4a19, pan0,1080.
Hook/hosts hashes, SSH22/telnet2323, Samba REJECT, oa_safe/coredumpnoreset remain;
oa_boot absent. All staging/backups/readiness/helpers and unit asset directories
removed. No stock restart or persistent deployment. Raw evidence is in
artifacts/analysis/ui-engine-review-2026-11-01.

Remaining scope: automatic selected-skin fallback/control-plane loading, real
OmarchyTV screens, vendor focused-row/text metrics, settings values/tabs and
remaining TV parity. The reconstructed row fixture is not a vendor focus capture.
