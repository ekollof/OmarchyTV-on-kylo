# SVG-list first consumer module — 2026-11-03

`modules/ui_list/ui_list.c` publishes `ui.list` v1, the first service consumer
of the verified SVG-list layers. It owns the list-box count word, the manager
first/size/selected words and a 16-slot handle array, drives the verified
key router → key bodies → move primitives → RowManager focus → updateImpl
chain, and exposes `list-state`/`list-key` through the existing Unix
socket/CLI control plane with OAL1 save/restore across hot reload. Host,
focused ASan/UBSan, ARM/qemu and actual Unix-socket IPC checks pass. No TV
access, wake, staging, deployment or pending job.

## Firmware contract

No new firmware bytes. The module reuses the exact verified bodies and the
exact call order/operands asserted by SVG-LIST-INPUT/BODIES/MOVE/ROWS/IMPL/
SCROLLBAR/UPDATE: router precedence and aliases, zero-count/size unhandled
paths, move/window receiver split, manager field layout, updateImpl
writeback, scrollbar gating. Owned fixture geometry (documented, not
firmware): 9-item list, 5-row window, 16 slots, vertical layout with
page-down 0x20c/page-up 0x20d widget fields. Animation/scrollbar/full-update
render targets and the +0x48 group virtual stay open: render callbacks are
inert counters, and go_group clamps selection to the synthesized 0/count-1
index through verified first/size arithmetic instead of moving a group.

`list-key <code>` runs one router call and reports normalized handled 0/1;
unknown keys consume nothing. `list-state` reports all words and counters.
Suspend makes keys EBUSY while state stays queryable; shutdown gates keys
while queries still answer. OAL1 persists the 4 words plus handled and the 5
counters (44 bytes, exact-length, published-guard, magic-checked).

ARM-HARNESS-REVIEW corrects the earlier horizontal-layout description (the
router returns0) and adds actual unpublished OAL1 payload checks. Geometry,
handle capacity, selection and handled values are validated atomically; larger
virtual totals and empty states remain allowed. Earlier malformed-data tests
ran only while published and therefore did not reach payload validation.

## Still open

Widget-level row getters/editing (getSelectedItemId, numeric/check/slide),
parent refresh, scrollbar construction/rendering internals,
startSelectAnimation, and native visible parity. The module is the first
consumer, not a screen replacement: no display/input binding, no scanout,
no independent startup. Bodies remain callback routers with stubbed
rendering, as recorded in every prior SVG-LIST report.

Evidence and reproduction: artifacts/analysis/svg-list-module-2026-11-03/.
