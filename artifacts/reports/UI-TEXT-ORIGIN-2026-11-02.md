# Client margins and vertical line origins — 2026-11-02

Firmware Aurora MD5 64c252340d3af6c5d66072611b7bad7a. Raw ARM/decompile and
checks: artifacts/analysis/ui-text-origin-2026-11-02/. This recovers vertical
logical placement and ratio conversion, not complete widget text origins.
Optional UI labels still use fitted coordinates; no UI drawing changed.

## Concrete client-margin bindings

The primary vtable address point follows the8-byte header. Slot+38 points to
UIWindow::getClientMarginRect1152579 (Thumb address) for these classes:

| class | vtable symbol | slot address |
|---|---|---|
|UIWindow|154e850|154e890|
|SVGListBoxBase|153b3e4|153b424|
|SettingItemsListBox|14dbd6c|14dbdac|
|SVGListBoxRow|153b4a4|153b4e4|
|SettingItemsListBoxRow|14dbe24|14dbe64|
|SVGTextBox|153b8fc|153b93c|

The base implementation returns zero margins (previous UI-SLICE-STRETCH
client-margin.txt). Thus getClientRect's window-plus/minus-margin formula
has zero margins for these bindings. This closes that subclass question;
do not generalize it to every vendor window class. Row creation still adds
its parent's client origin, as the previous report established.

## Vertical placement

TextFactory::handleTextBlock113260c creates TextCommand per logical line block;
TextRenderer first positions its Line objects. doVerticalPositioning1132f64
reads logical viewport top/bottom (+20/+28), line step+58, font logical height
through vtable+28, line count from its list, justification+70 and clipping+78.
URect::Height c7f7f4 confirms bottom-top at struct offsets+c/+4.

With H=bottom-top, F=fontHeight, N=lineCount, S=lineStep:

    capacity = S != 0 ? trunc((H-F)/S) : 0
    blockHeight = F+(N-1)*S

| justification | first logical Y / step adjustment |
|---|---|
|1 start|top|
|2 end|bottom-blockHeight|
|3 center|top+trunc((H-blockHeight)/2)|
|4 spread|top; if N>1 and blockHeight<H, add trunc((H-blockHeight)/(N-1)) to S|

For center, if clipping field is not2 and capacity+1<N, blockHeight is replaced
by F+S*capacity before centering. Unknown justification does not position lines.
The center half-division uses signed truncation toward0; the ARM sign-bit-add
then asr at1133034..303e confirms it. Zero step avoids division.

setYPositionOfLines1132ed0 adds each logical line position to reference+40
and font logical Y offset (font slot+50), multiplies by ratio+34, then subtracts
the separately ratio-converted reference:

    pixelY = floor((reference+logicalY+fontOffset)*ratio) - floor(reference*ratio)

This is not generally floor((logicalY+fontOffset)*ratio). Ratio::roundDown
c7c85c uses denominator at+0 and numerator at+4, with the explicit negative
numerator path establishing floor for normal positive denominators.

BitmapFont's vtable013f9338 resolves logical height+28→b4249d, which delegates
to getHeightInPixels b424d1 at slot+2c. That getter returns signed extremes
bottom-top. The logical Y offset slot+50→b42611 returns0. Do not confuse those
slots with getPointSize/getMetricsResolution at+44/+48; their shared function
b425b1 returns object+24, initialized to1 by the encoded-font constructor.

## Implementation and checks

common/text_origin.c + text_origin.h provide oa_text_vertical_position and
oa_text_line_y with explicit fields/ratio/reference. They do not shape, wrap,
clip, select fonts or translate windows. Positive line count and valid numeric
justification are required; unknown justification returns ENOTSUP. The ratio
API requires nonnegative numerator/positive denominator and nonnegative index.
Invalid/range errors preserve output instead of emulating firmware overflow or
roundDown's zero-denominator fallback. This is a bounded arithmetic API.

tests/text_origin.c covers all four vertical cases, center clipping-capacity
behavior, odd negative spare height, zero step, ratio reference phase, negative
floor, font offset, invalid inputs and atomic bounds failures. Full strict host
suite and focused ASan/UBSan pass; ARM/qemu and native TV tests pass. These are
instruction-derived contract tests, not vendor differential execution or pixel
comparison. font_text.h now calls its existing origin caller-supplied rather
than claiming it is an established vendor text-box origin/baseline.

Native build-tv-v3/text-origin/tests/text_origin:30248 bytes,
MD5 d8dcf038231ff43f355573d7798f7b7a, maximum GLIBC2.4. Separate successful
size/hash verification preceded execution. CPU/process-memory only. Temporary
staging removed and absence independently checked. Same stock boot71f7eb63-
727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967, hook/hosts,
SSH22/telnet2323, Samba REJECT and safety markers. No display/device/persistent
change. No helper/build/Ghidra job pending.

## Continuation

Horizontal positioning remains incomplete. Line::doCentreOrRightAlignment1134600
uses physical length in64ths, first-character adjustment and separate overhang
handling for center. Its end-alignment path subtracts the final glyph's advance
and reverses style changes; do not replace it with an assumed right-minus-string-
width formula. doHorizontalPositioning1133050 also invokes truncation, position
scaling and scaling checks. Fresh line-origin-detail.txt/line-align-ratio.txt
contain these entry points. The accidental @1131600 query in the latter resolves
an unrelated SectorCommand function and is not text evidence. @1132040 in the
former resolves inside TextCommand::renderText, not a separate function.

Next establish horizontal/truncation/direction contracts, actual per-widget
UITextStyle/ratios and row/child coordinate composition, then runtime theme
provider/UI integration. Do not infer the fitted label origins or complete
baseline fidelity from these vertical helpers. Full replacement remains open.
