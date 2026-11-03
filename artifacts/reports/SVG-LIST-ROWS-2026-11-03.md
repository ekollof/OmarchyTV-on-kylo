# SVG-list RowManager internals — 2026-11-03

`common/svg_list_rows.c` implements the SVGListBoxRowManager state backing
every list key/move body: the recovered field layout plus the focusItem body
and the updatePartial forward. 324 native/C comparisons pass with exact
field/ordering/store semantics, plus strict host, focused sanitizers and
ARM/qemu. The owner resumed the goal after the SVG-LIST-MOVE stop; no TV
access, wake, staging, deployment or pending job.

## Firmware contract

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Native reference fragments:

| address | bytes | CRC32 | body |
|---|---:|---|---|
| 10be6b0 | 52 | 35e7a0e3 | RowManager::updatePartial |
| 10be6e4 | 48 | 50e85ab7 | RowManager::size |
| 10be74c | 48 | ce283c69 | RowManager::selectedRow |
| 10be814 | 52 | 9f7c88dd | RowManager::selectedItem |
| 10be77c | 48 | 687fe952 | RowManager::getFirstVisibleItem |
| 10be3cc | 96 | b224a508 | RowManager::focusItem |

Only guard-pointer literals are redirected; updateImpl and the row +28 focus
virtual are controlled stubs, not vendor widgets. No vendor object
initialization is involved.

Recovered manager layout, all word offsets: +12 first-visible item, +16 row
count, +20 selected window-relative row, +28 row-pointer array. size,
selectedRow and getFirstVisibleItem are single field reads; selectedItem adds
first-visible and selected row. focusItem computes rel=index-first with
wrapping arithmetic, returns on zero count, rel reaching the count, or rel
already selected; otherwise it unfocuses the old row and focuses the new one
through the row +28 virtual (0 then 1) and stores rel. The differential
confirmed selectedRow 0 means "no selection to move from" in the key bodies,
not an absolute position, and confirmed the manager-row array indirection.
updatePartial forwards (index,false) to updateImpl. Getters return raw words
with errno out-param; focus/update return errno only. NULL ops or any NULL
callback returns EINVAL.

## Still open

Subsequent [SVG-LIST-SCROLL](SVG-LIST-SCROLL-2026-11-03.md) implements
updateScrollBar (gating, literal rect, show, position). updateImpl and row
widget internals (the +28 focus target), row getters and editing, parent
refresh, scrollbar construction/position bodies, startSelectAnimation, and
the owned backend/context/consumer with permission/modal/focus/lifetime/
visible gates remain open. Internals are not actual widget/rendering
acceptance, and no visible parity is claimed.

Evidence and reproduction: artifacts/analysis/svg-list-rows-2026-11-03/.
