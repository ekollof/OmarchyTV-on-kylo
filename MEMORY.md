# Current project state — last verified 2026-11-03

This is the canonical state summary. Latest narrow verification is recorded
in the dated entries below; the earlier CEC session is historical. Notes are preserved in
[the archive](artifacts/archive/README.md); they contain superseded claims.

## TV and persistent recovery

UI-FONT-GPU-2026-11-03: lower scene_gles coverage-glyph/FBO composition API
implemented from shipped ARM/shaders. Full strict host, focused ASan/UBSan,
ARM-qemu geometry and owner-approved native private-pbuffer synthetic/title
tests pass. No service/wire/UI integration, no window/visible acceptance.
One-pass core1971 still differs from vendor1446 by525; two-pass core1228 differs
by1150, so this is NOT a closed pixel fix. Native-varying versus old-composition
shader gives identical two-pass full hash; do not fit offsets or blame fonts.
Persistent framework/task/client/renderer/context graph was read, but actual
title draw execution was not traced; the old Surface was freed/rejected.
Final native160268 bytes/MD5 772970e414644b47ec2c3b415a7a1cbd/maxGLIBC2.7,
build-tv-v3/glyph-gpu. Corpus8294400/25e09914c06a252c31cf92de49d419ef.
All staging/helper gone; same boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197,
fb1 pan0,1080/full backing41735daa69dc137743b59efe12db4a19, hook/hosts,
SSH22/telnet2323 and /vendor/iptables Samba REJECT. No display drawing/input/
settings/restart/persistent mutation. ui.title stays default-off; no job pending.

FONT-X-OFFSET-2026-11-03: closed getter error: b42468 returns -signed font+5,
not zero. Guarded live font18 header reads0xe9 (-23); shipped font18/font7
minimum-lefts-23/-18 explain title23 and normal MODE-row18. common/font_box
now reserves the global offset and positions start/center/end accordingly.
Title origin23,5/ink136..388,31..85 matches saved vendor bounds; core pixels
still differ, so title default-off and complete raster/style parity remain open.
Full host/focused sanitizers/ARM-qemu/native memory-only corpus pass. Native
162884 bytes/MD5 22074919b41c8f0e789f0e3615ae3029/maxGLIBC2.7, build-tv-v3/font-x.
Separate size/hash verification before execution; staging gone/no helper/job.
Same boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/hook/hosts, SSH22/
telnet2323 and Samba REJECT; no display/input/device/settings/persistent change.

UI-TITLE-LIVE-GRAPH-2026-11-03: owner opened stock settings panel. Later ready
capture confirms red panel and title ink136..388/31..85. View-only heap scan plus
firmware-grounded pointer reads find foreground Surface108,5,691,95 and retained
LineObj23,5, exact Instellingen/zero margins/start1/center3/unit glyph ratio.
Flag+1b1 maps to CharClipping, not bold. X assignment remains open; no UI source
or coordinate change. Offline saved graph verifier passes; private data durable
in workbench/title-horizontal-20261103. Scanner32936 bytes/MD5
9d542f8a2cc4121590a18a45552871f1, separate verification before execution,
removed/independently checked. Same boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/
live1785/hook/hosts. No drawing/keys/settings/device/persistent change or helper/
job pending. Earlier launcher captures were stale relative to owner ready cue;
no MAU defect established, no fbregion execution. Owner can use TV normally.

UI-TITLE-BOX-2026-11-03: optional title now uses existing boxed alignment/clip
renderer, full583x90 viewport; font18 height80 gives local Y5. Corpus absolute
ink113,31..365,85 agrees vertically with dated vendor sample;23px X open, title
default-off. Full host/focused sanitizers/ARM-qemu and native memory-only corpus
pass. Native tests/font_box154344 bytes/MD5 4da2f04d1f5f857d2a69b69598cbf9da,
maxGLIBC2.7, build-tv-v3/title-box. Separate hash check before execution, staging
removed/absence checked and no helper/job pending. Current read-only sample:
same boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785, firmware/hook/hosts
hashes match, SSH22/telnet2323 listening, Samba OUTPUT REJECT retained. No
display/input/device/settings/persistent operation; not reboot persistence proof.

UI-WORK-REVIEW-2026-11-02: reviewed through6822fce, fixed scanner read-span
overflow and ui.edit_init bypassing the lab flag. Full host/focused sanitizer/
ARM-qemu checks pass; build-tv-v3/review-20261102, details/hashes in report.
No TV connection, display operation or deployment in this review. The other
agent's saved live-scan-title-2026-11-02.txt records a created font18 title
candidate and actual list/row geometry; this supersedes the earlier absence
sample for those observations, not a current live poll. ui.title remains
default-off with unexplained draw/layout delta. Durable copy of that agent's
research: artifacts/workbench/agent-ui-review-20261102; no helper/job pending.

UI-ROW-LABELS-2026-11-02: offline label-source trace completed. Ordinary key
constructor/drawRow/setLabel add no prefix; langprofile Languages/Dictionary
contains41 locales/2435 records. MODE hash5aa5907f record987 resolves dut slot10
to exact Modus bytes4d6f64757300. Hash-checked bounded full-table evidence script
passes; live translation-fit flag and actual parent/resource/ratio state remain
open. No fitted-coordinate/renderer/persistent change and no TV connection in
this investigation. Owner's latest observation is awake HDMI1 external video;
last verified device/access/privacy state is the following historical sample.
Handoff updated and work stopped as requested; no helper/build/Ghidra job pending.

UI-LIVE-OBJECTS-2026-11-02: read-only vtable-candidate scanner added/tested.
Native32736 bytes/MD5 c7ac91ee12465bad32819cebfbe13c57/GLIBC2.7,
build-tv-v3/ui-scan. Full strict host/focused sanitizers/ARM-qemu pass; native
four-heap-range176934912-byte scan completes:6 uncreated textbox candidates,
no settings/row/list/dialog candidate in that subset (not global absence).
Owner reports awake HDMI1 external video. Blank OSD/WS OTHER do not mean standby
or crash; one Menu1048 POST201 produced no observed menu. No navigation/power/
device operation followed that report; this turn returned to view-only work.
Old wspoll.c not run due to concrete parser bugs; host Node WS queried state.
Same stock boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967/
firmware64c252340d3af6c5d66072611b7bad7a/access/privacy; staging removed and
checked, no helper/job pending. No persistent change. Actual settings instance/
text/ratio/skin and visible acceptance remain open; fitted origins unchanged.

UI-TEXT-VIEWPORT-2026-11-02: common/font_box.c joins recovered widget alignment
mapping with vertical/local line rendering. Exact9-alignment pixels and actual
460->489/488/font7/20 render54 offscreen lines, Y2/1. Full strict host/focused
sanitizers/ARM-qemu/native TV CPU tests pass. Native152496 bytes/MD5
e0c91d6934f7803622016fd48bbb427a/GLIBC2.7, build-tv-v3/font-box. Same stock
boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967/access/privacy;
staging removed/checked, no helper/job pending. No display/device/settings/
persistent operation. Settings/Picture/SVGDialog margin bindings and BitmapFont
X offsets were incorrectly called0; FONT-X-OFFSET corrects that to the negative
global minimum-left bearing. Base-list ctor insets0, setLabel adds no fixed prefix. Actual
parent/window/text/resource/ratio state still open: conditional origin31,184
does not establish old fitted49,184. No guessed offset or UI fit replacement.

UI-TEXTBOX-COMPOSE-2026-11-02: common/widget_layout.c composes fresh normal
single-line textPos surfaces, alignment/font marker policy; full-height row
slices, not top/bottom cap subtraction. Actual460->489/488 and20 live startup
font metrics yield heights66/70 and fonts7/20 for an explicit parent. Text
color strings are stroke, fill none. Pre-mask textPos rejected pending complete
second-pass/cache support. Full strict host/focused sanitizers/ARM-qemu/native
TV CPU tests pass. Final native124016 bytes/MD5 d1e11762fe72fc0d2314731bd953e2ad/
GLIBC2.7 in build-tv-v3/textbox. Same stock boot71f7eb63-727f-4b9a-a9c7-
2b04dc3f9197/live1785/zombie1967/access/privacy; staging removed/checked,
no helper/job pending. No display/device/settings/persistent operation. Actual
parent/glyph-origin/ratio/clip and visible text integration remain open; fitted
UI coordinates and captured panels unchanged.

UI-ROW-WINDOWS-2026-11-02: ordered BinSVG group geometry now in
common/widget_layout.c; exact iterator rectangle accessor in common/binsvg.c.
Fresh firmware proves first mask raw/pre-mask identity/later cached transform
and normal rowItems/mRowM -> child-window composition. Actual460 tests derive
text1/labelLong/text2 for an explicit parent, not a live widget instance.
Full strict host/focused sanitizers/ARM-qemu/native TV CPU tests pass. Native
85492 bytes/MD5 7d40f2f7997459b0e683e82b5eb02a71/GLIBC2.4 in
build-tv-v3/row-windows. Same stock boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/
live1785/zombie1967/access/privacy; staging removed and checked. No display/
device/settings/persistent operation, no helper/job pending. Animation groups
explicitly unsupported; actual parent/textbox/text-origin/style integration
remains open and fitted UI origins unchanged.

UI-THEME-SKIN-2026-11-02: ui_vendor now holds/rebinds ui.theme and resolves
DialogLauncher animation markers from resource bytes. First numeric mode wins;
only copied mode survives selection/removal. Missing resource/markers means0,
missing optional provider retains stock4 fallback, ui.fs_type override retained.
ui.launch_anim remains default0/lab-only, not a vendor launcher transition.
Full strict host, focused sanitizers, ARM-qemu/native TV real-DSO policy/lifetime
tests pass, live resource selects60/type4. Native test479640 bytes/MD5
e876bcf71623a2ad6333402a7184827f; ui_vendor161632 bytes/MD5
72c2d2ba71d5751a8542aff979354ab9, both maxGLIBC2.17, build-tv-v3/theme.
Recorder display/injected progress: no visual/device acceptance claimed. Same
stock boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967/access/
privacy; all staging/fixtures removed and checked, no helper/job pending.
Actual row/textbox geometry, styles and visible integration remain open.

UI-THEME-PROVIDER-2026-11-02: common/svg_pack.c owns a fully validated immutable
index/binary snapshot; mod_ui_theme publishes ui.theme v1 (record/named/count).
Borrowed views require a service hold. Full strict host, focused ASan/UBSan,
ARM-qemu and native TV real-DSO lifecycle+702-record corpus checks pass. Source
now21 modules; preserved16-module stage/installed ABI1 unchanged. Native test
508228 bytes/MD5 9975f90fcce4483dd9b2187d034b549e/GLIBC2.17; provider
47116 bytes/33609e021a52fab6c3e9b61b0459d6d4/GLIBC2.4 in build-tv-v3/theme.
Same stock boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967/
access/privacy; staging and synthetic fixtures removed and independently checked.
No device/display/settings/persistent changes, no helper/job pending. Actual UI
service ownership/rebind and widget/text integration remain open; current panels
and text origins unchanged. Failed replacement still has no host binary rollback.

UI-BINSVG-VIEW-2026-11-02: common/binsvg.c adds bounded immutable record
validation/iteration and first-match named rectangle lookup, optionally scoped
to the first matching group (including descendants). All702 records/11888
elements/6733 rectangles/39 animations pass strict host/focused sanitizers/
ARM-qemu/native TV; full host suite passes. Actual477 slices now feed the
maintained geometry helpers from the pack, with an explicit test window.
Native75356 bytes/MD5 4ecee7b4a67391ff35dcc22ac47ed2cd/GLIBC2.7 in
build-tv-v3/binsvg. Live theme hashes match offline copies. Same stock boot
71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967/access/privacy;
staging removed and checked. No device/display/persistent changes, no helper/
job pending. Indexed loader/theme provider/actual widget integration remain
open; current UI still uses fitted origins/captured panels.

UI-HORIZONTAL-ORIGIN-2026-11-02: common/font_text.c adds aligned line layout/
drawing with physical width, center overhang trim/reset-X and end right-bearing/
reverse paint order, correcting the previous advance interpretation. Whole-line
validation; explicit plain EF41 unit-ratio scope, no word justification/shaping.
Full strict host, focused sanitizers/corpus, ARM/qemu and native TV synthetic/
30-line font7/16 offscreen checks pass. Native82476 bytes,
MD5 bd1384ee414f673ad2858477ddcc7d3c, GLIBC2.7, build-tv-v3/font-line.
Same stock boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967,
hook/hosts/access/privacy; staging removed and checked. No device/display/
persistent changes, no helper/job pending. Actual widget style/resource/clip/
vertical-origin integration remains open; existing UI origins still fitted.

UI-TEXT-ORIGIN-2026-11-02: concrete list/row/textbox margin bindings resolve
the zero-margin base getter. Vertical logical placement and phase-preserving
ratio-floor helpers implemented in common/text_origin.c. Strict full host,
focused sanitizers, ARM/qemu and native TV contracts pass. Native30248 bytes,
MD5 d8dcf038231ff43f355573d7798f7b7a, GLIBC2.4, build-tv-v3/text-origin.
Same stock boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967,
hook/hosts/access/privacy; staging removed and independently checked. No
display/device/persistent change, no helper/job pending. Horizontal/direction/
actual style/coordinate/runtime theme integration remains open; UI origins
remain fitted. See UI-TEXT-ORIGIN for exact entries and rejected assumptions.

UI-SLICE-STRETCH-2026-11-02: retained double-ratio horizontal stretch and middle
destination construction implemented; composed raw477→measured/painted row test
passes without screenshot fits.49,538 VFP rectangle cases pass ARM/qemu and
native TV, full strict host/focused sanitizers pass. Native46836 bytes,
MD5 d38cfe511c7b0b6f3aaa8ba31c57b92d, GLIBC2.4, build-tv-v3/slice-stretch.
No device/display/persistent changes. Same boot71f7eb63-727f-4b9a-a9c7-
2b04dc3f9197/live1785/zombie1967/access/privacy; staging removed and checked.
No helper/job pending. Runtime theme/client-window/text-origin integration
remains open; fitted UI coordinates unchanged. See UI-SLICE-STRETCH.

UI-LIST-METRICS-2026-11-02: raw slice measurement, painted-row cached metrics
and visible-count ordering implemented in common/text_layout.c. Raw475/477
slice heights give step70/top16/bottom10. Strict full host/focused sanitizers/
ARM-qemu and native TV list_metrics test pass. Native36008 bytes,
MD5 e888fe330468ff6b40e61b3e883d3c1d, GLIBC2.4, build-tv-v3/list-metrics.
Same stock boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967,
access/privacy, staging removed and checked. No device/display/persistent change.
Older widget-name expansion corrected at source; pointer chains retained.
Runtime transformed inputs/window step/text origin and UI integration remain
open; optional live label positions remain fitted. No helper/job pending.

UI-MASK-LAYOUT-2026-11-02: common/text_layout.c now also supplies explicit
mask positioning and vertical/horizontal/mirrored row arithmetic, recovered
from firmware. Strict host/full suite, focused sanitizers, ARM/qemu and TV
tests pass. Native26776 bytes/MD5 f45e0b8791697caa2cf09da628cb4d51,
GLIBC2.4 in build-tv-v3/mask-layout. No display/device operations. Same stock
boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967/access/privacy;
staging removed and independently checked. No helper/job pending. Actual
runtime metrics/slice composition/text origin and full widget integration are
open; current optional live label origins remain fitted. See UI-MASK-LAYOUT.

UI-TEXT-GEOMETRY-2026-11-02: common/text_layout.c implements the recovered
single-line textPos height adjustment with float32/truncation and bounded errors.
Foreground consumes+1ec textPos; +254 is addition. This corrects the newer
research's width/round-half-away/unknown-consumer claims. Strict host/sanitizer/
full suite and49,728 ARM VFP comparisons on qemu and TV pass. Native17620 bytes,
MD5 873adb0528dc642df3fab21e6be64cc9, GLIBC2.4, build-tv-v3/text-geometry.
Same boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967, hook/hosts/
access/privacy and safety markers; no device/framebuffer access. Staging removed
and independently checked. No pending helper/job. Goal resumed; actual row/mask
positioning, baseline and complete layout integration remain open.

UI-GLYPH-DRAW-2026-11-01: single-glyph RGBA compositor, clipping/signed bearings
and firmware coverage arithmetic implemented. All52022 shipped glyphs compared
to stored coverage offscreen; full host249 plus three font groups, focused
ASan/UBSan, ARM/qemu and TV pass. Native build-tv-v3/font-draw identities are in
the report; no device/framebuffer access. Same stock boot71f7eb63-727f-4b9a-
a9c7-2b04dc3f9197/live1785/zombie1967/access/privacy; staging removed and checked.
SVG marker mapping confirms focused488 is bold with a different text rectangle
from regular/default489; captured recoloring is not focus text parity. String
layout/font-data provider/UI text integration remain open. Owner requested
handoff and stop after this item; no helper/build/Ghidra job remains pending.

UI-FONT-LIVE-2026-11-01: common/font_pack.c reads the vendor index and one
record by ID (font7=847,332 bytes; font16 also loaded from the live /vendor2
pack; the 30 MB pack is not loaded whole). common/font_text.c renders one LTR
packed-UTF8 line with EF41 advances, explicit spacing, a clip and
common/font_draw coverage compositing; it validates the whole line before
drawing (missing/invalid draws nothing). Fits vs the corrected capture: row
labels font7 at (49|79,184+70*row) IoU0.9799..0.9840; text values font7
(Natuurlijk493,183; Uit553,533; Auto536,605; Aan545,673) IoU0.875..0.954; slider
digits font16 at (659,250+70*(row-1)) IoU0.907..0.921; disabled "Beeld zoomen"
font16 grey at (51,602). C measures font7 height61/advances118,143,302,168,48,
67,83 and font16 height68/digits49,51. ui_vendor ui.text=1 pre-renders 17
items, erases only the captured text strips (labels x40..486, text values
x480..680, digits x648..738) and blits live text; missing/invalid font falls
back to the captured panel. Title/tabs/slider graphics and focus overlays stay
captured; row8 uses the visible captured string. Host check-font-pack/
check-font-text/check-ui-settings-text (live+missing) pass; full make check 251
indented checks plus the font groups. Real fb1 (PID20201, ui_engine1952912c… v2,
ui_vendor e0e7a690…) vs the corrected panel: 24,545/2,073,600 any-channel
(1.184%), only22 px >8 (max11), RMSE0.2712/255; rows0-4/6 have no >8 diff
(row5 5, row7 7, row8 10). fb1 restored (16588800 bytes, offset0,1080), staging
removed. Next: title/tabs/slider graphics/focused labels (firmware geometry
only — analysis/ui-text-setup-2026-11-01 records the decoded state-indexed
resources and renderForeground line/font/alignment fields; transformed textPos
and per-widget setup are still undecoded, so no title/tab implementation from
fits), value/tab interaction and exact alignment/substitution/localization.
Evidence: artifacts/analysis/ui-font-live-2026-11-01/ and UI-FONT-LIVE-2026-11-01
report.

UI-FONT-SELECTION-2026-11-01: common/font_finder.c implements firmware-derived
stable typed maximum-height selection, signed height/line-count division and
global-smallest fallback. Startup20 IDs/heights verified from decoded theme.
Host full249 plus two font groups, focused sanitizers, ARM/qemu and native TV
tests pass. build-tv-v3/font-selection binaries37748/b4afdf02bc01759f17107257dd5c5204
and24240/c476f17d4f3d747e4ec4b429f55b16ff. Same stock boot/live1785/zombie1967,
access/privacy, all staging removed. No device/display operation. Widget text
style/layout/render integration remains open; see UI-FONT-SELECTION report.

UI-FONTS-2026-11-01: bounded EF41 grayscale bitmap reader and packed UTF8
lookup implemented; all38 live/offline font records pass host, ASan/UBSan,
ARM-qemu and TV checks. Full host suite exits0. UI text rendering/font selection
remains unimplemented; browser/FreeType native-row claim corrected. Native test
31640 bytes/MD5 1867d20017ecdc3e9c633afe9c7ba798, GLIBC<=2.7, isolated
build-tv-v3/font-bitmap. Same stock boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197,
live1785/zombie1967/access/privacy; no devices opened or drawing. Temporary
staging removed and independently checked. See UI-FONTS report/raw analysis.

Vendor UI is running on 192.168.178.87 / 70:54:b4:fe:70:c6. Following physical
USB recovery, fresh SSH key login and root telnet worked; Ethernet, routes,
hosts block, Samba OUTPUT REJECT and privacy/internet/WoL values were checked.
[Stability evidence](artifacts/reports/STABILITY-2026-10-31.md).

- Review's later view-only sample: boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197,
  uptime6min, stock Aurora PID1785 and zombie1967. The boot changed before our
  query; its cause was not established. Installed core/hook/hosts hashes match
  the values below; SSH22/telnet2323, Samba REJECT and safety markers remain.
  No restart or TV write was performed by the review. Raw review-tv-state.txt
  is in artifacts/analysis/cec-ownership-2026-11-01.
- Boot ID ebb0aaaa-72a2-4410-9f8d-95b9249f8a90 was the earlier CEC-session
  id. A later same-day view-only read did not recheck that id. SSH and ping
  answered, uptime was 14h48, and load was about 15 from the usual uninterruptible
  driver threads. No process was named aurora. dmesg showed `[COREDUMP] SIGNR:6`,
  then `kcec: SW RX data full`. `sdb` was attached. `run.sh` was in `do_wait`.
  Vendor partitions stayed mounted. Nothing was restarted. The earlier PID 1737
  claim is superseded.
- /conf/oa_safe and /conf/coredumpnoreset present; /conf/oa_boot absent.
  Native and web launcher boot are disabled. No native test/helper was left running.
- Installed core is ABI 1, MD5 a898384df076572cf377749f25427689.
- Hook MD5 0b1cd32a9de2b0251f6252e6603db8b7; keepalive
  4ebd0f9d7ee9f470afde8b0f2807522b. One keeper repairs SSH/telnet permissions/
  listeners, hosts/Samba protection, privacy/internet/WoL and Ethernet/DHCP.
- Privacy: internet=1, ads/personalised/marketing/third-party/ACR=0,
  DNT=1, logging=0, wake-on=1. Internet=0 disables networking on this firmware.
- SSH 22 is key authenticated; telnet 2323 is unauthenticated root fallback.
  Standby suspends services; historical quick-standby WoL works. Deep standby
  and guaranteed always-on remote access have not been proved.

The tested recovery image is 994da7ff76a0b2fbafb8f6fa0c682627, with script
de1591298d633cf6422a1ade4e8835a9 (584 bytes). Loose payload and
recovery-stability copies match. Raw mtd13 is older; identities are in
[artifacts/README.md](artifacts/README.md). Do not rebuild/replace these during
ordinary development.

Outage lesson: an stdin upload in a backgrounded AND-list likely truncated the
hook. Separate upload, size/hash verification and subsequent actions. The exact
older USB script worked; the earlier failed recovery attempts do not prove a
syntax defect.

## Development build and evidence

SKIN-SELECTION-2026-11-01: host socket/stdin `skin NAME`/`skin` and startup
ui.skin policy now select modules and fall back to vendor on missing/start/
enter/first-frame failure, after safe failed-candidate cleanup (including NULL-
returning failed init). ui.skin v1 status is separate; drawing/registration stays
ui.engine v2. Runtime override is not persisted. Host249/focused ASan/UBSan/
Unix socket/native contracts pass. Real GPU PID8795 switches Omarchy/vendor,
recovers missing skin, verifies alpha and restores full backing hash
41735daa69dc137743b59efe12db4a19/pan0,1080. Same boot/PID1785/protections/access,
staging removed. Native build-tv-v3/skin-selection, exact final-tested.md5 and
stage.md5 in analysis identify artifacts. Production16/installed ABI1 unchanged.
Real OmarchyTV/content/exact visual/cold parity and crash recovery remain open.

UI-ENGINE-REVIEW-2026-11-01 supersedes the split's v1 dispatch contract below.
ui.engine v2 uses skin-owned trampolines and a bounded event queue; legacy
register_skin fails ENOTSUP. Forced unload/rebind, both skins' re-registration,
async/alpha companion return, failed receipts/readiness, timer failures, assets
and identity reset are covered. Host242 checks/focused ASan/UBSan/native skin
contracts pass. Actual GPU PID4365 passes vendor-instant Select,11 rail revisions,
reload/module cycle/BGRA24,23,22,153/full restoration. Backing MD5 remains
41735daa69dc137743b59efe12db4a19, pan0,1080, same boot/PID1785/access/privacy.
Staging/backups/helpers removed. Native build-tv-v3/ui-engine-review and report
identify final binaries; production16/installed ABI1 unchanged. Initial retained-
transform timeout was fully restored and corrected. Full content/visual parity
and automatic fallback remain open.

UI-TRANSPARENCY-2026-11-01: firmware rectangle reader + ARM fill/opacity
conversion recover portal RGBA36,38,40,153. Both display backends now publish
display.alpha v1; canvas source-over and CPU identity premultiplication preserve
alpha, ui_vendor owns companion hold. Host227 checks/focused sanitizer pass; real
GPU readback BGRA24,23,22,153, dialog/reload/module cycle and complete restore
pass. Independent real-video appearance and exact animation sequence/cadence
remain open (owner requests explicit audit). Native build-tv-v3/ui-alpha:
scene_display59752bytes/5d6a1d411c37b1769b2098c348bcd9c6, UI90184/
42aa9ed02e8cdeceff9fee818569b4cd(max2.4), probe500996/
6acfb7f39389f2fd5f91958df1b554ab(max2.17). Same stock boot/PID1785,
full framebuffer41735daa69dc137743b59efe12db4a19/pan0,0/access/privacy; staging
removed. Installed assets/core and matching production16-module stage unchanged.

RAIL-ASSETS-2026-11-01: verified eleven vendor APNG frames exported to ignored
artifacts/workbench/rail-ui-20261101 with provenance/hashes. Full-bundle regression
exposed MAX_ICONS24 truncating the loop to nine frames; capacity now29 includes
the maximum12 cards. Focused strict/sanitizer and full host226 checks pass.
Real TV private bundle, GPU dialog completion, UI reload and module suspend/
resume pass; seven image revisions observed in nominal660ms do not establish
visible frame order/30ms cadence. Full backing hash41735daa69dc137743b59efe12db4a19,
pan0,0 and same boot/PID1785/access/privacy/protections verified afterward.
Staging/backups/readiness/helpers removed; installed assets unchanged. Native
build-tv-v3/rail-assets: UI89704bytes/7f0106c721fcb478266f4166127081cb(max2.4),
diagnostic501052bytes/1d6120b69608b09e554b2313ddc87904(max2.17).
Production16-module stage unchanged. RAIL-ASSETS report distinguishes the
initial pre-fix run/manifest from the final tested DSO.

UI-ASYNC-2026-11-01: ui_vendor consumes display.async receipts, acknowledges only
actual success, owns its sampler/holds and caches transform-only dialog progress.
Four strict host/sanitizer/TV groups, full host226 checks pass. Actual TV portal
with installed assets, Down/Select/Exit, completed dialog, UI reload and host-only
module suspend/resume pass. Full snapshot/region/MAU/pan restored; independent
hash41735daa69dc137743b59efe12db4a19/pan0,0 unchanged. Same boot/PID1785,
hook/hosts/access/privacy/safety markers. All staging/backups/readiness/helpers
removed. Installed assets lack rail_hl01..11; no rail animation acceptance here.
Native build-tv-v3/ui-async: UI DSO89728bytes/0e29cd402c4aa9cacf678aaa85ea8522
(maxGLIBC2.4), final unit463032bytes/ffca0080be7e9a324fe09197774859cb and
tested diagnostic499600bytes/6da1edfe2327da62cb38988c34091eca (max2.17).
The old3-group unit identity is historical; final-tested.md5 owns current unit.
Source18 modules, matching production16 unchanged. Remaining visual/asset/input,
full feature parity and cold/TV standby acceptance remain open.

SCENE-DISPLAY-2026-11-01: optional scene_display canvas/coalescer publishes
display.async receipts; legacy present rejects ENOTSUP. Four strict host/
sanitizer/TV groups, full host222 checks pass. Real101-request burst retains
the final identity receipt; full diagnostic observes412 GPU completions, host
drain/unload and full snapshot/pan/MAU/region restoration. Independent backing
hash41735daa69dc137743b59efe12db4a19/pan0,0 unchanged. Same boot/PID1785,
hook/hosts/access/privacy/safety markers; staging/backup/helpers removed. Native
build-tv-v3/scene-display: DSO53236bytes/9293d421420b1ad0d7b03e41f3a57f1a
(maxGLIBC2.4), unit469528bytes/7f6f98030dcd8f40631713f2e369435a and
diagnostic487196bytes/a62146c477759bfb3d3e3d94d2dd055e (max2.17). Source18
modules, matching production16 unchanged. Subsequent UI-ASYNC above integrates
ui_vendor/readiness; full visual/cold acceptance remains open.

GRAPHICS-MODULE-2026-11-01: mod_graphics/display.gpu v1 adds owned watch/timer,
nonblocking quiescence, public host-id tickets and suspend/resume/holds. Four
strict host/sanitizer/TV groups pass; full host218 checks. Actual service
completes410 GPU frames, then the real host reactor retries cleanup until DSO
unload and full framebuffer/region/MAU/pan restoration. Native isolated artifacts
under build-tv-v3/graphics-module: DSO69900bytes/2afefd7ffaedc6bff9e411561e05ddc0
(maxGLIBC2.15), lifecycle478980bytes/f3ff7b116074fe7cee3f7440cb2fc065 and
diagnostic483724bytes/3e091ecdf8ef747c6c949d70b24d6415 (max2.17). Same stock
boot/PID1785, full hash41735daa69dc137743b59efe12db4a19/pan0,0, hook/hosts,
access/privacy/safety markers. Live Mali hash matches offline source. Staging/
backup/FIFOs/helpers removed. Source now17 modules; production stage remains16,
not rebuilt/deployed. Next: GPU display consumer/coalescing/ui_vendor/readiness.
Abnormal child exit latches unproven cleanup; automatic native recovery is open.

SCENE-CLIENT-2026-11-01: lower nonblocking parent (submit/pump/take/shutdown,
WNOHANG reap) passes five strict host/sanitizer/TV groups and410 actual GPU
frames through the diagnostic adapter. Blocked finish/close retains the child;
shutdown never waits/signals, canceled results stay collectable. Full host214
checks pass. Isolated build-tv-v3/scene-client: unit90108bytes/
4068c3b34d51bb38e0b340fdeb3a2762 (maxGLIBC2.15), diagnostic91068bytes/
df6428c2f968b617121474b15fc933c7 (max2.17); helper/renderer hashes unchanged
from SCENE-HELPER. Child exit0, full snapshot/pan/MAU/region restored; independent
hash41735daa69dc137743b59efe12db4a19/pan0,0 unchanged. Same boot/PID1785,
hook/hosts hashes/access/privacy/safety markers. Staging/backup/FIFOs/helpers
removed. Production stage unchanged. Next: host service/watch/reap timer,
coalescing, completion-driven readiness and UI/module lifecycle integration.

SCENE-HELPER-2026-11-01: separate EGL-free bootstrap plus late-loaded renderer
passes two process fixture groups on strict host/sanitizers/TV and410 actual GPU
frame requests on the TV. Constructor fd survives inherited-fd cleanup; child
drains/reaps before full snapshot/pan/MAU/region restoration. Independent full
hash41735daa69dc137743b59efe12db4a19 and pan0,0 unchanged. Same boot/PID1785,
installed hook/hosts/access/privacy/safety markers; staging/backup/helpers removed.
Full host209 checks pass. Isolated binaries under build-tv-v3/scene-helper:
bootstrap57304bytes/596643ed63c76231e6a7e31fd22d7efd, renderer68064bytes/
f4196fa2413cbf60efdfb136d8b56e4b, diagnostic74588bytes/
d875eff769e4b421dd6e6d4d70bf14fb; maxGLIBC2.4/2.4/2.17 respectively. No
production stage refresh. Next is nonblocking parent provider/completion/readiness
and module lifecycle; the diagnostic client blocks and must stay out of the host.

SCENE-BROKER-2026-11-01 adds the lower immutable-image IPC and blocking owner
server. Four injected-renderer groups pass strict host, ASan/UBSan, ARM/qemu and
actual TV; complete host suite207 checks pass. Native unit87760bytes/
ba89e0afce422f03e8254ae1b0c307cc, maxGLIBC2.4,
preserved under build-tv-v3/scene-broker. Target mmap_region rejects read-only
MAP_SHARED on sealed files; instruction-backed MAP_PRIVATE correction passes
with all seals retained. No EGL/hardware calls in this test. Staging removed;
same boot/PID1785, full framebuffer hash41735daa69dc137743b59efe12db4a19,
pan0,0, installed hook/hosts hashes, access/privacy and safety markers confirmed.
Production stage unchanged. The later SCENE-HELPER closes executable integration;
host service remains open. Mali must load after inherited-fd sanitation. See report for the
initial timeout and emulator-only ancillary truncation discrepancy.

EGL-WINDOW-2026-11-01: direct GPU display diagnostic passed, owner independently
confirmed upright/full-screen/smooth effects. Eight types each51 presents in
about1000.4ms at50Hz pacing. Full16588800-byte snapshot, MAU0, region and pan0
restored; full backing MD5before/after41735daa69dc137743b59efe12db4a19. Stock
boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/PID1785 and protection/access hashes
remain; staging/backup/helpers removed. No production module integration or
stage refresh. Current native binaries under build-tv-v3/egl-window:
egl-window-probe88780bytes/5a66a935317e0623660151ffac96a926 and regression
egl-scene-probe74504bytes/aed778d42cc3aa4e3ff8ddc995b9767c, maxGLIBC2.17.
The regression also verifies cached textures, bounds, thread ownership and
pbuffer reopen. Next: nonblocking owning helper/service/completion/readiness
and module lifecycle integration. Synchronous EGL cannot run in quiesce.

SCENE-WARP-TV-2026-11-01: the display v2 software scene warp was exercised on
real fb1 with tools/display/scene-probe.c (ARM, staged /tmp, SHA-256 checked,
vendor Aurora left running). All eight types presented at p=0..1; the identity
endpoint readback matched BGRA (80,0,0,255) for every type; the probe restored
all 16,588,800 bytes with snapshot_match=1 and original yoffset 0. Per-present
timings: types 0/2/3/4/5/6 87-117 ms; type 1 keystone 483 ms; type 7 427 ms;
type 8 493 ms. Vendor PIDs 1967/1785 unchanged, staging removed, no helper left.
Readback is not scanout proof and these software timings are far below smooth
30/60 fps; the EGL path remains the parity target.

PANEL-OBSERVATION-2026-11-01: the owner watched the real-GPU UI on the panel for
two runs (~20 s). The rail highlight played once, but appeared to restart before
completing because the probe's 2000 ms rail window ends with a focus-changing
Down key that resets the sequence; the hold/observation window must extend past
the last frame before any key. The dialog transition visibly moved DOWN, not up:
we apply only the recovered full-screen type-4 shift (negative clip y) to the
whole scene, while the vendor's dialog has its own named-rect animation
(SVGDialog::drawAnimatedRectangles 0x10c5161 looks up the animation record's
rectangle in the dialog description and draws it). The full-screen surface and
the dialog therefore move separately; our single whole-scene shift sends the
dialog the wrong way. Next: recover the dialog's named animation rect/motion
(record name at +0xc, rectangle lookup at this+0xdc) and draw the dialog upward
over the type-4 background shift, then re-observe on the panel.

DIALOG-MOTION-AUDIT-2026-11-01 (partial): DialogLauncher.svg (resource 129,
span 57345..57634) contains the element `animation_slide_up_fade_in` followed by
four signed-16 values -109, 1096, 100, 100. SVGDialog::drawAnimatedRectangles
(0x10c5161) iterates the AnimationContainer at this+0xe8, reads the record name
at +0xc, builds a UString, calls an unnamed helper at 0xc86b4c and then 0xc8724c
with operands (1, len-1) before RectangleContainer::getRectangle at this+0xdc,
and draws the resulting rect through the facade. The exact meaning of the
substring step (the helper is not exported) and whether the four values are a
from/to pair or a rect is not resolved. Do not implement a dialog offset until
those are decoded; the whole-scene type-4 shift remains the only implemented
motion and is the observed wrong-direction defect.

DIALOG-HELPERS-2026-11-01: the two helpers are identified by their call-site
labels: 0xc86b4c = UString::length() and 0xc8724c = UString::getSubstring(uint,
uint). drawAnimatedRectangles calls getSubstring(name, 1, len-1), i.e. drops the
record name's first character before RectangleContainer::getRectangle. If the
record name carries a leading type marker, the looked-up name is exactly the
pack element "animation_slide_up_fade_in" (values -109,1096,100,100); that is
consistent with the name existing in DialogLauncher/MainMenu3/4/Tall, but the
record-name prefix and the four values' semantics (rect vs from/to) are still
unproven. Consequence: no dialog offset has been implemented; verify the
hypothesis against the AnimationContainer record construction or a live A/B
before changing the motion.

DIALOG-DIRECTION-2026-11-01: the firmware pipeline does not invert the type-4
motion. The vendor vertex shader is exactly `gl_Position = in_Position;
ex_Texture = in_Texture;` (source at 0x13f8b28..0x13f8bbc), the fragment shader
only multiplies alpha, and onRenderCompleted draws the same four-vertex quad with
UVs (0,0)(1,0)(1,1)(0,1). Our scene_gles applies the same oa_fs_apply type-4
rule (y -= (1-p)/6) and only inverts the texture v to compensate for top-down CPU
rows; that does not change the motion direction. With clip +y at the top (the UI
renders upright), p=0 is 1/6 lower and settles upward. The owner still reports
the dialog moving down, so the remaining difference is in what content is
transformed or when it is presented (single-surface screen swap / async frame
coalescing), not the recovered arithmetic. Resolve with frame-level panel
evidence (camera capture or a step-hold) before changing the transform sign;
do not invert the recovered vendor rule.

DIALOG-DIRECTION-MEASURED-2026-11-01: the GPU window path was measured on real
fb1 at held endpoints with a new probe diagnostic (first non-black row):
identity top=0, type-4 p=0.5 top=45, p=1 top=0. That is exactly the recovered
rule (dy = -(1-0.5)/6 clip = 1080/24 = 45 px): the surface starts lower and
slides up, and p=0 is fully transparent because the shader multiplies alpha by
progress, so an endpoint readback at 0 shows black. The transform arithmetic and
the GPU orientation are therefore correct; the owner's panel report of a
downward dialog is not the transform. Remaining suspects: the single-surface
screen swap at completion, or the owner observing the close/instant switch.
Resolve with a UI step-hold that pauses the dialog content at p=0.5 and p=1 for
direct viewing; do not invert the rule.

DIALOG-MOTION-FINAL-2026-11-01: firmware settles the question. The only caller of
AnimationContainer::addAnimation is AnimationHandler::handleAnimationElement
(0x00ef8fb8, call at 0x00ef90f2), and DialogLauncher/MainMenu resources contain
zero animation elements (named-animations.tsv has no rows for 129/136/139). There
is therefore no separate dialog named-rect motion to recover: the entire
transition is the full-screen type-4 rule (dy = -(1-p)/6 clip = 45 px at p=0.5
for 1080), the recovered 1000 ms controller, the cubic min(1,1+(p-1)^3) easing
and the alpha = progress fade. Our implementation applies exactly that to the
dialog content, and the real-fb1 measurement (identity=0, p0.5=45, p1=0) matches.
Do not add a guessed dialog offset or invert the rule; the residual panel
appearance is a presentation/hold question, not firmware timing.

SELECT-PATH-2026-11-01: firmware shows the launcher has no Select transition.
MainLauncherMenuDialog::onSelectPress (0xe48405) only calls
focusLauncher(false), and focusLauncher (0xe4831c) does index/logging/menu-active
bookkeeping with no animation controller or SVGDialog start. Our previous
type-4 dialog animation on Select was an invention and caused the visible
wrong-direction artifact. mod_ui_vendor now switches instantly by default; the
type-4 path is lab-only behind ui.launch_anim=1 (ui_nav and ui_async enable it to
keep animation coverage). Host make check 227 ok.

SUBL AUNCHER-ITEM-2026-11-01: sublauncheritemFA.svg (resource 552, span
232881..233354) recovers the vendor sub-launcher item: background (-2,0,1920,1080);
gRowM/mRowM (121,251,240,140); textPos (79,374,323,39); glyph (121,253,240,138);
scalerect (121,251,240,140). Its animation records are 300 ms linear:
#scalerect scale 1,1 -> 1.35,1.35; #glyph y 0 -> -24; #glyph x 0 -> -42 (the
-fvc variant 556 has the same three records). This is the next screen's tile
geometry and focus animation. Do not implement the card focus animation yet:
the scalerect's transform anchor/origin and the DialogSubLauncher (189) item
grid positions are not decoded, and our current card row is an invented layout.
Next: decode DialogSubLauncher 189 (item positions/count) and the animation
element's transform origin, then build the sub-launcher screen against them.

SUBL AUNCHER-LAYOUT-2026-11-01: the list/grid placement math is recovered.
FullScreenGrid::getFocusedButtonPosition (0xd206d0) tail-calls
SVGGrid::getButtonPosition (0x10ca8ac), which computes:
  x = column * this[+0x1e8] + this[+0x1fc]
  y = row    * this[+0x1e4] + this[+0x1f4]
  rect = (x, y, x + this[+0x200], y + this[+0x1ec])
The SVGGrid constructor (0x10cc605) zeroes +0x1d8..+0x200, so the six layout
members are filled later (create 0x10cacb9 / setWindowRect 0x10cacf1 or the
description lookup). Next decode where those stores happen and with which named
description rects, plus the #scalerect transform origin; then the sub-launcher
screen can be built without guessing.

SUBL AUNCHER-LAYOUT-2-2026-11-01: the six SVGGrid members are not set by
create/setWindowRect directly. setWindowRect (0x10cacf0) calls 0x10cabc8, which
calls SVGGrid::renderBackground (0x10c9ed0) among three others; renderBackground
computes the layout from the grid's area and margins using a division helper
(0x1285478) and reads/writes +0x1d8..+0x200: e.g. at 0x10ca194..0x10ca1dc it
derives +0x1dc/+0x1e8 from the area span minus +0x1f4/+0x1f8 and +0x1fc/+0x200,
and +0x1ec/+0x1fc from the other axis. The item-count members +0x1e4/+0x1e0 gate
the computation. Still undecoded: which description rects supply the margins and
the exact division constants, and the #scalerect transform origin. No screen code
was written; the next decode is renderBackground's inputs.

RAIL-CADENCE-TV-2026-11-01: real GPU UI run on the set with the preserved rail
stage refreshed to the current modules, our 10 ms-poll ui_vendor and the probe's
UI/rail sampling. Completion-paced advance is now implemented: the rail tick
waits for the async present serial to complete before advancing, so no frame is
coalesced away. Result: 11 image revisions over the 2000 ms measurement window
(the earlier 660 ms window under-counted because the full-frame async path
completes ~100 ms/frame), backdrop raw BGRA 24,23,22,153 matching the expected
premultiplied vendor colour, dialog type-4 completion, reload and host
suspend/resume passed, and the 16,588,800-byte snapshot restored
(snapshot_match=1, region_match=1); vendor PIDs 1967/1785 unchanged.
A first run with the stage's older modules showed BGRA 13,14,14,255: the
predates-alpha scene_display made ui_vendor fall back to the opaque clear. Reusing a
preserved stage requires refreshing every module, not just ui_vendor. Frame order and
scanout remain unproven.

EGL-SCENE-2026-11-01 adds common/scene_gles.c and opt-in context/scene probes.
The actual TV Mali-G51 passed offscreen pixel/orientation/fade/triangle and
close/reopen checks; no native window or fbdev writes. Scene helper70384bytes/
98e60a726b5169bf77614fd504ca17f1 (maxGLIBC2.17), context helper23032bytes/
b0f82f027ad28d251a5ac4ea95ff7609. The context helper is preserved there; the
70384byte scene helper is historical/source8a535b6 and that build path was rebuilt.
Five1080p uploads/renders/readbacks take301..374ms. EGL-WINDOW subsequently
validates visible native presentation; production service integration remains open.
Final TV read retains the same boot/PID1785/access/privacy/hashes and pan0,0;
sampled16580608-byte framebuffer prefix hash unchanged (last8192 backing bytes
not sampled). Staging/helpers removed. Target Mali source MD5d1aa5c1679a9467ec51865d4f2120112
matches TV. New Ghidra project is partially analyzed; see report/tool limits.

UI-SCENE-REVIEW-2026-11-01 corrects fan-triangle UV interpolation, GL_LINEAR
sampling and progress-alpha fade/premultiplied BGRA from fresh firmware reads.
Dialog progress now requests frames; provider aliases/interruption/failed-resume
cleanup are covered. Full strict host203 ok, focused ASan/UBSan and ARM/qemu
display regressions pass. Real-fb1 tests twice drew all types and restored
16588800 bytes and original yoffset0 (snapshot_match1), without stopping stock
Aurora or running HAL init. Final probe101324bytes, MD547dcd8ca304e67f42eed48c3a3172b33,
maxGLIBC2.17, under build-tv-v3/review-scene; full production stage unchanged.
Final live reads retain boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197, PID1785,
installed hashes, listeners and privacy/safety markers. Staging/helper removed.
Five-present default y-slide batch improved4499.56->399.68ms; general transforms
remain slow. This does not close smooth animation, independent scanout/startup,
cold/resume or visual frame-sequence acceptance. See the report for exact scope.

UI-ANIM-SLIDE-2026-11-01: text-slide math implemented from instructions, not
inferred. `AnimationHelper::calculateSlidingDuration` 0xef0210 is
`1600 + trunc(distance/50)*1000` and `calculateSlidingMargin` 0xef0264 returns
0 through the 800 ms pause, then `(int)(distance*frac)` with the clamp only
above — forward ends at distance, reverse at zero; a short slide (distance < speed)
has a zero span and is immediate after the pause. Constants re-read from the
binary: pause 800.0f at 0xeefc8c, divisor 0x32 in the constructor 0xeefc54,
multiplier 1000.0f at 0xef0260. Added `oa_anim_slide_duration`/
`oa_anim_slide_margin` (common/ui_anim.c) and published them through `ui.anim`
v2. Exact vectors in tests/ui_anim_review.c pass on host (make check exit 0,
195 ok), ASan/UBSan and ARM/qemu against the TV libc. No TV access, no drawing,
no deployment; installed ABI 1, recovery images and hook/keeper hashes unchanged.
The report's "returns to zero" sentence was corrected at source and in FINDINGS.
Caller wiring is now in mod_ui_vendor as well: `SVGTextBox::isTextSlidingNeeded`
@0x10c76b0 sets the distance to text_width − box_width, the framework samples at
10 ms, and `SVGTextBox::onAnimationProgress` @0x10c8e48 arms a single 2000 ms
timer (progress > 0.99, constant at 0x10c8ee8) instead of a repeating one. The
module starts a pass when the loaded title exceeds `ui.slide_box` (default off),
offsets the title blit by 29 − margin, and re-arms after the 2000 ms rest.
tests/ui_slide drives two full passes through the real DSO with a settable
ui.clock and observes the clipped source column; it also covers the repeat armed
at progress 0.99, animation disabled, suspend/resume and stop/start. This is
caller behaviour verified offline, not a live on-TV visual observation.

REVIEW-FIXES-2026-11-01 (GPT-6.1 Sol review): fixed before accepting the
milestone. The slide now uses `oa_ui_time_elapsed` (90,000,000 ms period) instead
of raw subtraction; start/repeat are gated on `ui.anim` enablement; the title is
clipped to `ui.slide_box` via a reusable crop buffer (the old code blitted the
whole label); the repeat is armed at progress > 0.99 while the sampler still
runs; the sampler/repeat are deleted on suspend/stop and the slide state is
cleared (the old code could dereference a released display or suppress the next
repeat); allocation failures are logged; `oa_anim_slide_margin` clamps the
float-to-int cast; the wobble uses the double precision π from 0xb32aa8; and
`mod_ui_vendor`/`mod_ui_anim` link `-lm` (verified DT_NEEDED) because `sinf` was
undefined. The module now calls the v2 provider for duration/margin instead of
carrying a copy. Host, ASan/UBSan and ARM/qemu rerun after these fixes.
Initial limits, subsequently fixed by PROVIDER-REBIND and UI-SCENE-REVIEW:
late ui.anim/ui.clock discovery and stale pointers after forced provider reload.
Ordinary reload while held remains blocked by the host.

LIVE-SLIDE-PROBE-2026-11-01: the slide was exercised on the TV with real timers
and no display hardware: tests/mod_slide_probe.c published a recording display
plus a CLOCK_MONOTONIC-backed ui.clock, the real mod_ui_anim/mod_ui_vendor ran from
/tmp with ui.anim_enable=1 (default-off bootstrap lever), size/SHA-256 checked
and staging removed afterwards. 914 samples at a mean 10.08 ms cadence; first
nonzero margin at +822 ms, final 163 at +3808 ms, repeat restart at +6557 ms
(= 4554 + 2000, the 0.99 threshold), clean exit. Vendor PIDs 1967/1785 unchanged
and no helper left. This is not visual scanout evidence; fb1, keys, settings and
vendor modules were not touched.

RAIL-HIGHLIGHT-2026-11-01: `MainLauncherMenuDialog::init` calls
`SVGButton::setMainlauncherHighlightId` five times (0xe4981a/0xe498ec/0xe499e8/
0xe49ac6/0xe49b98), once per category. The setter stores the resource id at
+0x96c (0x10aeee5); `SVGButton::renderForeground` (0x10af894) creates an
SVGImage from it at `RectangleContainer::getRectangle("mainlauncher_highlight")`.
Resource 579 (myMenuLauncheritemFA.svg) contains that named rect, so the rail
buttons use the category resources, not launcheritemFA/FI (536/537). Resource
579's local signed-16 rects: glyph (119,240,150,150), mainlauncher_highlight
(119,212,199,210), background (-2,0,1920,1080). The highlight resource
`mainlauncher_item_highlight_animated.png` is an 11-frame, 30 ms APNG of an
orange bar at the image's left edge growing to full height; all 11 fcTL entries
use dispose_op=0 and blend_op=0 (full-canvas SOURCE replacement), so no frame
compositing is required. The APNG num_plays
field is metadata: PngImageDecoderImpl::decodeNextFrame (0xb30654) increments the
frame index and only decodes while index < count, with no rearm at the end, so
the last frame is held. The 0.3 s 536/537 records are launcher tiles, not the
rail.

Placement recovered: init creates five SVGButtons named HOME/VIVA/SETTINGS/
APPLICATIONS/SOURCES; updateCategoryButtonRects looks up description names
`button1`..`buttonN`; setMenuItemPositions walks the rect vector cyclically with
the type array at +0x39b4; setMenuItemPosition calls SVGButton::setWindowRect
(vtable slot 0x30 = 0x10ade19). DialogMainMenu.svg (136) defines button windows
(6,211/330/460/600/719,160,160). The capture shows the focused 150x150 tile at
(11,450) and the bar at x 6..10, y 458..591 (mid-growth), so the 210x210 image
is drawn at (focused_x-5, focused_y+10). Implemented in mod_ui_vendor: rail_hl01..11
OAI1 frames from the vendor APNG, a 10 ms decoder poll that advances a frame once
now-last >= the 30 ms fcTL delay (numerator used as ms; last stamped at decode)
while the portal shows (the last frame is held, no wrap), playhead reset on
focus change, scaled into the button window. tests/ui_rail_hl asserts frame
order, the poll/delay gate, the one-shot stop and the window; host make check
227 ok, focused ASan/UBSan pass.
Live probe on the set (fake display, no fb1/keys): 166 blits, mean gap 29.98 ms,
window x=6 y=461, clean exit; staging removed, vendor PIDs unchanged. Visible
panel appearance is still unverified.

PROVIDER-REBIND-2026-11-01: mod_ui_vendor now reacts to `module/changed` and
`service/removed`. Events only schedule a 0 ms rebind timer (event dispatch runs
with the subscriber as cur_module, so holds and timers belong to ui_vendor);
`service/removed` nulls the cached display/ui.anim/ui.clock pointers before they
dangle. The rebind reacquires missing services, restarts or holds the slide as
enablement allows, and ignores ui_vendor's own lifecycle events. Suspend defers
rebinding: events mark it pending and resume performs it, so no display hold is
taken while suspended. tests/ui_rebind covers late ui.clock/ui.anim publication,
the disabled gate, forced unload of ui_anim, and provider return. Host make
check 197 ok and ASan/UBSan 197 ok; the ARM/qemu rebind run passes; TV rerun
pending.

FULLSCREEN-ANIM-2026-11-01: the full-screen progress path is recovered.
`SVGDialog::startAnimation` @0x10c5009 returns for mode 0x38 and otherwise maps
modes 0x39..0x40 through the byte table at 0x153b80c (01..08) to HAL types 1..8,
starting a 1000 ms controller (0x447a0000 at 0x10c5094) when the HAL supports the
animation, an app exists and `UIApp::modalTargetsDepth() == 0`.
`SVGDialog::onAnimationProgress` @0x10c4dac feeds `min(1,1+(p-1)^3)` to
`UIFramework::setFullScreenAnimationProgress`, and `SVGDialog::destroy`
@0x10c4e10 forces 1.0 when the target is still below 1. The HAL
(`hal::OpenGLGraphicsContext`, type +0x78, progress +0x74, dirty +0xa4) clamps
and rewrites vertex positions in `iterateFullScreenAnimation` @0xb32910 with the
π/2-3/1-3/1-6 pool at 0xb32aa8. Correction 2026-11-01 from the instructions:
the surface is one 4-vertex clip-space quad built in `onRenderCompleted`
@0xb33075 (96 bytes, `GL_TRIANGLE_FAN`, positions (-1,-1)..(1,1), UV 0..1);
types 2/3 add to the x field and types 4/5 to the y field at 0xb329b6 (the
pointer+4 is bytes with a 0x18 stride), not a second vertex group. Type 1 scales
only the two right vertices, a keystone. Encoded as `oa_fs_apply` and the
scene-level `oa_fs_scene_transform` (clip-space affine, keystone flagged) with
cross-check vectors. The display now publishes v2 with
`set_scene_transform(type, progress)`: all draws go to an offscreen RGBA scene and
present() maps it through the recovered quad (corrected to triangle interpolation,
linear sampling and alpha fade by UI-SCENE-REVIEW, BGRA output);
fake-fb tests cover x-slide, scale, keystone and clearing. mod_ui_vendor is the
consumer: on dialog progress it pushes the selected type with the eased value and
clears on completion/suspend/stop. `SVGWindow::getWidgetStandardName` (0xeee798)
names the modes: 57 flip, 58 slide-right, 59 slide-left, 60 slide-up, 61
slide-down, 62 fade, 63 grow, 64 bounce; `SVGDialog::create` tries 57..64 in
order and starts the first widget present. DialogLauncher.svg (129) declares
only animation_slide_up_fade_in, so mod_ui_vendor selects type 4 by that rule;
ui.fs_type remains an explicit override. Evidence: host 200 ok, ASan/UBSan 200
ok; check-ui-nav asserts the type-4 selection. Subsequent UI-SCENE-REVIEW supplies
ARM/qemu and existing-layer real-fb1 execution. Smooth/visual independent
acceptance remains open.

RAIL-TIMING-2026-11-01: MainLauncherMenuDialog moves rail items synchronously
(setMenuItemPositions from scrollItems/updateButtonPositions/init/onExit/
preHandleKeyPressMessage; no controller call). The focus animation is per
SVGButton: drawButton starts the controller only when the window's animation is
enabled and getAnimationDuration() > 1.0 ms; getAnimationDuration
(atof(record+28) x 1000.0 double at 0x10af1f0, max over records, default 0)
supplies it. Generic launcher tiles launcheritemFA/FI (536/537) are 0.3 s
linear scale 1->1.32 with glyph x->-34, y->-37; service buttons use 1.35.
Category glyph resources 579..598 have no animation records, and whether the
rail buttons are built from 536/537 is unproven — do not copy the 300 ms onto
the rail. mainlauncher_item_highlight_animated.png is an 11-frame APNG at
30 ms/frame (330 ms); the sublauncher/launcher highlight files hold one frame.

SPEAKER-GAIN-2026-11-01 is the latest review: direct SetOPUGain operands are
0..0x8000 and bypass UI AQ/balance/ratio processing. GetOPUGain's cached100 to
0x8000 mapping makes it unsuitable for exact rollback. Lower codec/client
dispatch passes three wire and six session groups host/sanitizers/ARM-qemu/TV,
using injected I/O only; no actual gain/table/mute write occurred. Native units
37668bytes/a90d0a2c4635af1210a9ecb1467d9bf2 and
67756bytes/bcd8ad1eff1f78aaa0dbb97543e7dc2c are preserved in
speaker-wire-tested-tv.tar under analysis/cec-ownership-2026-11-01. Staging
removed; boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197, live Aurora1785,
installed hashes, listeners and privacy protections unchanged. Broker/service
remain query-only. Only ARM unit tests were rebuilt in build-tv-v3/tests;
the production16-module set below retains its previously tested identities.

MIXER-MODULE-2026-11-01: mod_audio_mixer publishes
audio.mixer v1 and the core exposes mixer-query/result/cancel. Nonblocking IPC,
owned watch/reap timer, WNOHANG teardown, cancel/drain, held-provider protection,
resume failure cleanup and stale-ticket rejection are implemented. Five DSO
groups pass host/sanitizer/ARM-qemu/TV. Real TV socket query/reload/query returned
device0 L/R32768 twice; standalone core config/start/status/quit also passed.
No HAL init, setters, drawing or stock-UI restart. Both stages were removed;
boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197, Aurora1785, installed hashes,
listeners, safety markers and Samba REJECT remain. Rooted installed core ABI1
MD5a898384df076572cf377749f25427689 stays unchanged.

Current matching build-tv-v3: core08f549d4026cfb626c1ffc93b2e02fc6, all16
modules and helper/supervisor. All16 modules passed real-TV --check from a
verified temporary stage, removed afterward. stage.md5 identifies this complete
set. Previous ABI3 corefc9.../13-module set (including old test fixtures) is
archived at artifacts/archive/build-tv-v3-before-mixer-20261101.tar; SHA256 and
all old stage.md5 entries verified. ABI1/ABI2 and recovery files preserved.
Current exact tested mixer stage/tar and matching manifest are in analysis.
Target libc spawn's return0 followed by exec-failure exit127 is firmware behavior,
not a qemu defect; the adapter handles both immediate and asynchronous failure.

AUDIO-BROKER-2026-11-01: oa-audio-broker owns its
native PID-bound query socket in a separate executable, with strict32-byte local
seqpacket IPC and no HAL initialization/setters. Real posix_spawn execution on
TV returned device0 L/R32768, then closed/reaped cleanly; unrelated inherited
fds were absent. Parent85640bytes MD5bf227ea86b93b3a8c0c0055250fe0348;
helper62176bytes MD52e2856c667df2a40d2cbb38589a6753d, preserved in analysis.
Full host suite, focused sanitizer and ARM/qemu fixtures pass. Helper needs at
most GLIBC2.4; parent2.15. Fresh read-only firmware instructions confirm the
plat prelude's mixer initializer calls the PID-binding initializer. Parent
loop/module/CLI integration was subsequently added in MIXER-MODULE. No gain/table/profile/mute write.
Staging /tmp/oa-audio-broker-20261101 removed; boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197,
Aurora1785, installed core/hook/hosts hashes, listeners and Samba block unchanged.

CEC-ASYNC-2026-11-01: the lower CEC owner now has
nonblocking ticketed acquire/send/receive/release and copied take() results.
Four injected-I/O groups pass host/sanitizer/ARM-qemu/TV, including blocked open/
receive, copied path/tables/frames and failed-close generations retaining fd.
Test86172bytes/MD5d2ca064a491065637b78f9349c56e2b0 is preserved in analysis;
/tmp/oa-cec-async-20261101 removed. Same boot/PID1785, installed hashes,
listeners and Samba block remain. No live device, STBC or bus operation occurred.
Full host suite passes. Service adapters/native ownership/cancellation remain
open; both lower owners now support nonblocking submissions and value results.

AUDIO-ASYNC-2026-11-01: the lower audio owner accepts
nonblocking ticketed open/gain/close and returns copied results with take().
Three injected-I/O groups pass host, ASan/UBSan, ARM/qemu and TV. Full host suite
passes; no real socket, amp or CEC access occurs in those tests. Verified native
test75264bytes/MD571a53754d8c48fe97cfb65580c0f8d2c is preserved in analysis;
/tmp/oa-audio-async-20261101 removed. Boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197
and Aurora1785 were unchanged, as were core/hook/hosts hashes, listeners and
Samba protection. The module bind service still rejects all arguments: native
ownership and async service/lifetime integration remain open. CEC's synchronous
receive now treats positive1 as success instead of propagating stale worker errno.

New work uses ABI 3 in build-tv-v3, not the installed ABI 1/recovery build.
Core and all modules must move together at a future authorized deployment.
Historical build-tv-v2 remains preserved. ABI 3 adds a nonblocking quiesce gate
that can refuse destructive cleanup, including force and partial-start failures.
Six device-free DSO groups passed host, ASan/UBSan, ARM/qemu and actual TV.
The earlier core 8b06fd51c9ff662f13815ef6406dc184 plus 13 modules passed
--check on the TV, exit 0; temporary /tmp/oa-quiesce-20261101 was removed.
The archived previous build-tv-v3 core fc9eb71f924dd850c7ba4f1c9b8a02cd, max GLIBC2.17,
adds shutdown retry and was not checked on the TV. Its archived stage.md5 identifies
that historical set; modules were not rebuilt for that core change.
No production module was started in that earlier preflight. Stock boot/PID, installed core/hook/hosts
hashes, listeners and Samba block were reconfirmed unchanged. Host shutdown
now retries a quiescence veto offline (SHUTDOWN-2026-11-01). Offline CEC and
audio workers pass host, ASan/UBSan, ARM/qemu and the full host suite
(OWNER-WORKER-2026-11-01). mod_cec can suspend and resume its idle worker, and
mod_audio creates the mixer owner without binding a socket. Enabled HAL suspend
still fails closed. They were not in that previous13-module stage, not deployed,
and do not open live hardware. This core is not deployed.

Subsequent WORK-REVIEW-2026-11-01 fixes are ahead of that preserved stage set.
That review's source/host build had15 modules, including ui_anim and cec; the
that milestone's matching16-module build added audio_mixer (source is now21;
see UI-THEME-PROVIDER above). Review
fixes worker state ownership, join failure retention, fully nonblocking quiesce
and pending reload retry. The loop-thread bind_session service rejects every
argument with ENOTSUP until an async interface exists. Animation timer ownership,
resume, reentrant completion and extreme-coordinate conversion were fixed.
Full host make check and focused sanitizer/ARM-qemu checks pass. Review build
directories are build-review-san and build-review-arm. No review binary was
uploaded to the TV; no production module/hardware acceptance is implied.

| implemented milestone | evidence / limit |
|---|---|
| owned worker posts, descriptor-size checks, registry fixes | host, sanitizer and ARM/qemu tests; MODULE-SAFETY report |
| state-aware reload / restore failure handling | nine host/sanitizer/ARM-qemu regression groups; suspended activation deferred; no binary rollback |
| dependency-ordered lifecycle | nine host/sanitizer/ARM-qemu groups; service snapshots survive reload; CLI propagates failures; native device ownership remains open |
| sleep-cycle engine / power service+CLI | nine engine plus seven wrapped-power host/sanitizer/ARM-qemu groups; final entry/restoration and pinned notifications implemented; hardware acceptance remains open |
| bounded control replies / Unix transport | status heap overflow fixed; full-table/tiny-buffer checks plus five backpressure/partial-write/disconnect/input/cleanup groups pass host/sanitizer/ARM-qemu; stdout console remains synchronous |
| --check loader/ABI preflight | staged core plus 11 modules passed on TV, exit 0; staging removed; RUNTIME-CHECK report |
| present always commits FBIOPAN_DISPLAY | fake-device host/sanitizer/ARM tests; not deployed or tested drawing |
| maintained six-/four-word PWM declarations | decompile + return instructions; brightness still unverified independently |
| O_RDONLY fbregion diagnostic | four GET requests tested; staged fb1 query removed after use |
| oa-supervise/heartbeat/fallback | host and ARM fake-hardware smoke tests; real native takeover unverified |
| standby firmware recheck | outer/idtv/wake-network stages traced; monitor helper is task state and UART send helpers are semaphores; POWER-STANDBY-CONTRACT; no new hardware behavior |
| ui_vendor assets/rendering | historical framebuffer comparison; not full navigation or independent display parity |
| animation firmware/extractor | UI-ANIMATIONS: 39 named descriptors; all 702 resource IDs/names CRC-verified; list/dialog controller is mod_ui_anim offline; text slide wired in mod_ui_vendor; full-screen type/progress mapping encoded in oa_fs_apply; rail timing and renderer composite remain open |
| ui_clock / ticker paired hooks | FROST_Time arithmetic and reloadable UI time implemented; seven host/sanitizer/ARM-qemu DSO groups; absolute vendor clock phase unverified; ui.anim samples this clock |
| CEC/audio foundations | spec-based frame/status/ARC and identity/name/power validators; audio scalar ABI, prelude ownership, failure/cache/handoff fixes; six wire plus five fake-HAL groups pass host/sanitizer/ARM-qemu; transport/policy integration open |
| CEC packet/transport RE | raw/VPL ABI and 16-byte storage tested in four host/sanitizer/ARM-qemu groups; VPL status/filter/close/reopen hazards recovered; live open remains open |
| native raw CEC backend | PreInit/real cdev registration recovered; explicit blocking open/send/RX/close, failed-close retention and fd-zero/reopen logic pass three injected-I/O host/sanitizer/ARM-qemu groups; hardware acceptance open |
| offline CEC/audio owners | injected authority; mod_cec idle suspend/resume and mod_audio owner pass host, ASan/UBSan and ARM/qemu twice plus the full host suite; hardware_open_supported is 0; enabled HAL suspend returns failure; no live open; OWNER-WORKER |
| System Audio/ARC route | cec.audio v1 on mod_cec; injected 1.3a System Audio and 1.4 ARC; local volume/mute refused while the mode is on; 8-byte reload snapshot; host make check plus one ASan/UBSan and one ARM/qemu run; no live bus or amp write; OWNER-WORKER |
| portal navigation | mod_ir input.feed, mod_ui_vendor hit/open/home/back/source and mod_ui_anim list/dialog; 4-byte reload snapshot; host make check plus one ASan/UBSan and one ARM/qemu run; no ttyS2 or fbdev; live UART code byte unconfirmed; UI-PARITY |

The previously TV-tested preflight core MD5 was 424ef6ea213cb706acb51a7adb39f778.
The preserved historical offline ABI 2 build in build-tv-v2 has 13 modules, including ui_clock
and audio_curve. Core MD5 54a626f4c3a00b2e90490bc2626fe055;
ui_clock d9fa142605ef2a8fa75fa7f0fb1db7db;
ticker 9f1fdadf6f9ca721ed0a97f2d5da98eb. Core max GLIBC2.17.
Latest audio 4bb669961f0379575d98a5e2fcbb01eb and plat
b66b3a1eeff42ef77be782ad06674249 include the prior contract fixes and were
rebuilt after the service header addition. audio_curve a644c66e92bd6d53ef1032a990f8c4cc.
That ABI 2 full13-module set has build/host validation, not an on-TV preflight or
deployment; the separate curve/ticker real-TV runtime test is described below. Older
installed audio/panel/source have standalone unresolved V4L2 imports; the rebuilt
matching closure passes. --check proves loadability, not initialization/hardware.

Last fb1 read: MAU0 primary, PA0x35bbb000, size16588800, stride7680;
1920x1080 virtual1920x2160, offset0,1080; region
{0,0,1920,1080,0,0,3840,2160}. This does not diagnose previous invisible output.

Offline follow-up: both target NAND kernel slots confirm the GEM +0x70 size
store; fresh decompression matches reconstructed ELF segments. OSD binds that
allocation size separately from advertised dimensions. Update payload byte4
commits, byte5 requests gated vsync; 0x63 enables the region, not alpha. New raw
instruction logs and details are in OSD-BUFFER-CONTRACT. No TV access occurred.

Alpha/PM follow-up (OSD-KERNEL-INIT): normal-plane blend-enable callback does
no register write; SetPar global alpha is separate. Driver PM callbacks restore
cached registers/palettes, and PanDisplay can recover first-pan show state.
Existing fbdev is the first independent-validation candidate; cold/resume native
acceptance remains open. No new device call, deployment or TV access occurred.

## Settled decisions and next work

Owner focus is CEC/audio. Latest follow-up is CEC-OWNERSHIP-2026-11-01: PreInit
emulation and real registration are recovered, and a blocking raw backend now
exists/tested offline. Native authority/worker/reload integration and actual node/
hardware acceptance remain open. New PreInit project is durable in workbench.
November 1 follow-up: [CEC-LIVE](artifacts/reports/CEC-LIVE-2026-11-01.md).
The existing NDAL fd 83 maps to /dev/video28 (ndp-hdmi-switch); shared-owner
logical-address query returned TV address 0. Native ntkcec major 229 is registered
but has no node; a temporary node's open failed with EPERM while stock owns it.
No raw init/close, RX dequeue, authority write or takeover was attempted.
Vendor cache identifies Steam Deck at LA8 / PA1000, power On. Three standard
queries 83/46/8f to LA8 returned ioctl success AND driver acceptance. Cache
mirrored name/address/power into an unseen LA4 slot; this is not proof of a second
client or fresh replies to each query. No response payloads were captured.
Temporary files/node and probe processes are gone. Same boot/UI, Ethernet/MAC,
SSH/telnet, hosts/Samba blocks and hook/keeper hashes verified afterward.
No persistent deployment, reboot, audio write or recovery change occurred.
Audio follow-up: [AUDIO-ROUTING](artifacts/reports/AUDIO-ROUTING-2026-11-01.md).
Verified live amp descriptor has one instance at 033341c0, ESMT AD82587_D (id1),
with callbacks matching AMP_AD82587_Create. Earlier mod_audio AD82586C comment
was wrong and is corrected. AOUT output 0 owns present speakers 0,1,2,3,5;
outputs 1/2 have no handle/list. Decoder main volume, speaker EasySet gain and
amp register volume are separate layers. A new offline audio-state diagnostic
decodes the saved 612-byte cache; strict host/sanitizer/ARM outputs match.
These /proc reads preserved the same live UI/boot and made no device/audio write.
Native amp GPIO/I2C/scale/lifecycle and audible acceptance remain open.
Subsequent [AUDIO-BOARD](artifacts/reports/AUDIO-BOARD-2026-11-01.md) recovers
reset GPIO114, MCU mute GPIO208/STBC8, I2C bus3/address34 and actual register-read
ABI. A temporary ARM amp-probe read registers03..05=1b/05/05, ioctl count2/exit0;
it did not initialize/reset the chip or change gain/mute. Staging/probe removed;
same stock PID/boot, network/access and privacy/hook/keeper identities verified.
An incorrect fd-inspection cat blocked on vendor stdin; its surviving PID9821
was explicitly terminated and absence verified. Use readlink for fd targets.
Native lifecycle, volume conversion and audible acceptance remain open.
Latest [AUDIO-VOLUME](artifacts/reports/AUDIO-VOLUME-2026-11-01.md) traces UI
volume via speaker gain and101-entry AQ tables. New common/audio_curve.c decodes
normal/aggressive tables with verified low16-bit narrowing. All101 normal entries
match a202-byte live vendor table. Strict host/sanitizer/ARM-qemu and the same
test executed on the actual TV pass. Staging removed; same boot/UI, Ethernet,
SSH/telnet, hosts/Samba and hook/keeper hashes verified afterward. No gain/mute
write, module deployment or recovery change. The helper is not yet integrated
with mod_audio/HAL; routing, profile/default policy and native audio lifecycle
remain open. SetMainVolume's named wrapper only range-checks, with no device write.
Latest [AUDIO-CURVE-MODULE](artifacts/reports/AUDIO-CURVE-MODULE-2026-11-01.md):
audio_curve now publishes audio.curve v1 with caller-owned table copies and
bounded lookup. AQ path/profile are explicit; no guessed fallback. Six real-DSO
lifecycle/CLI/hold/collision/file/FIFO/suspended-reload groups pass host,
sanitizer, ARM/qemu and on the TV. Full host suite passes. Three temporary staged
test/module files and fixtures removed; stock boot/UI/access/privacy/hook/keeper
unchanged. This data provider is not yet connected to mod_audio gain application.
Latest [AUDIO-GAIN-TRANSPORT](artifacts/reports/AUDIO-GAIN-TRANSPORT-2026-11-01.md)
finds that message-backed Hal_Audio_Ctrl0 can hide sendmsg failure, while the
speaker ALSA setter updates a request cache and ignores lower failure results.
Getter returns that cache; no audible acceptance follows. A new read-only
mixer-probe passed on TV with ALSA_CONFIG_PATH=/vendor/etc/alsa.conf, count1,
range0..100, value0. Default fresh-SSH configuration first failed; vendor/run.sh
supplies this variable for the stock runtime. Helper removed and boot/UI/access/
privacy/hook/keeper state verified unchanged. New durable Alsa project and
separate manifest retain receiver/cache evidence. Native gain integration still
needs checked dispatch/completion/lifetime, not a blind HAL table call.
Latest [AUDIO-NETLINK](artifacts/reports/AUDIO-NETLINK-2026-11-01.md) resolves
timing: the target kernel invokes the callback synchronously; table data is
copied before the syscall returns. Operation errors are still discarded.
New bounded codec passes host/sanitizer/ARM-qemu. A fresh native query returned
device0/left32768/right32768, exit0, without gain/table writes or HAL init. Probe
and staging removed; same boot/UI, network/access/privacy/hook/keeper verified.
Process/socket ownership and owned blocking execution remain to integrate. Codec
is not in production audio yet. Neither reply nor ALSA cache proves DSP/audio.
Follow-up: common/audio_client.c now owns a process-bound blocking session with
explicit fd lifecycle, validated queries and checked table/selection sends
(dispatched only). Five injected-I/O/fork groups pass host/sanitizer/ARM-qemu;
full host suite passes. Refactored query probe (44556 bytes, MD5
bfef5a6123cbece7d17dfee92440f5e3) passed on TV with the same raw gain values.
Staging removed; same boot/UI/access/protections verified. No real table/selection
write was tested. An owning worker now serializes that session off the host
loop and rejects NULL I/O with ENOTSUP. It is not wired into mod_audio.
Production artifacts were not rebuilt/deployed. OWNER-WORKER, AUDIO-NETLINK.
CEC-TRANSPORT-CONTRACT records the preceding transport evidence:
normal traffic uses private V4L2/kernel CEC, not UART. VPL close/reopen is unsafe
for reload; driver filtering alone rejects some spec-valid messages, while stock
layers lose/ignore that result. Packet marshalling is implemented/tested offline;
offline authority and an owning worker now cover injected I/O and an idle
module. Live open and hardware acceptance remain open. OWNER-WORKER.
CEC-AUDIO-CONTRACT records ARC/System Audio requirements and audio ABI fixes.
Required /tmp/opencode material has moved to artifacts/workbench (4.2 GB at
migration), including firmware, Ghidra projects, toolchains and specification
copies. Active paths are durable; temporary compatibility links are not required.
See workbench/README and SHA256SUMS. New PreInit/HdmiSwitch projects have separate
manifests in analysis/cec-ownership; the historical migration manifest is retained.
Installed files and recovery are unchanged.

Latest power follow-up is POWER-STANDBY-CONTRACT. Target Ethernet private ioctl
opcode 1 sets a next-suspend policy (0 keeps PHY power, 1 powers down), not an
immediate link transition. Driver file from dump and imported copy match MD5
2efe1ee138ee29c2d490f6e4a667a38d. The new internal host cycle is tested with fake
entry. mod_power now routes standby through the host.sleep service and a final
RAM-entry callback; ordinary hooks do not sleep. CLI standby and power/state
ordering pass seven wrapped-I/O groups. Other modules need explicit quiescence
hooks and device/wake ownership. No TV execution/deployment; recovery unchanged.

APPS-GRID-2026-11-01: the full-screen apps portal is DialogFullScreenPortal.svg
(114): background/main_rectangle, top strip (0,0,1921,162) #481730, appsGrid
(183,180,1554,720), scrollbar (1750,180,8,720), dialogTitle (7,39,546,90),
customerLogo (1587,32,300,120), paging (893,984,223,60), portalMode
(1474,984,250,60). FullScreenPortalDialog::init gets the appsGrid description and
calls FullScreenGrid::create(id 0, rect, items, dialog); create passes 5 as the
column count (SVGGrid +0x1e0) and offsets the rect by the parent client origin.
FullScreenGrid uses FullScreenGridFA.svg (375 focused, 376 unfocused): mRowL
(156,122,0,230), mRowR (576,125,0,229), scrollPos (591,127,6,229), mRowM/rowPos
(225,121,320,230). gTopL/mTopL and gBotL/mBotL are zero-size path/polyline
elements (tags 7/6), not rectangles, so mTopL/mBotL yield no rectangle and the
top/bottom margins are 0. The button SVG is FullScreenGridButtonFA/UFA
(373/374): mRowL (149,0,36,180), mRowM (187,0,320,180), mRowR (509,0,36,180),
bottom strip (69,180,545,66); text is FullScreenGridTextboxFA/UFA (378/379).
SVGGrid::calculateHeight (0x10c9d00) zeroes +0x1f4/+0x1f8/+0x1e4, then sets top
= mTopL height, bottom = mBotL height, row step = mRowL height (rect fields
+8/+0xc/+0x10/+0x14 are x/y/x1/y1). SVGGrid::create (0x10cacb8) stores the
column count in +0x1e0. renderBackground (0x10c9ed0) computes visible rows =
(h-top-bottom)/row step and column step = (w-left-right)/cols with __aeabi_idiv
(0x1285478, truncated); item height = rowPos height; getButtonPosition
(0x10ca8ac) gives x = col*step + left, y = row*rowstep + top,
x2 = x+left+step+right, y2 = y+item_h. The per-cell background loop rewrites
left/right from rowPos and the mask rectangles, so after a paint they hold the
last cell's values; createListButtons (0x10ca924) then calls getButtonPosition
for every cell. A fresh grid has zeroes (col*310, width 310); which state the
vendor uses at createListButtons time is not established. Do not render the
grid with an assumed inset. Still undecoded: the HorizontalMaskScaleTransform
width evaluation and the widget-standard-name id->name map (0xeee798,
conditional).
common/vendor_grid.c encodes only the verified arithmetic and tests/vendor_grid.c
asserts the portal and gridFA numbers (make check-vgrid); no screen was rendered.

APPS-GRID-2-2026-11-01: decompiled with the new durable
artifacts/workbench/ghidra_scripts/DecompAt.java. SVGGrid::drawGrid (0x10cabc8)
is renderBackground -> createListButtons (0x10ca924) -> createScrollBar ->
update. createListButtons loops +0x1dc rows x +0x1e0 cols, calls
getButtonPosition and then makeButton/addButtonItem; getTotalNumberOfRows is
ceil(items/+0x1e0). SVGGridButton::renderBackground (0x10ccad0) resizes its
surface to the button window and renders the button SVG as a 3-slice
(left/right masks stretched vertically, mRowM stretched horizontally), so the
button window is exactly the getButtonPosition rect. Vendor assets for these
widgets are in artifacts/ui-assets: gridButton_fa.png 236x354, myapps_animated
320x180 (matches mRowM 320x180), app art 240x135/320x180. The runtime
contradiction stands: renderBackground's per-cell writes leave +0x1fc =
rowPos.x - cell.x and +0x200 = cell.x1 - rowPos.x1 (last cell: -1015/+1005 for
the portal), while a fresh grid gives the expected col*310/310. drawGrid calls
createListButtons after renderBackground, so static order does not explain the
vendor result; the RectangleContainer may return renderer-scaled rowPos. The
grid must not be rendered with an assumed inset; resolve with the vendor object
state (not while the set is in use) or the on-screen portal. commonCreateStep
(0x10cac10) sets the column count before drawGrid and SVGGridButton::create
(0x10ccdcc) uses the rect unchanged, so the last-cell fields really would put
columns 0..3 at negative x; nothing resets them. Working hypothesis: the
full-screen portal is broken or unused on this model (DialogNavigator only
reaches it through a portal navigation message), so do not port it as parity
without observing the vendor screen. SubLauncherDialog is instead a
SettingsDialog (SVG 189) created by LauncherDialog::showSubLauncher;
SettingsDialog is an AnyUIRefreshMessageFullScreenDialog (widget 175) with no
constructor window offset, so the p0/03 settings capture is not the plain
DialogSubLauncher listBox. The tested layout
now also exposes renderBackground's cell rect and the DialogFullScreenPortal
chrome plan (oa_vgrid_cell, oa_portal_plan_init).

TV-USE-2026-11-01: the owner reported the set showing HDMI1 after the idle test
window; return to view-only until a new idle window. Before that report one
DIAL KeyMenu (1048) was sent while both fb layers captured BLANK; it returned
201 and no layer change was captured. No further keys or wake attempts were
made. Vendor PIDs 1785/1967 were running.

SOURCE-SELECT-2026-11-01: the vendor source list ("Bronlijst") is a
PopUpSelectionDialog created by DialogNavigator::doNavigateSourceMenuDialog
(0xd17b10) with SVG 185, DialogSourceSelection.svg: background 1920x1080,
main_rectangle (0,0,780,1080), listBox (18,136,750,933), dialogTitle
(54,47,695,60). The p0/06 screenshot matches this panel/title geometry; its
focused first row is at ~(20,147) with ~63 px rows at ~70 px pitch, and the
rows list TV/BACK AV/HDMI1..3/YPbPr/VGA-PC/Wireless Display/Audio video delen.
The row template and exact row height/pitch are not in the dialog resource;
they belong to the popup list row and remain undecoded. This is a real, used
screen (unlike the likely-dead full-screen portal) and is the better next UI
target once the TV is free. No implementation yet.

SOURCE-SELECT-IMPL-2026-11-01: mod_ui_vendor renders DialogSourceSelection.svg 185
as a real screen. Rail Select on the source icon opens the 780x1080 panel
(#2D1EBE), the 748x66 focus bar (#1994FE) at 18,152 with a 75 px pitch, and the
vendor's own text extracted by tools/extract-source-selection.py
(src_title/src_0..src_8, ten new OAI1 blobs). Up/Down move the bar, Exit
returns; focused-row text scales between the capture's 34 px and 28 px ink
sizes. tests/ui_sources.c checks the state and fills; make check 232 ok. Text is
the Dutch capture; localization and exact row-template focus metrics remain
open. On-TV run pending (owner allows fb1).

UI-ENGINE-PLAN-2026-11-01: owner direction: the whole UI engine must be its own
module so ui.omarchy can replace the skin after parity, with ui.vendor as the
known working fallback. Design in docs/ARCHITECTURE.md: ui.engine owns display/
assets/present/ack/input/screen dispatch and publishes ui.engine v1; ui.vendor
registers the current screens and keeps OA_SVC_PORTAL. Not implemented yet;
TODO has the acceptance gates.

CCACHE-2026-11-01: ccache 4.14.1 installed. The Makefile wraps the default host
CC and the arm/tv cross compilers automatically; an explicit CC= is untouched
and CCACHE_DISABLE=1 disables. Most module/test rules compile+link in one
-shared call and are uncacheable; splitting them is a TODO. Build-only runs can
use make -j$(nproc) all; check stays sequential.

SOURCE-SELECT-TV-2026-11-01: the source-selection screen ran on the real set
under the owner's fb1 permission while HDMI1 stayed on the video plane. Native
ARM host PID17592 staged in /tmp/oa-src-stage (mod_ui_vendor md5
3c1b1bd562be3fdd79dd9a8946fc6790), a test plat stub (mod_display requires the
service; the real plat would re-run the vendor HAL), and mod_display with
set_region/mirror/pan 0. The legacy p0/06 capture is R/B swapped: its panel
(45,30,190) and bar (25,148,254) are the resource colours #BE1E2D and #FE9419
with R/B exchanged. The screen now uses the resource palette (panel #BE1E2D,
bar #FE9419, focused ink #7E000A, white unfocused rows). tvshot fb1 matched the
swap-corrected p0/06 panel (0,0,780,1080) with RMSE 181.535/65535 (0.28%) and
AE 61.7/842400 (0.0073%); flat colours exact. fb1 was backed up (16,588,800
bytes, yoffset1080) and restored (bytes 16588800, offset 0,1080); staging
removed; no openaurora left. src_0..src_8 and src_*_f are the vendor ink.
Evidence: artifacts/analysis/source-selection-2026-11-01/. The live RGBA-asset
-> BGRA-fb1 -> tvshot-BGRA path is consistent; the error was importing a legacy
swapped capture, not the render path.

UI-ENGINE-SPLIT-2026-11-01: mod_ui_engine publishes ui.engine v1 and now owns
display/alpha discovery and rebinding, lazy OAI1 assets (bounded cache), draw
ops, present/async receipts and the readiness receipt. It emits ui/ready when it
binds or rebinds a display; subscriber callbacks run in their own owner context,
so skin timers stay skin-owned. mod_ui_vendor is the vendor skin: it holds the
engine service (not display), keeps the portal/source screens, devctl and
OA_SVC_PORTAL, calls reset_ready on start and repaints on ui/ready. Every UI
test and tests/recovery.py load ui_engine before ui_vendor; full make check 232 ok.
The vendor skin is the known working fallback. Still pending: move key/screen
dispatch into the engine, rename the DSO/module to ui_vendor, add a
config/control-plane skin selection and write ui_omarchy against ui.engine.
The split also ran on fb1 with plat,display,ui_engine,ui_vendor (PID21135,
mod_ui_engine md5 d831bd64..., mod_ui_vendor 8056eb04...) and produced a
byte-identical capture to the pre-split screen (sha256 8f34f218...); fb1 was
restored (16588800 bytes, offset 0,1080), staging removed, no helper left.
Evidence in artifacts/analysis/source-selection-2026-11-01/.

UI-SKIN-SEAM-2026-11-01: ui.engine now subscribes OA_TOPIC_KEY and owns the
ui.fps repaint timer; skin enter/paint/key callbacks are posted to the skin's
owner token, so actions and timers stay skin-owned. register_skin/unregister_skin
plus ui.skin select the active skin. mod_ui_fb was renamed to ui_vendor (dir,
file, descriptor, tests, boot config, scene-module-client). ui_omarchy is a
skeleton skin (dark shell, accent rail, placeholder tiles, its own key handler)
with module ui_omarchy v0.1. tests/ui_skin switches vendor->omarchy->vendor by
unload/load with no rebuild; full make check 233 ok. The renamed ui_vendor also
ran on fb1 (PID24069, md5 283b5201...; engine 69470d50...) and produced the same
source-screen sha256 8f34f218...; fb1 restored, staging removed. Still open:
control-plane fallback loading when the selected skin fails and the real
OmarchyTV screens; the vendor skin remains the fallback.

SETTINGS-SCREEN-2026-11-01: p0/03 ("Beeld") is the SubLauncherDialog picture-
settings overlay, not a generic settings list. SubLauncherDialog::
initialiseFields (0xee3615) dispatches item type 2 to
initialisePictureSettingsShortcuts (0xee0ed0); that function builds the top
SettingItems and the category shortcuts. Resources: DialogSubLauncher.svg (189)
listBox (189,255,1774,222); DialogSettings_fullscreen.svg (177) and
DialogSettings.svg (175) are vertical panels with listBox (16/20,170/171,750,
826/895), bottomText, warningText; DialogSettings_tab.svg (179)
main_rectangle (0,162,780,918), listBox (20,170,750,826), dolbyVisionIcon;
DialogTabMenu.svg (191) tabs picture/sound/network/installation/system at
x=11,163,316,467,619, y=104,150x58, dialogTitle (57,5,634,90), glyph
(38,24,60,60). SettingHorizontalListBoxFA (467): selectionIndicator
(0,0,240,140), rowPos/mRowM (278,90,170,106), mRowL/R (225/329,90,10,140);
SettingHorizontalListBoxRowFA (471): mRowL/M/R (0/50/202,0,50/151/50,155).
SettingListBoxFA (475): rowPos/mRowM (244,178,75,70), mTop/mBot pieces;
SettingListBoxRowFA (479): mRowL/M/R (18/54/178,61,33/120/33,66) #FE9419.
SettingsDialog::createSettingsList (0xd5692d) creates the list from the
dialog's "listBox" description; setListBoxHorizontal (0xd56ebd) calls
SVGListBoxBase::makeHorizontalListBox. Row templates: SettingItemsListBoxRowItems
(460) is an 813x66 row (mRowM, text1 (11,-16,382,50), button (429,-17,348,64),
selectionbox/checkbox/slidebar/progressbar); its _sublauncher variant (464) is a
240x140 tile (mRowM (10,-140,240,140), button (10,-140,246,140), text1
(10,-134,240,54), banner (10,-170,960,180)); SettingListBoxRowSelected (483) is
a 296x71 #F7941E row. Still unresolved: whether p0/03's bottom row is the
launcher app row showing sub-launcher shortcuts and the top tiles are the
SubLauncherDialog settings list, or all of it is one SettingsDialog; the dialog
popup anchor/window offset and the SettingItemsListBox item placement on the
horizontal list are not decoded.

What ships is explicitly the captured state, not that interaction: ui_vendor
SCREEN_SETTINGS=3 paints three R/B-corrected p0/03 bands (settings_rail
0,180,176,660; settings_top 0,240,1920,180; settings_cat 0,440,1920,260 with
the "Beeld" caption) over background (13,14,14). ui.screen=settings and
Select on rail 4 enter it; Exit/Menu return to the portal. It is static: no
navigation, per-category lists or localization. tests/ui_settings.c passes,
and a real fb1 run (PID26424, mod_ui_vendor md5 524d58bf...) is pixel-identical
to the swap-corrected p0/03 over the whole 1920x1080 (0 differing pixels, max
channel delta 0, identical sha256 13125757...); fb1 was backed up/restored
(16588800 bytes, offset 0,1080) and staging removed. Evidence:
artifacts/analysis/settings-2026-11-01/.

SETTINGS-PANEL-2026-11-01: artifacts/protocol/shot-settings.png (Oct 30, before
the R/B correction) is the vendor SettingsDialog picture tab: a 780x1080 panel
with "Instellingen", the paintbrush glyph, Beeld/Geluid tabs and the rows
Modus Natuurlijk, Contrast 95, Helderheid 50, Scherpte 50, Kleur 60,
Energiebesparing Uit, Beeld zoomen Auto, Virtual HDR Aan, Geavanceerde
instellinge. Its (45,30,190) is #BE1E2D swapped. Rows use
SettingItemsListBoxRowItems (460) at a 65..66 px pitch; tabs are the
DialogSettings_tab (179)/DialogTabMenu (191) family. ui_vendor now has
SCREEN_SETTINGS_LIST=4 (settings_panel 780x1080 at 0,0), entered by Select on
the p0/03 quick tiles or ui.screen=settings_list; Exit returns to the tiles,
Exit again to the portal. The panel area is opaque; the rest of the OSD is
cleared transparent (clear_rgba 0,0,0,0) so the video stays visible. The
capture's black right side is an empty canvas, not a painted backdrop: the
dialog's full-screen "background" record is named and named backgrounds are
descriptions, not drawn (UI-TRANSPARENCY-2026-11-01). Raw fb1 confirms alpha 0
at (1000,500) and (900,900), alpha 255 at (100,500) BGRA 45,30,190,255. The
first run's opaque black clear was corrected. tests/ui_settings_panel.c covers
the flow and tests/ui_settings.c the tile entry. A real fb1 run (PID30540,
mod_ui_vendor md5 c12f800f..., panel asset 0979dbf2...) is pixel-identical to
the corrected capture over 1920x1080 (0/2,073,600 differing, max delta 0,
sha256 cf4a2296...); fb1 backed up/restored (16588800 bytes, offset 0,1080),
staging removed. Row focus selection now exists as a reconstruction, see
SETTINGS-ROWS-2026-11-01; the panel capture remains the unfocused reference.
Evidence: artifacts/analysis/settings-panel-2026-11-01/.

SETTINGS-ROWS-2026-11-01: SCREEN_SETTINGS_LIST now has Up/Down row selection
(sel, -1 none) at the captured 70 px pitch: FA overlay settings_focus_N 750x66
blitted at (20, 183+70*N), row 6 ("Beeld zoomen", grey/disabled in the capture)
skipped, clamped at both ends, ui.sel=N preselects for lab captures, Select
only logs (no value editing), Exit returns to the quick tiles. FA evidence:
state mapping 0=UFA/1=FA/2=UFI/3=FI from SettingItemsListBox (0xd518f1) and
makeRow (0xd5072d); SettingListBoxRowFA 479 is the #FE9419 66 px 3-slice and
SVGListBoxRow::renderBackground (0x10bec09) stretches it horizontally
(HorizontalMaskScaleTransform); settingListTextboxFA 488 / ButtonFA 496 /
SelectionBoxFA 504 use #7E000A textPos (UFA 489/497/506 are white).
tools/render-settings-focus.py reconstructs the overlays from the corrected
panel (recolor only white ink on the bg->white line; keep sliders/icons).
tests/ui_settings_rows.c covers selection/skip/clamp/Exit; make check 236 ok.
Real fb1 run (PID32567, mod_ui_vendor b82ddbe2..., focus overlay e92ee007...)
matches the reconstructed reference exactly (0/2,073,600, sha256 86b66c00...);
raw fb1 alpha 0 at (1000,500), BGRA 25,148,254,255 (#FE9419) at (100,500),
45,30,190,255 panel at (24,185); fb1 restored, staging removed. This verifies
the pipeline, not a vendor focused-row capture: bar centering/row rect and
focused slider/button graphics are inferred; value editing, tabs, scrolling,
localization and native font/text integration remain open. UI-FONTS below
corrects the earlier FreeType assumption. Evidence:
artifacts/analysis/settings-focus-2026-11-01/.

SETTINGS-FOCUS-LIVE-2026-11-02: row selection + live text + ui.theme composed
on fb1 (PID12364, mod_ui_vendor 41eea1bc..., engine e307de66..., theme
47d7579a...; ui.screen=settings_list, ui.text=1, ui.sel=4). Capture sha256
ade07e09... differs from the live unfocused capture only inside the row-4 bar
band (x20..769, y463..528; 48,584 px); bar BGRA 25,148,254,255, alpha 0 outside
the panel. fb1 restored (16588800 bytes, offset 0,1080), staging removed, no
helper left. Evidence: same directory, tv-fb1-focused-row-4-live-text.png.

SETTINGS-EDIT-LIVE-2026-11-02: lab slider editing (NOT parity) verified live.
CORRECTION 2026-11-02 (adversarial review): the first capture's digits were
the focus overlay's own glyphs, not the edited value — live text was painted
under an opaque 750x66 band. Paint order fixed (flat bar + focused ink);
verified by fixed runs ("49" = 283 px spanning x660..703, "7" = 121 px
spanning x660..679, same origin). Bar frame now firmware-derived
(listBox+insets → Y0 186, pitch 70): fb1 run (mod_ui_vendor eb3768ca...)
shows the fill at exactly y256..321 for sel=1. Old 33c719a1 capture
superseded. Shadow store only, no DB writes; range 0..100 invented, step 1
matches the firmware default. fb1 restored each run (16588800 bytes,
offset 0,1080), staging removed, no helper left. Evidence:
artifacts/analysis/settings-edit-2026-11-01/.

Firmware before experiments; instructions before inferred Ghidra signatures.
No telemetry/advertising in any form. All other TV features remain parity scope.
Vendor animations, transitions and UI/UX effects are included, not yet accepted.
Measured 32bpp OSD storage is BGRA; OAI1/blit inputs are RGBA and screenshot PNGs
are converted RGB. Keep each boundary explicit; older captures may be swapped.
Follow-up audit corrected tvshot's RGB565 raw decoder and legacy splash's BGRA
pixel writer. Synthetic capture tests and strict C in-memory pixel checks pass;
no TV read/deployment or native/recovery binary rebuild occurred for this audit.
Use -Werror -pedantic; edit with apply_patch; use tools/tvssh.sh for authorized reads.

[Findings](FINDINGS.md) consolidates the OSD/PWM/input corrections.
[TODO](TODO.md) owns unfinished work; [HANDOFF](HANDOFF.md) gives exact RE entry points.
Document cleanup changed local documentation and explanatory source comments;
no executable behavior, binaries or TV state changed. Raw logs were preserved,
with six previously temporary DRM traces copied into the analysis directory.
