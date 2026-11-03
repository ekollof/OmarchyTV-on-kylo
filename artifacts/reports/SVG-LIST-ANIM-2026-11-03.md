# SVG-list select animation — 2026-11-03

**Superseded acceptance:** [ARM-HARNESS-REVIEW](ARM-HARNESS-REVIEW-2026-11-03.md)
replaces the14-case/string model below. Those cases did not prove full string
ownership or parsing/destructor order. New opaque owned/context-bound callbacks
and153 native cases cover those distinctions, signed sizes and bounded character
slicing. NULL names construct empty strings. The historical crash explanations
below were the earlier agent's diagnosis; no retained core proves the exact old
fault in this review. Runtime/provider/rendering acceptance remains open.

`common/svg_list_anim.c` gains SVGListBoxBase::startSelectAnimation. Runs
after the selected row changes and after every handled key/move/update:
returns silently when animation is disabled, when there is no selected row,
or when the row rectangle matches the stored +0x25c words. Otherwise it
archives the stored rectangle to +0x24c, stores the fresh rectangle to
+0x25c, and starts the first listed animation whose name minus its first
character equals the widget standard name, with value
atof(duration)*1000.0f. 14 native/C comparisons pass with exact call
order/args, written snapshot words and started float bits, plus strict host,
focused ASan/UBSan, ARM/qemu and all previous reference groups. No TV
access, wake, staging, deployment or pending job.

## Firmware contract

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Native reference fragment:

| address | bytes | CRC32 | body |
|---|---:|---|---|
| 0x10bbedc | 372 | 10f1780d | SVGListBoxBase::startSelectAnimation |

Only the guard-pointer literal is redirected; getAnimationEnabled,
selectedItem/getItemRow, the row getRect vtable slot, the standard-name/
size/getAnimation calls, animationController/startAnimation, the five
UString entry points and atof are controlled stubs, not a vendor UI. The
1000.0f scale was read from the shipped literal pool (ELF file offset of
0x10bc04c, `0x447a4000`); the UString (start 1, length-1) arguments were
verified against `subs r3,r0,#1 / movs r2,#1` at 0x10bbfb6/0x10bbfb8. Both
sides run the real atof on the same duration bytes and compare float bits;
UString construction is a 1:1 owned plain-C-string stub in ASCII scope
(length is byte count, substring is pointer+start). NULL ops, NULL
list/manager, NULL output or any NULL callback returns EINVAL.

The differential caught three real defects: UString length/substring stubs
that read the object bytes instead of dereferencing the stored character
pointer, a missing atof PLT redirect, and a Thumb trampoline installed
where the fragment's BLX-immediate enters ARM state (fixed with an ARM
`ldr pc,[pc,#-4]` redirect, cf. step_stub). A GPT-6.1 Sol subagent
identified the UString dereference defect from read-only sources; the
remaining two were found via qemu gdbstub backtraces with host gdb.

Gate cases cover disabled, missing-row and unchanged-rect exits; the search
sweep covers empty/missing containers, null-slot skips, non-matching names,
single-character names, exact-name (empty-substring) rejection, alternate
standard names, zero start values and multi-hundred scales, with first-match
exit verified. Snapshot words and started float bits are compared on every
case; the output float is asserted untouched when no animation starts.

## Still open

Parent refresh, scrollbar construction/rendering internals, and the owned
backend/context/consumer with display/scanout binding, independent startup
and visible parity. Animation start is not actual motion/rendering
acceptance, and no visible parity is claimed.

Evidence and reproduction: artifacts/analysis/svg-list-anim-2026-11-03/.
