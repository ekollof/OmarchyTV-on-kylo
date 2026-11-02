# Boxed text alignment and rendering — 2026-11-02

common/font_box.c now joins numeric widget alignment, vertical positioning and
the plain EF41 line renderer into a zero-origin text-surface renderer. Actual
resource-derived surfaces/font7 and20 render offscreen through this chain.
Absolute parent placement/text state, non-unit ratios and visible widget parity
remain open. The UI's fitted origins/captured fallback are unchanged.

## Alignment bridge and coordinate scope

Aurora MD5 64c252340d3af6c5d66072611b7bad7a. Fresh ARM/decompile evidence:
artifacts/analysis/ui-text-viewport-2026-11-02/, with a README identifying raw
queries and file-backed table/vtable reads. Existing TextRenderer/Line evidence
is in UI-TEXT-ORIGIN/UI-HORIZONTAL-ORIGIN.

SVGSurface::drawText eedfe4 builds UITextStyle with font/line height and numeric
horizontal/vertical fields. UISurfaceImpl::drawText115ffec translates horizontal
style0/1/2/3 to HAL3/1/2/4 and vertical through bytes3,1,2,4 at154f700
(pointer11602b0). The resulting widget-to-HAL mapping is:

| widget alignment | horizontal | vertical |
|---|---|---|
|1|start1|start1|
|2|center3|start1|
|3|end2|start1|
|4|start1|center3|
|5|center3|center3|
|6|end2|center3|
|7|start1|end2|
|8|center3|end2|
|9|end2|end2|
|other|start1|center3|

The fallback is verified switch behavior, not a guessed conventional grid.
oa_text_box_justification exposes this bridge. SVGSurface supplies the text
height as line step; its call uses clip mode3. The fresh one-line/unit-ratio
renderer reuses oa_text_vertical_position/oa_text_line_y and the existing line
width/alignment/center-trim/end-reverse contracts. EF41 logical offsets are0.

Canvas IS the separate zero-origin text surface, so canvas bounds provide its
clipping. Alignment/trimming happens locally, including the center slice's X0
reset; the caller separately positions/blends the completed surface. No extra
intermediate raster/compositing layer is introduced by the boxed renderer.
It accepts explicit RGBA, spacing and immutable font data, and returns local
origin/range/policy metadata. Invalid text/glyph/range errors precede painting.

The scope is plain single-line EF41, positive logical height and unit X/Y ratios.
No markup, BiDi/shaping, wrapping, ellipsis, cursor/background policy, mirrored
alignment or retained overrides are claimed. This lower renderer does not infer
that the whole live widget necessarily uses unit ratios or choose its parent.

## Parent/spacing evidence that narrows the remaining work

SettingsDialog::createSettingsList d5692c looks up its description and passes the
rectangle to SVGListBoxBase::create. makeRow d5072c forwards state resource IDs,
the configured row-items ID and supplied row position into the concrete row.
SVGListBoxBase's constructor initializes cached insets/row fields to0.
SettingsDialog/PictureSettingsDialog/SVGDialog primary margin slots all bind
the same zero-margin1152579 base getter; they do not supply a new inset.

BitmapFont vptr slots+1c/+20 both bind the verified zero getter b42469. The
setLabel(UString) setter copies the supplied text; the string-key overload
localizes it. Neither setter itself adds a fixed prefix/padding. That does not
establish whether the original localized value/markup contains spacing.

For the earlier explicit conditional parent, the local regular/bold Y offsets
are2/1; text-surface bounds therefore give absolute glyph origins31,184 and
31,182. Those are conditional arithmetic, not independent vendor pixel proof.
The old fitted regular X49 is not reproduced by these inputs. Do not invent an
18px inset to hide this difference: inspect actual parent/window/text/resource/
ratio state first. In particular, the zero getter/margins and zero constructor
insets are not sources of such padding. Complete current-instance proof remains
required before replacing fits.

## Verification and TV state

tests/font_box.c checks exact single-pixel placement for all9 alignments with
guard bytes, invalid/default alignments, actual center trimming, atomic invalid
UTF8/font/dimension errors and unchanged output. The corpus joins resource460
child windows to489/488 surfaces and renders54 offscreen lines using font7/20
and all9 alignments; it checks local Y2/1. This is instruction-derived renderer/
data testing, not a vendor differential raster comparison or visible acceptance.

Full strict host suite, focused ASan/UBSan, ARM/qemu and native TV synthetic/
live-pack corpus pass. Native build-tv-v3/font-box/tests/font_box:
152496 bytes, MD5 e0c91d6934f7803622016fd48bbb427a, maximum GLIBC2.7.
Separate successful size/hash connection preceded execution. It reads SVG/font
files, uses CPU/process memory only, and sends no TV input. Staging removed and
absence/processes independently checked. Same stock boot71f7eb63-727f-4b9a-
a9c7-2b04dc3f9197/live1785/zombie1967, hook/hosts, SSH22/telnet2323,
Samba REJECT and safety markers. No display/device/settings/persistent operation,
no helper/build/Ghidra job pending.

Next establish actual parent/window and original text/resource/ratio state,
then wire this renderer into the skin and independently validate FA/UFA pixels
and interaction on the display. Broad localization/style/animation parity and
all other replacement gates remain open.
