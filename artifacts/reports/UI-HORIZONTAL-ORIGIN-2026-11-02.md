# Single-line horizontal layout and drawing — 2026-11-02

Aurora MD5 64c252340d3af6c5d66072611b7bad7a. Raw ARM/decompile and checks:
artifacts/analysis/ui-horizontal-origin-2026-11-02/. This adds an actual
single-line aligned renderer to common/font_text.c. It does not replace the
UI's fitted text origins or implement full shaping/style/layout parity.

## Physical width versus pen advance

BitmapFont::getPhysicalTextWidthIn64thsPixels b42b78 sums glyph advances plus
spacing, removes final spacing, then adds two adjustments:

    firstAdjustment = alignment!=1 ? max(-first.left,0) : 0
    lastAdjustment = last.right-last.advance
    physicalWidth = sum(advance)+spacing*(count-1)+firstAdjustment+lastAdjustment

The returned width and first adjustment are in64ths. adjustWidthForFirstChar
b42ae4 and adjustWidthForLastChar b42b34 confirm signed bytes1/3/4. Existing
oa_font_text_measure remains the earlier pen-advance API, including final
spacing; it is not silently redefined. The new line layout reports physical
width separately. Firmware bitmap X offset is0; horizontal scaling is outside
this new unit-ratio/unshaped EF41 path.

## End alignment and dispatch correction

Line::doCentreOrRightAlignment1134600 reads the final glyph's **right metric**,
not its advance: output glyph data starts at[r7+4], ldrsb[r7+7] reads byte3.
Its X is viewport.right-last.right. Earlier UI-TEXT-ORIGIN/HANDOFF wording
calling this an advance is corrected at source.

TextCommand::doExecute11321dc selects the helper from LineObj alignment:
1/4 and3 use LTR vtable154c434;2 uses RTL154c440. These literal values are
preserved in direction-vtables.txt. Constructor FindCallers returns no calls
because doExecute initializes these helper records directly; that is not
evidence RTL is unused. RTL helper1131708 starts with the last glyph, then
subtracts each preceding glyph's advance and spacing before painting it.
Reverse paint order matters for overlapping antialiased glyphs even at identical
origins because integer compositing can differ by one channel value.

This is renderer traversal, not a complete Unicode BiDi/shaping implementation.
Markup/style-run reversal is outside the new plain one-LineObj API.

## Centering and overhang

For a fitting bitmap line,1134600 computes in64ths:

    offset = floor((trunc((available64-width64)/2)+firstAdjustment64)/64)
    origin = oldLineX+offset

With this EF41 unit-ratio/integer-spacing API that equals
oldLineX+trunc((availablePixels-physicalWidth)/2)+firstAdjustment for the
nonnegative fitting difference.

centreOverhangingLine113452c tries slices[i,N-i), starting i=1 while N-i>i,
measuring each until it fits. Success adopts the slice and uses
floor((available64-sliceWidth64)/128). A fresh LineObj1135110 starts X0 and
createSlice11356f4 copies text/styles/cursor without copying old X, so success
does not retain oldLineX or add the fitting path's first-bearing adjustment.
If no nonempty symmetric slice fits, the original line/X survive. This is
character trimming, not merely draw clipping or an ellipsis.

## API, tests and limits

oa_font_text_layout_line and oa_font_text_draw_line decode/validate the entire
UTF8 line, allocate temporary glyph metadata/prefix advances, select range,
plan every glyph X and then draw through the existing coverage compositor.
Output includes selected decoded range, physical width, cursor origin and
reverse flag (origin is last-glyph X for end alignment). All invalid UTF8,
missing glyph, allocation and range failures precede painting. Font storage
must remain immutable. Safe errors replace integer-width overflow behavior.

Supported alignment1/2/3 is explicit. Word justification4 returns ENOTSUP;
styles/markup, BiDi shaping, wrapping, ellipsis, horizontal ratio scaling and
implicit viewport clipping are not included. Draw clipping is the separate
explicit clip argument. Baseline/window/theme integration is still pending.

tests/font_line.c distinguishes advance from physical width, checks fitting
center/negative first bearing, successful/failed symmetric trim and reset X,
draws the selected trimmed glyphs, and asserts end paint order with overlapping
coverage1/9 on a translucent background. It tests invalid UTF8 and origin
overflow without partial output. An optional live-pack corpus renders30
start/end/center lines using font7 and16, checking the final right-bearing
origin. These are instruction-derived contract/raster tests, not vendor
differential pixel comparisons or complete text acceptance.

Full strict host suite passes; focused ASan/UBSan including the corpus passes;
ARM/qemu and native TV synthetic/corpus checks pass. Native isolated
build-tv-v3/font-line/tests/font_line:82476 bytes,
MD5 bd1384ee414f673ad2858477ddcc7d3c, maximum GLIBC2.7. A separate successful
size/hash connection preceded execution. It reads live font records and draws
only to process memory. All staging removed and independently checked. Same
stock boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967,
hook/hosts, SSH22/telnet2323, Samba REJECT and safety markers. No device/display
or persistent changes. No helper/Ghidra/build job pending.

Next integrate concrete row/textbox styles, viewport/clip and vertical origin
with runtime resource geometry; retain the captured fallback until the whole
chain is verified. Word justification, truncation/scaling rules before alignment,
direction/shaping/localization and visible focused labels remain open. Do not
use the old advance-width or final-advance assumption to center/end-align text.
