# SVG-list lower bodies — 2026-11-03

`common/svg_list_bodies.c` implements the seven SVGListBoxBase key bodies
called by the router in SVG-LIST-INPUT (onSelectKey/onDownKey/onUpKey/
onPageDownKey/onPageUpKey/onKeyGoFirstItemGroup/onKeyGoLastItemGroup) through
owned callbacks. 26048 native/C comparisons pass with exact call order/args
and handled truth, plus strict host, focused sanitizers and ARM/qemu. The
owner resumed implementation after the SVG-LIST-INPUT stop; no TV access,
wake, staging, deployment or pending job.

## Firmware contract

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Native reference fragments:

| address | bytes | CRC32 | body |
|---|---:|---|---|
| 10bc964 | 148 | 6d2f8674 | SVGListBoxBase::onUpKey |
| 10bc9f8 | 176 | e84f6632 | SVGListBoxBase::onDownKey |
| 10bcaa8 | 152 | 66ad2f6d | SVGListBoxBase::onPageUpKey |
| 10bcb40 | 164 | 69caf4f4 | SVGListBoxBase::onPageDownKey |
| 10bcbe4 | 404 | bc4274b5 | SVGListBoxBase::onSelectKey |
| 10bce98 | 92 | 7deea255 | SVGListBoxBase::onKeyGoFirstItemGroup |
| 10bcef4 | 88 | 272417eb | SVGListBoxBase::onKeyGoLastItemGroup |

Only guard-pointer literals are redirected; RowManager/move/window/notify/
parent callees are controlled stubs, not a vendor UI instance. Framework and
vendor object initialization are not involved.

Each body returns unhandled when the list-box word +0x1ec is zero, and when
RowManager::size (this+0xf8) is zero; those paths run no callbacks. Pure
query reads repeat exactly as shipped (size is read twice in down/up/page
bodies, three times on the down wrap path, matching the native BL sequence).

Routing correction found by the differential: movement/window/animation
primitives run on the list box itself, while only the RowManager queries and
focusItem run on this+0xf8; getSelectedItem/parentWindow/id run on the list.
The +0x1ec word is an opaque count compared against first-visible+size
arithmetic; the last-group index is count-1. focusItem clamps mirror the
shipped bic/max0 and min(count-1) forms; page comparisons are signed32 per
the settings-navigation convention, all other arithmetic wraps uint32_t.

Direction correction (native proved the first implementation backwards):
page-down wraps (window-bottom + clamped focus) when signed
first+size-1 exceeds signed aux-size, and scrolls by the window size
otherwise; the earlier scroll/wrap assignment was inverted. Page-up was
already correct (scroll while signed first-1-size stays above zero).

Select reads size, then the parent; with a null parent it consumes without
notify. Otherwise getSelectedItem runs before the window-id read, then one
notify(parent,item,wid) covers the shipped ctor/post/destroy triple, and the
body consumes. First invokes the +0x48 group virtual with index 0 only when
the selected item is nonzero; last always invokes it with count-1. Neither
forwards the router-supplied message. Every handled run ends with
startSelectAnimation. NULL ops/callback/handled returns EINVAL untouched.

## Still open

Subsequent [SVG-LIST-MOVE](SVG-LIST-MOVE-2026-11-03.md) implements the eight
primitive move/selection/window bodies called by these key bodies. Primitive
move targets, RowManager internals, row getters and editing, parent refresh,
and the owned backend/context/consumer with permission/modal/focus/lifetime/
visible gates remain open. Callback routing is not actual widget/rendering
acceptance, and no visible parity is claimed.

Evidence and reproduction: artifacts/analysis/svg-list-bodies-2026-11-03/.
