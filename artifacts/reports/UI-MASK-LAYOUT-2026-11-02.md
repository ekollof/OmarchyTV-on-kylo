# Mask positioning and list row arithmetic — 2026-11-02

Continues UI-TEXT-GEOMETRY, using Aurora MD5
64c252340d3af6c5d66072611b7bad7a. New helpers are pure layout arithmetic;
the optional live settings labels still use the earlier fitted coordinates.

## MaskPositioningTransform

Fresh decompile/ARM: artifacts/analysis/ui-mask-layout-2026-11-02/mask-transform.txt.
Relative subtraction eec424 is also preserved in the preceding geometry report's
rect-handler.txt. Object fields: surface corners+8..14, mask corners+18..24,
alignment+28, cached-found flag+34. getMask eec524 copies a found container
record's corners and caches it; if lookup fails, transformPoint/transformRect
leave their input untouched. Cache invalidation remains the caller's lifecycle
responsibility; the new helper receives an explicit mask, not a container/cache.

When a mask exists, transformPointRelativeToMask eec424 first subtracts its left
and top. alignPoint eec464 then applies these numeric cases:

| alignment | x anchor added | y anchor added |
|---|---|---|
|0,1,3,5|surface.left|surface.top|
|2,6|surface.right-mask.width|surface.top|
|4,7|surface.left|surface.bottom-mask.height|
|8|surface.right-mask.width|surface.bottom-mask.height|
|other|0|0|

The unsupported-value case still performs the preceding relative subtraction.
Do not substitute a conventional centered nine-position grid for this table.
transformRect eec624 translates the top-left then reconstructs right/bottom
from the original spans. It preserves width/height, including signed spans;
it does not scale the rectangle. This is distinct from both the horizontal
stretch transform and the text-height correction in UI-TEXT-GEOMETRY.

oa_text_rect_position_mask in common/text_layout.c implements these rules,
with NULL mask representing a failed lookup. Its arithmetic uses int64_t and
rejects out-of-range output without partial writes. It does not reproduce
firmware integer wrap or implement resource lookup/cache/reset.

## SVGListBoxBase row placement

Fresh getRowPosition10bbaa8 and SVGListBoxRow::renderBackground10bec08 are in
list-row-layout.txt. getRowPosition reads these runtime fields:

| field | meaning |
|---|---|
|e8/e9|horizontal/mirrored flags|
|1f4/1f8|vertical step/row height|
|1fc/200|top/bottom insets|
|204/208|left/right insets|
|20c/210|horizontal step/item width|

For index i, vertical rows are:

    left = leftInset
    top = topInset + verticalStep*i
    right = window.right-window.left-rightInset
    bottom = top+rowHeight

Horizontal rows are:

    left = leftInset + horizontalStep*i
    top = topInset
    right = left+itemWidth
    bottom = window.bottom-window.top-bottomInset

Mirrored horizontal changes only left:

    left = window.right-leftInset-(i+1)*horizontalStep

The last formula uses window.right itself, not its width. Preserve that actual
operand even if normal widget coordinates often start at0. The mirrored flag
has no effect in the vertical branch. Negative indices are arithmetic inputs;
no bounds/count/scrolling check is present in this function.

oa_list_row_rect implements these formulas from explicitly supplied
oa_list_metrics. Out-of-range output fails atomically; metadata/population,
visible-row selection, animations and theme parsing are not implied. The
renderBackground extract is saved as the next entry point for tracing how
resource slices and mask inputs reach the actual rows.

## Checks and current state

tests/text_layout.c now checks all nine numeric mask cases and an unsupported
case, nonzero surface/mask origins, absent mask, invalid surface, unchanged
extents, vertical/horizontal/mirrored rows, negative index and bounded failures.
Nonzero window origins distinguish mirrored window.right from a width-based
substitution. Existing height rounding and49,728 VFP comparisons remain intact.

Full strict host make check exits0; focused ASan/UBSan and ARM/qemu pass. Native
TV execution passes all new arithmetic cases and the existing VFP comparisons.
These are contract/CPU tests, not rendering or independent scanout acceptance.
No UI code or fitted positions were changed.

Native build-tv-v3/mask-layout/tests/text_layout:26776 bytes,
MD5 f45e0b8791697caa2cf09da628cb4d51, maximum GLIBC2.4. A separate successful
size/hash connection preceded execution. Only temporary process memory/CPU was
used. No devices/framebuffer opened. Staging removed; absence and stock state
independently checked. Same boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197,
liveAurora1785/zombie1967, hook/hosts, SSH22/telnet2323, Samba REJECT and safety
markers. No persistent changes. No helper or Ghidra job is pending.

Next populate these runtime metrics from the actual list/window/resource path,
apply slice positioning/stretch and text-height correction in their firmware
order, then recover text origin/baseline/alignment. Only that complete chain
can replace fitted label origins. Do not assume raw BinSVG geometry is already
transformed, or treat these helpers as complete vendor UI parity.
