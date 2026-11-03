# Settings-row renderItems type 0x16 — 2026-11-03

`common/settings_row_render.c` now covers type 0x16 (22): the shared
row+0x104 tail, then a checkbox case body that looks up the "checkbox"
rectangle and drives the row+0x27a8 child. Uncreated writes names 9/10/11/12
into slots 1/0/3/2, sets text alignment 5, creates from an empty label with
the (0, 0x74, 0x75) triplet, then applies the row's 0x2e44 checked byte;
created applies EASIRect math plus setWindowRect, setText(empty) and the same
checked byte. **7592** native/C comparisons pass with exact call order/args,
rect words, text bytes and row state, plus strict host, focused ASan/UBSan,
ARM/qemu, all prior groups and a focused real-TV CPU run (exit 0).

## Source and recovered behavior

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Read-only objdump/ELF. Case
0x16 enters at the jump-table target `0xd54b9e` (table[20]) and:

1. `getRectangle("checkbox")` (literal 0xd54e28 -> 0x178d610); return on null.
2. `isCreated(row+0x27a8)`.
3. Uncreated: `setSVGName` 9/10/11/12 into slots 1/0/3/2, then
   `SVGCheckBox::setTextAlignment(5)`.
4. Build a UString from the shipped empty literal at 0xd54e2c (`0x12f5cda`),
   then `SVGCheckBox::create(sl, 0x74, 0x75, ustr, rect, parent)` — a **seven
   argument** call (this + 6: two stack words plus the parent).
5. Destroy the temporary, then `SVGCheckBox::setChecked(row[0x2e44])`.
6. Created: EASIRect copy/point/offset, `setWindowRect`, `setText(empty)`,
   then the same `setChecked(row[0x2e44])`.

The harness needed two fidelity fixes this turn:

- **Row label slots are UString objects**, not raw char arrays: the first word
  is the character pointer. Earlier cases passed because both sides happened
  to read the slot as a raw string, but the checkbox's constructed UString
  exposed the conflation. `rr_setup_row`, `rr_ctext`, `rr_cbtn`, `rr_settext`
  and the C-side label readers now all dereference the slot, and every case
  was re-verified under the corrected convention.
- **A seven-argument preserving trampoline**: the checkbox create passes two
  stack words plus a parent, so the Thumb-2 `ldr.w r12; bx r12` stub was
  extended to a seven-parameter C shape; it never touches the stack.

The three new callbacks (`set_text_alignment`, `set_checked`,
`create_checkbox`) and the fifth rectangle key were added to the ops API; the
presence grid widened to five keys (32 masks) so the checkbox path is swept
with the rectangle both present and absent.

## Verification and limits

**7592** native/C comparisons: types {0,1,4,0x1a,0x16} × 32 five-key presence
masks × created × 3 point variants × row-word set × label pair × rebuild
flag, minus the 96 firmware-crashing type-4 inputs, plus eight default-type
cases. Host units add the checkbox name order (1/0/3/2 → 9/10/11/12),
alignment 5, the empty-label seven-arg create and the checked byte both ways.
Strict full host/actual Unix IPC, focused ASan/UBSan, fresh ARM/qemu units,
every prior group and the focused TV CPU test pass. Isolated build
`build-tv-v3/settings-row-render`:

| executable | bytes | MD5 |
|---|---:|---|
| tests/settings_row_render | 57908 | 9a3996a5f7b7a557e6df98438d80837b |
| tests/tab_policy_firmware | 1018508 | b7d9fc934aeb87adb442af25c74289a7 |

The standalone TV test requires only GLIBC_2.4 and the correct ARM loader.
tools/tvssh.sh found the TV already awake, verified no-reset flag/free space,
staged only the focused binary and checked size/hash in a separate connection
before execution. Test exit 0 passed, staging was removed and the /proc/*/exe
scan found no helper; /conf/coredumpnoreset remained present. No vendor
fragment, device API, screen/input/audio control, service restart, network/
settings/boot mutation or persistent deployment was run on the TV.

This closes type 0x16 and hardens the UString-slot convention for every later
case. Still open: switch case bodies 2..25 except 0x16, row construction,
child visibility internals, live settings/resource acquisition, scrollbar
internals, skin/module/display/scanout/startup and independent visible parity.
