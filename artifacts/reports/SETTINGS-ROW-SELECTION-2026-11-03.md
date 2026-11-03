# Settings-row renderItems types 0x2/0x3 — 2026-11-03

`common/settings_row_render.c` now covers the selection-box cases. Both look
up the "selectionbox" rectangle and drive the row+0x1014 child. Case 0x3
additionally calls the row's setSelectionBoxImageId with [row+0x2e54] — and
does so **after** the rectangle lookup but **before** the null check, which
the differential corrected. Created applies EASIRect math, setWindowRect and
setSelectedItemWithId([row+0x2de8]); uncreated writes four names in slot
order 1/3/0/2 (case 2 with key1 uses 0x26b plus font 0x1e, otherwise
0x2e9c/0x2ea0/0x2ea4/0x2ea8) and creates from an empty label with the
(0, 0xc, 0xd) triplet. **21320** native/C comparisons pass with exact call
order/args, rect words, text bytes and row state, plus strict host, focused
ASan/UBSan, ARM/qemu, all prior groups and a focused real-TV CPU run (exit 0).

## Source and recovered behavior

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Read-only objdump/ELF.

| type | jump-table target | key | child |
|---|---:|---|---|
| 0x2 | 0xd54c76 | "selectionbox" (0xd54e30 -> 0x178d5b8) | row+0x1014 |
| 0x3 | 0xd54d7c | "selectionbox" | row+0x1014 |

Case 0x2 flow: `getRectangle("selectionbox")`; null → return; `isCreated`;
created → EASIRect copy/point/offset, `setWindowRect`, then
`setSelectedItemWithId(row[0x2de8])`. Uncreated → key1 present ? names
0x26b×4 (slots 1/3/0/2) + `setFont(0x1e)` : names row[0x2ea0]/
row[0x2ea8]/row[0x2e9c]/row[0x2ea4], then an empty-label
`SVGSelectionBox::create(0,0xc,0xd,ustr,rect,parent)` and the temporary
destructor.

Case 0x3 differs only by the extra `setSelectionBoxImageId(row[0x2e54])`
between the lookup and the null check, and it never takes the 0x26b/font
branch. The created tail is shared with case 0x2 (`b.n d54d46` from
`d54e1e`).

The differential caught the ordering error: my first pass called
`set_selection_image` before the lookup, and the native trace showed
`get(selectionbox)` first. The corrected order also means the image id is
set even when the rectangle is missing.

## Verification and limits

**21320** native/C comparisons: types {0,1,2,3,4,0x1a,0x16} × 64 six-key
presence masks × created × 3 point variants × row-word set × label pair ×
rebuild flag, minus the 192 firmware-crashing type-4 inputs, plus eight
default-type cases. Host units add the two slot orders (shared tail 1/0/3/2
vs selection 1/3/0/2), the 0x2ea0/0x2ea4/0x2ea8/0x2e9c mapping, the empty
label and the created `setSelectedItemWithId` id. Strict full host/actual
Unix IPC, focused ASan/UBSan, fresh ARM/qemu units, every prior group and the
focused TV CPU test pass. Isolated build `build-tv-v3/settings-row-render`:

| executable | bytes | MD5 |
|---|---:|---|
| tests/settings_row_render | 68940 | de749fa24ece86e7b20de2fdcd06fabd |
| tests/tab_policy_firmware | 1028772 | 806eee8791762ba808a2fd17233dd5e0 |

The standalone TV test requires only GLIBC_2.4 and the correct ARM loader.
tools/tvssh.sh found the TV already awake, verified no-reset flag/free space,
staged only the focused binary and checked size/hash in a separate connection
before execution. Test exit 0 passed, staging was removed and the /proc/*/exe
scan found no helper; /conf/coredumpnoreset remained present. No vendor
fragment, device API, screen/input/audio control, service restart, network/
settings/boot mutation or persistent deployment was run on the TV.

`setSelectionBoxImageId` itself (0xd54374: store, "image" lookup, SVGImage
destroy/create) is a controlled stub here; its internals remain open with the
rest of the SVGImage path. Still open: switch case bodies 5..21 and 23..25,
row construction, child visibility internals, live settings/resource
acquisition, scrollbar internals, skin/module/display/scanout/startup and
independent visible parity.
