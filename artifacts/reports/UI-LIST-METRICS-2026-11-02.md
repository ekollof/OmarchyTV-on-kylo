# List metric measurement phases — 2026-11-02

Continues UI-MASK-LAYOUT, Aurora MD5 64c252340d3af6c5d66072611b7bad7a.
Fresh ARM/decompile and tests: artifacts/analysis/ui-list-metrics-2026-11-02/.
The new pure data transitions feed the recovered row arithmetic. No runtime
theme parsing or changes to fitted UI text origins are included yet.

## Raw slices and corrected names

calculateHeight10bb7ec uses group/name IDs2/14 for top,5/17 for row-left,
6/18 and10/22 for horizontal middle/right additions,11/23 for bottom.
getWidgetStandardName eee798 selects char* globals, with explicit mirror
branches. Older four-entry table/name expansion is withdrawn at source.
ELF PT_LOAD reads (PC literal→global char*→string) are preserved in
widget-pointer-chains.txt. Normal/non-mirrored examples:

| ID | pointer chain | name |
|---|---|---|
|2|eee9b4→178df54→153af73|gTopL|
|14|eee9e4→178df40→153af3b|mTopL|
|17|eee9fc→178df3c→153ae7b|mRowL|
|18|eee9ec→178df70→153ae6f|mRowM|
|22|eee9e8→178df5c→153ae63|mRowR|
|23|eeea08→178df88→153af0f|mBotL|

Do not assume these normal names for mirrored IDs. Adjacent global pointer
values do not form a universal four-variant table.

calculateHeight resets top,bottom,verticalStep,horizontalStep to0, measuring
raw rectangle extents; rowHeight,itemWidth,left,right remain cached.
top=mTopL.height, bottom=mBotL.height, verticalStep=mRowL.height,
horizontalStep=mRowL.width. Horizontal mode additionally adds mRowM.width
and mRowR.width. Missing lookup contributes0. Title-row flag+23c adds one
verticalStep to top in either orientation.

SettingListBoxFA475/UFA477 raw slices (list-slices.tsv): top height16,
row-left height70/width0, row-middle width75, row-right width1, bottom height10.
These directly establish step70. With explicit750x826 window and no title,
vertical count would be(826-16-10)/70=11. That is capacity arithmetic, not
visible content count or an independently verified runtime widget instance.

## Visible count and painted-row cache

renderBackground10bd304 computes count before the per-row loop:

    vertical:   (window.height-top-bottom)/verticalStep
    horizontal: (window.width-left-right)/horizontalStep

Only a positive step updates count; otherwise previous count survives. Signed
division truncates toward0. Horizontal insets are therefore the previous
paint's values, not the new values discovered inside the current loop.

After side masks and middle surface are established, transformed rowPos updates
these fields at10bda60..10bda9a:

    rowHeight = rowPos.bottom-rowPos.top
    itemWidth = rowPos.right-rowPos.left
    left  = leftMask.width  + rowPos.left - middleSurface.left
    right = rightMask.width + middleSurface.right - rowPos.right

Missing rowPos preserves cached values. Last painted row wins; no count
recalculation occurs inside that loop. Raw resource coordinates are not valid
substitutes for the transformed rowPos here.

common/text_layout.c now supplies oa_list_measure_slices,
oa_list_measure_painted_row and oa_list_update_visible_count. All take explicit
inputs, reject range failures atomically and retain the firmware's phase/state
semantics. Overflow rejection replaces firmware integer wrapping. Resource
lookup, container ownership, transforms and drawing remain outside these APIs.

## Checks and continuation

tests/list_metrics.c covers the actual475/477 slice dimensions, title/horizontal
modes, preserved cached fields, last-painted state and visible-count ordering,
missing slices/rowPos, invalid inputs and range failures. Full strict host suite
passes. Focused ASan/UBSan and both layout tests pass ARM/qemu; native TV
list_metrics passes. Previous49,728 VFP cases rerun under qemu, not claimed as
a new native VFP run in this milestone.

Native build-tv-v3/list-metrics/tests/list_metrics:36008 bytes,
MD5 e888fe330468ff6b40e61b3e883d3c1d, GLIBC2.4. Separate successful size/hash
check preceded execution. CPU/process-memory test only. Temporary staging
removed and absence independently checked. Same stock boot71f7eb63-727f-4b9a-
a9c7-2b04dc3f9197/live1785/zombie1967, hook/hosts, SSH22/telnet2323,
Samba REJECT and safety markers. No device/display/persistent change.
No helper/Ghidra/build job pending.

Next row manager/window-coordinate step, complete middle-slice stretch/resource
transforms, then TextFactory origin/alignment. Fresh createListItems10bbc3c
is in list-metrics.txt; row background10bec08 is in UI-MASK-LAYOUT's
list-row-layout.txt. The metric transitions are implemented; actual transformed
inputs, complete widget rendering and replacement of fitted UI geometry remain
open. Do not infer those from the pure arithmetic tests.
