# Rail asset export and full-bundle loading — 2026-11-01

Status: verified offline export, corrected icon capacity, and real GPU/UI
lifecycle test. Visible frame order, 30 ms scanout cadence and independent
vendor visual equivalence remain unaccepted.

## Firmware source and export

UI-ANIMATIONS recovers the five category highlight bindings and image placement.
The existing instruction cache was rechecked: calls at e4981a/e498ec/e499e8/
e49ac6/e49b98 select setMainlauncherHighlightId; 10aeef8 stores at +96c;
10af984 loads that id and 10af98a calls SVGImage::create with the rectangle.
Existing Aurora native-window uint16 stores at79a9ec/79a9f6 and Mali MAU parsing
at477fed74 were also rechecked. No new hardware API was introduced.

`openaurora/tools/export-rail-assets.sh` locates the named 5001-byte APNG in
telefunken18_img.bin using the shipped index (offset1673932). It verifies SHA256
d49c1fe5822ea814d7a97234c13e39ae8977557024d9667f3c538a170e0f4267,
210x210 RGBA and eleven30ms frames, decodes one loop, and produces eleven
176412-byte OAI1 files. Independent ImageMagick/ffmpeg first-frame bytes agree.
All eleven frame hashes differ. Existing output directories are refused.
The original APNG's num_plays0 is the loop contract already recovered in
UI-ANIMATIONS; decoding once does not change the UI's loop policy.

Local, ignored vendor-derived output:
`artifacts/workbench/rail-ui-20261101/` (PROVENANCE.txt and SHA256SUMS).
The normal bundle fetched read-only from TV is preserved in
`artifacts/workbench/rail-ui-base-20261101/base.tar`; its hash is in raw evidence.
The complete temporary test bundle is preserved in workbench/rail-stage-20261101.
These assets are local only, outside git. OAI1 remains straight RGBA;
framebuffer storage remains BGRA. No channel swap was applied to the assets.

## Implementation gap and regression

The previous MAX_ICONS24 held title + five glyphs + nine default cards, leaving
only nine slots for the eleven highlight frames. The original rail unit loaded
only highlight images and could not catch this. A complete normal bundle now
reproduces failure against the old implementation; the strengthened test checks
each of the eleven frames and wrap. Capacity is now derived from title + five
glyphs + MAX_CARDS12 + eleven frames =29. Strict host and ASan/UBSan focused
tests pass; full host suite passes226 checks. This is our loader defect, not an
inferred firmware quirk or a change to recovered vendor timing.

The real-display diagnostic now requires an actual type4 completion followed
by a newer successful identity completion on screen1, instead of a fixed1200ms
sleep. Optional OA_SCENE_PROBE_RAIL=1 samples successful image revisions for a
nominal660ms window; this is not a pixel/frame-index or exact cadence test.

## Real TV acceptance and restoration

Foreground upload, then separate size/hash verification preceded execution.
The initial run used the older UI DSO and exposed no diagnostic failure despite
the capacity defect; it is retained as historical tv-ui.txt, not eleven-frame
acceptance. After the fix, tv-ui-fixed.txt records PID10563:

    UI rail completed image revisions=7 over660ms (not a frame-order measurement)
    UI dialog animation receipt=17179869216 completed screen=1 identity receipt=17179869218
    UI readiness after GPU completion serial=17179869222 revision=17179869785 pid=10563
    UI reload id=19 completed with fresh receipt
    UI host-only suspend/resume completed receipt=17179869225
    graphics host drained and unloaded provider
    restored bytes=16588800 yoffset=0 MAU=0 region_match=1 snapshot_match=1
    probe_exit=0

Seven observed image revisions do not prove all eleven frames reached scanout
at30ms intervals; coalescing and CPU repaint/upload throughput need measurement.
No independent owner visual confirmation was obtained. Project-local keys and
application-module suspend/resume are not native IR or TV standby acceptance.

Native isolated build-tv-v3/rail-assets artifacts:

| artifact | bytes | MD5 | maximum glibc |
|---|---:|---|---|
| modules/mod_ui_fb.so |89704|7f0106c721fcb478266f4166127081cb|2.4|
| scene-module-probe |501052|1d6120b69608b09e554b2313ddc87904|2.17|

Other modules/helper/renderer retain UI-ASYNC identities. Initial stage.md5
describes the pre-fix upload; final-tested.md5 and tv-fixed-verify.txt identify
the corrected UI DSO. The preserved local test bundle contains the fixed DSO
and regenerated stage.md5, also saved as final-stage.md5 in raw evidence.
Matching production16-module stage, installed ABI1 and recovery are unchanged.

Same boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197, live stock1785/zombie1967.
Independent full framebuffer MD5 remains41735daa69dc137743b59efe12db4a19,
pan0,0. Hook/hosts hashes, SSH22/telnet2323, Samba REJECT and safety markers
remain. All temporary staging/backups/readiness/helpers were removed after
restoration; persistent assets and stock operation were not replaced.
Raw evidence: `artifacts/analysis/rail-assets-2026-11-01/`.
