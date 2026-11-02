# Text rectangle consumption and height scaling — 2026-11-02

Review of text-setup commits61ab377..443ebd0, using shipped Aurora MD5
64c252340d3af6c5d66072611b7bad7a. This closes the pending textPos/addition
question and corrects the old axis/rounding claims. The UI still uses fitted
origins for its optional live settings labels; no new screen behavior here.

## Instructions establish the draw source

SVGTextBox::renderBackground10c5e8c, lines==1 branch:

-10c621c..622e requests standard name42=textPos from the rectangle container.
-10c624e..6296 rescales that record's bottom relative to its top.
-After slice/font processing,10c6396..63a2 again constructs name42, then
  branches to10c6b7a. This is not an else-only path.
-10c6b7a..6bd6 looks up that same record and copies its name and geometry to
  object+1ec / coords+1f4..200 / tail+204..20c. Container lookup returns its
  stored record pointer, not a detached copy.
-renderForeground10c7a18..7a26 copy-constructs RectangleDescription from+1ec,
  then uses its geometry for the text surface and font selection. +254 is a
  distinct preamble copy of the optional `addition` rectangle.

Thus the open question in443ebd0 is resolved: textPos is consumed by the
single-line row path. Fresh create/foreground/container/mask/rect extracts are
in artifacts/analysis/ui-text-geometry-2026-11-02/. Background instructions and
marker literals are already durable in ui-text-layout-2026-11-01; the vanished
/tmp/opencode files are not prerequisites for continuing.

## Axes and rounding

RectHandler::handleSVGElement ef6128 reads SVG x,y,width,height (vtable getters
18/1c/20/24) and calls EASIRect(x,y,x+width,y+height). EASIRect9a7fcc stores
those four arguments in order. The container has an8-byte UString followed by
these corners. Record+0xc is top, +0x14 is bottom. HorizontalMaskScaleTransform
transformRect eecda8 separately scales first-axis spans; this agrees with the
corner order and is not the manual textbox height adjustment.

For the manual textPos correction:

    surfaceHeight = [r7+234] - [r7+22c]
    maskHeight    = [r7+26c] - [r7+264]
    span          = [record+14] - [record+0c]
    bottom        = top + trunc(float(surfaceHeight) / float(maskHeight) * float(span))

Offsets above are hexadecimal. The stack slots are the surface and mask corners
from their respective getMaskRectangle calls. Instructions use single-precision
vcvt/vdiv/vmul and vcvt.s32.f32 (toward zero). The previous width/double/
round-half-away interpretation is withdrawn at its source. Input int-to-float
conversion uses the normal default FP environment. This adjustment preserves
left/right/top and changes only bottom. The mask transform's own double width
ratio is a different operation; do not conflate them.

Also corrected at source: standard name65 is fontRegular, not fontLight.
UI-GLYPH-DRAW's literal chain already proves this, and the font-type writer is
the renderBackground marker scan (10c636c/637c/63aa..63c4), not an unknown
row-specific setter. Older row-inventory/static-label notes without retained
raw logs still require fresh instruction verification before implementation.

## Implementation and verification

common/text_layout.c + text_layout.h provide the recovered height arithmetic
as oa_text_rect_scale_height. It has no UI integration yet. Bounds reject a
zero mask, overflowing signed span, out-of-range float conversion or resulting
bottom; failure leaves the rectangle unchanged. These invalid-input failures
are deliberate safe API behavior, not reproduction of firmware overflow or
zero-division artifacts. It preserves float32 division-then-multiplication and
requires the default FP environment, with no fast-math/reassociation flags.

tests/text_layout.c covers preserved axes/origin, fractional positive/negative
truncation and range/denominator failures. On ARM it independently executes the
actual VFP conversion/division/multiplication/conversion instruction sequence
and compares49,728 combinations. Host strict C11/-Wall/-Wextra/-Werror/-pedantic
and ASan/UBSan pass. ARM/qemu and the native TV test pass every comparison.
Full host make check exits0, including the existing UI/font/module suites.
This is arithmetic equivalence, not whole-widget or visible-frame parity.

Native build-tv-v3/text-geometry/tests/text_layout:17620 bytes,
MD5 873adb0528dc642df3fab21e6be64cc9. Size/hash verified in a separate successful
connection before execution. TV test uses only CPU/process memory; no device
or framebuffer operation. Staging removed and independently checked. Same
boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197, stock live1785/zombie1967, hook/hosts,
SSH22/telnet2323, Samba REJECT and safety markers. No persistent deployment.

Next resolve slice/mask positioning and actual row/window allocation, then the
text origin/baseline/alignment. Apply this helper in a complete geometry path
once those inputs are established. Do not insert guessed dimensions to replace
the fitted label positions. Focus animations, title/tabs, localization, value
changes and complete replacement remain open. No helper or Ghidra job pending.
