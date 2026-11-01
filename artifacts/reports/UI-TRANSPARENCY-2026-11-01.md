# Rail/portal transparency mismatch — 2026-11-01

Status: firmware-derived translucent backdrop implemented in both display
backends; real GPU framebuffer alpha/premultiplication and lifecycle pass.
Independent visual comparison over live video remains open. Earlier screenshots/
static RGB comparison cannot establish transparency or composition over video.

The owner reports our rail UI has a dark background where the vendor UI is
transparent. Source confirms `ui_fb::paint` unconditionally clears the entire
display to RGB(14,14,13); display v2 clear has no alpha operand. scene_display
delegates to oa_canvas_fill, which writes alpha255. oa_canvas_blit likewise
flattens source pixels onto an opaque destination and writes alpha255. Merely
changing the clear color cannot fix this contract. The GLES owner already
clears its render target to transparent black and samples texture alpha; input
canvas opacity is a separate issue. CPU display must retain correct alpha too.

Fresh read-only Ghidra decompile plus ARM instructions from the shipped Aurora
MD5 64c252340d3af6c5d66072611b7bad7a establish:

- MainLauncherMenuDialog::showSubLauncherBackground e47f20 only forwards
  visibility to its child at +37f0; it is not a background-color/alpha setter.
- MainLauncherMenuDialog::init e4965c creates that child with image id0x2ce
  (e49dde/e49de4), initially hides it, and separately creates a child at +370c
  using id0x9c (e49e2e/e49e32). Trace image-resource lookup before equating integer
  ids to names solely from index order.
- SVGDialog::renderSVGData10c5104 delegates to the SVG renderer using the
  description at +110; it does not directly impose a full opaque color.
- RectHandler::handleSVGElement ef6128 extracts both fill and stroke properties
  and passes them to DrawingFacade. SVGRect::read ef3b34 reads the rectangle's
  opacity byte at fixed-data+10 into object+16 (ef3b70/72).
  SVGRect::getOpacity ef2bc8 returns it with **ldrsb** at ef2bdc, despite Ghidra's
  erroneous void prototype. Recover inheritance/range conversion in
  ShapeHandler::extractFillProperties ef6540 before interpreting that signed
  byte as an ordinary0..255 alpha.

Three background-named PNGs were carved offline using cumulative shipped-index
sizes, into ignored artifacts/workbench/ui-transparency-20261101. The two
11x11 mainmenu_background_tall/thin files have both transparent and nontransparent
pixels; tall includes alpha0,15,63,255. The100x100 sublauncher_background image
has alpha0 throughout. These are evidence that resource alpha matters, not proof
which image/layer supplies the owner's observed backdrop or its final opacity.
Recover the specific DialogMainMenu theme records and image lookup, then preserve
straight RGBA canvas and premultiplied BGRA scanout semantics end to end.

## Recovered fill and implemented contract

The bounded svg-animations reader now accepts --rectangles, preserving its
original animation output byte-for-byte. It consumes all702 records/11888
elements/289764bytes and emits raw signed fields/strings without guessing their
meaning. Existing truncation checks plus a signed rectangle golden row pass.
DialogMainMenu.svg/hashafb0b73e contains an **unnamed** rectangle at pack59908:
x0,y0,w1920,h1080, raw opacity6, fill#242628. Named background/main_rectangle
records are descriptions: RectHandler stores them instead of invoking the
unnamed draw path. Do not paint those opaque merely because they carry colors.

Fresh ShapeHandler::extractFillProperties ef6540 instructions call the opacity
getter, compute integerToScaledInteger(1) minus that value, multiply by255 and
convert back. integerToScaledInteger ef8860 multiplies by10; scaledIntegerToInt
ef88d4 adds/subtracts5 then divides by10. rgbColourParser ef63e8 **complements**
the result (mvns at ef6462), inserts it into the alpha byte (ef6482), and returns
RGBA in r0. Thus raw6 => round((10-6)*255/10)=102 => alpha153. SVGRect::getFill
loads object+1c, matching the reader's second string. The backdrop is straight
RGBA(36,38,40,153), not fully clear and not an opaque sampled RGB value.

Both display backends publish optional display.alpha v1 with clear_rgba, keeping
display v1/v2 layout and opaque clear semantics intact. The canvas remains
straight RGBA; source-over weights include destination alpha. Identity CPU
presentation now premultiplies RGB, matching its existing transition path and
GPU blending into transparent black. Clear invalidates the CPU opaque fast-path
flag when needed. Colored tiles remain opaque. No global OSD alpha change.

ui_fb acquires/releases the companion hold alongside its other display handles;
removal, suspend and failed-resume cleanup clear it. Providers without the new
companion retain the legacy opaque fallback; only matching rebuilt backends
provide the transparency correction. Clear failure stops painting that frame.

## Validation and real hardware

Strict host combined checks227 pass; focused ASan/UBSan display/canvas/UI/SVG
groups pass, including transparent source-over, opaque compatibility, identity/
fade premultiplication, UI backdrop RGBA and hold cleanup. The rectangle golden
test was added afterward and passes final strict/sanitizer SVG checks. Original
39 animation descriptors are byte-identical to the preserved earlier TSV.

Foreground temporary upload and separate hash/size verification preceded the
actual TV/GPU UI test. With the complete private rail bundle, PID13237 passed:

    UI dialog animation receipt=17179869210 completed screen=1 identity receipt=17179869212
    UI readiness after GPU completion serial=17179869216 revision=17179869697 pid=13237
    UI reload id=19 completed with fresh receipt
    UI host-only suspend/resume completed receipt=17179869219
    UI backdrop raw BGRA=24,23,22,153 expected24,23,22,153
    graphics host drained and unloaded provider
    restored bytes=16588800 yoffset=0 MAU=0 region_match=1 snapshot_match=1
    probe_exit=0

The readback samples the visible framebuffer's top-right background after the
resumed UI's completed frame. This verifies target native alpha/storage, not
independent visual equivalence or whole vendor animation fidelity. The rail
sample observed five image revisions in nominal660ms; frame order/30ms cadence
remain open. No native input/TV standby/cold takeover was exercised.

New native build-tv-v3/ui-alpha artifacts:

| artifact | bytes | MD5 | maximum glibc |
|---|---:|---|---|
| modules/mod_scene_display.so |59752|5d6a1d411c37b1769b2098c348bcd9c6|2.4|
| modules/mod_ui_fb.so |90184|42aa9ed02e8cdeceff9fee818569b4cd|2.4|
| scene-module-probe |500996|6acfb7f39389f2fd5f91958df1b554ab|2.17|

Other module/helper/renderer identities remain the earlier UI-ASYNC versions;
stage.md5 owns the exact temporary bundle. CPU display was exercised by its
wrapped-device unit, not used to take hardware ownership. Matching production16
stage and installed ABI1 assets/core/recovery are unchanged.
Same boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197, stock1785/zombie1967,
hook/hosts hashes, SSH22/telnet2323, Samba REJECT and safety markers remain.
Full framebuffer MD5 independently remains41735daa69dc137743b59efe12db4a19,
pan0,0. All staging/backups/readiness/helpers removed. Ignored local test bundle
is workbench/ui-alpha-stage-20261101; no persistent deployment.
Raw evidence is in analysis/ui-transparency-2026-11-01.
