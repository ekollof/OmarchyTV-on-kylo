# Setting-row type and child visibility — 2026-11-03

`common/settings_row_type.c` implements SettingItemsListBoxRow::setType,
including cache/rebuild ordering, the virtual render callback, subsequent
visibility query and exact ordered qualified child setters. A focused consumer
test joins the real owned settings model through its existing draw dispatcher
to real cached row type/child visibility fields. Other row setters, actual
renderItems and lower visibility methods remain open; no skin/module or screen
renderer was added.

## Source and recovered behavior

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Read-only decompile and ARM:
`analysis/settings-row-2026-11-03/source.txt`. The offline reference copies
1580 bytes at d51e00, CRC32 `370237d3`, preserving the original branch table;
only the guard pointer at d52158 is redirected. Constructor, renderItems and
lower visibility implementations are controlled callbacks.

A changed type is stored to row+2dec, then rebuild byte+2e49 becomes1, before
virtual+5c renderItems runs. An unchanged type clears rebuild to0 and skips
rendering, but still queries isVisible and runs the same child setters when
visible. The visibility query comes **after** renderItems, so rendering may
change whether the child phase happens. The comparison retains full32-bit type
values; it is not a truncated row-type enum or a signed range check.

When visible, child+3c4 is shown only for type1 and child+104 is its inverse.
Types0..26 then invoke their exact child sequences; larger unsigned types
(including27) stop after those two setters. Unknown types do not hide all
remaining children. Qualified selection/text-entry/checkbox visibility methods
are distinct from UIWindow::setVisible and stay explicit in the callback API.

| types | shown children beyond the two labels |
|---|---|
| 0,1 | none |
| 2 | 1014 |
| 3 | 1014,2cf0 |
| 4..8,26 | 684 |
| 9 | 13c0,1ca8 |
| 10 | 1498,1ca8 |
| 11 | 1498,2b28,2c0c |
| 12 | 1498,1ca8,1f68,2228 |
| 13..21 | 18d8 |
| 22 | 27a8 |
| 23 | 1570,24e8 |
| 24 | 1724,24e8; **1ca8 is not touched** |
| 25 | 2b28,2c0c,1724 |

The table is not a generic hide/show algorithm. Orders differ between ordinary,
slider, endpoint/image and progress rows. Type24 has only14 subsequent calls
and preserves existing child+1ca8 visibility. All other recognized types have15.
The implementation retains five literal offset orders and per-type shown sets,
with no deduplication or invented widget role names. Offsets are binding IDs,
not offsets a provider should apply to our C objects.

`render-source.txt` saves the complete actual renderItems decompile/ARM for
continuation. It reads the rebuild flag at d5483c and d563a6, including a branch
to reconstruct an existing button; do not replace this with an unconditional
post-render dirty-flag clear. Source for label, progress and range setters is
also saved in source.txt, but those bodies were not implemented by this change.

## Verification and limits

**12480** shipped ARM/C comparisons cover256 type bytes plus4 full-width
outliers,4 old-type cases (including unchanged),4 raw visible values and3
render visibility outcomes. They compare complete ordered traces, state as
observed inside callbacks, final row bytes and visible-query ordering. The
reference's child setter logs distinguish UIWindow/selection/text/check calls.
All earlier reference groups remain required and pass.

Host units additionally verify retained child state for type24 and unknown
types, hidden/unchanged/invalid gates, visibility changes during render, and
owned-model type9->23 writeback joined to the row cache and child fields.
Strict full host/actual Unix IPC, focused ASan/UBSan, fresh ARM/qemu and the
focused test on real TV CPU pass. Isolated build `build-tv-v3/settings-row`:

| executable | bytes | MD5 |
|---|---:|---|
| tests/settings_row_type | 73700 | 18e83e5cd1d9a9126172f211a022ad66 |
| tests/tab_policy_firmware | 884992 | 6f7d199a7942cbeb9c082c0028feb360 |

The standalone TV test requires only GLIBC_2.4 and the correct ARM loader.
tools/tvssh.sh found the TV already awake, verified no-reset flag/free space,
staged only the focused CPU binary and checked size/hash in a separate
connection before execution. Test exit0 passed, staging was removed and the
/proc/*/exe scan found no helper; /conf/coredumpnoreset remained present.
No vendor fragment, device API, screen/input/audio control, service restart,
network/settings/boot mutation or persistent deployment was run on the TV.

This establishes the type/visibility controller and a bounded owned-state
consumer join. It does not implement row construction/renderItems, child
visibility internals, live settings/resource acquisition, all row event setters,
scrollbar internals, skin/module/display/scanout/startup or native visible parity.
Use the saved render source for the next provider/widget integration work.
