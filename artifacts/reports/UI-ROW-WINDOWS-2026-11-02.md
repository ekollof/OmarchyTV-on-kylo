# Ordered row-item geometry — 2026-11-02

common/widget_layout.c now composes named row-item geometry from an actual
validated BinSVG record, using the recovered stream-order mask/container
semantics. It derives child windows from a supplied parent row window. This
does not yet replace the UI's fitted text origins or prove a live row instance.

## Firmware chain and ordering

Aurora MD5 64c252340d3af6c5d66072611b7bad7a. Fresh ARM/decompile for
SettingItemsListBoxRow::renderItems d54400, SVGListBoxRow::renderItems10bf06c,
setWindowRect10bf46c, drawRow10bf328 and BinSVGRenderer::renderGroup ef2330
is in artifacts/analysis/ui-row-windows-2026-11-02/. Its README maps the raw
files and corrects the secondary @eecc10 query's actual containing function.
The missing previous /tmp renderItems extract is no longer required.

Base renderItems loads the row-items resource from+a4 into+100, clears the
description container at+a8, and constructs a fresh horizontal scale transform.
Its surface is(0,0,window.width,window.height); ARM explicitly selects ID18
(mRowM) as mask and ID48(rowItems) as group. Pointer-chain verification for48
is eeea64 ->178e040 ->153ae33=rowItems, not a guessed neighboring name.

renderGroup clears the SAME output container and processes elements in stream
order. RectHandler ef6128 transforms a rectangle before appending its named
description. HorizontalMaskScaleTransform::getMask eecc24 queries that container
and caches the first found description. Therefore:

1. Before the named mask has been appended, transform lookup fails and geometry
   stays raw. Missing lookup is retried for later elements.
2. The first mask's own rectangle also stays raw; it cannot see itself before
   insertion. It becomes available to subsequent elements.
3. Subsequent rectangles scale relative X and width independently, translate Y,
   and reconstruct corners using the established retained-double helper.
4. Later duplicate masks do not replace the first/cached mask. First matching
   child name wins, including a child encountered before the mask.

A prelookup of all raw rectangles would incorrectly transform pre-mask items
and the first mask itself. Earlier arithmetic helpers still take valid explicit
inputs; this finding supplies the missing ordered resource/container lifecycle,
not a change to their scaling formula. Actual460 places mRowM first, so later
child rectangles do get scaled using the raw813px mask.

SettingItemsListBoxRow then uses transformed text1/labelLong/etc descriptions.
Existing children receive an offset by the row window's top-left before
setWindowRect (e.g. d54670). Creation uses the relative description and parent
through SVGTextBox::create; earlier firmware evidence establishes parent-client
translation and zero concrete row margins. A supplied absolute row window is
therefore the next required input, not another resource-space rowPos.

## Maintained APIs and evidence

oa_binsvg_rectangle decodes the exact rectangle at an iterator offset, including
unnamed rectangles; it avoids first-ID lookup when stream position matters.
oa_widget_group_rect walks the first selected group and nested groups, returns
the first named rectangle after the fresh ordered horizontal mask transform,
and borrows its metadata strings. Names/surface are explicit inputs. The
normal/non-mirrored oa_widget_row_item_window supplies rowItems/mRowM and a
local zero-origin surface, then adds the supplied absolute row origin. Errors
preserve outputs and overflow is rejected. These are geometry operations, not
a renderer, clipping, font selection or mirror/name policy.

Encountered animation tags return ENOTSUP: renderGroup groups9/10/11 into the
same depth-decrement branch and uses the previously documented defective
animation seek arithmetic. This helper does not silently claim support for
that path. Actual460 has no animations. Full animation-group handling and
other transforms remain pending; normal rectangle-only rowItems is supported.

tests/widget_layout.c checks pre-mask identity, the raw first mask, nested and
first-group lookup, duplicate mask caching, missing mask, transformed metadata,
absolute offset/overflow, rectangle accessor errors and animation rejection.
The actual460 corpus test supplies explicit row client(20,186,769,256), deriving:

| child | absolute corners |
|---|---|
| text1 |30,187,381,237|
| labelLong |30,187,480,237|
| text2 |638,183,763,237|

Adding70 to the row Y translates text2 to638,253,763,307. These use actual pack
coordinates and existing arithmetic, not copied fitted origins. The parent
window remains an explicit conditional input; this is not independent live
widget/font/raster parity or proof of which template/profile is active.

Strict full host suite, focused ASan/UBSan, ARM/qemu and native TV synthetic plus
actual460 tests pass. Isolated build-tv-v3/row-windows/tests/widget_layout:
85492 bytes, MD5 7d40f2f7997459b0e683e82b5eb02a71, maximum GLIBC2.4.
Separate successful size/hash connection preceded execution. It reads live theme
files and uses CPU/process memory only. /tmp/oa-row-windows-20261102 removed
and absence/processes independently checked. Same stock boot71f7eb63-727f-
4b9a-a9c7-2b04dc3f9197/live1785/zombie1967, hook/hosts, SSH22/telnet2323,
Samba REJECT and safety markers. No device/display/settings/persistent change,
no helper/build/Ghidra job pending.

Next compose the actual row window, textbox slice surfaces/textPos and foreground
origin/font/clip chain, then replace the fitted origins and validate FA/UFA on
the display. Textbox slice records488/489 are saved as raw evidence here; do not
add a fit-derived offset to make the incomplete composition resemble a capture.
