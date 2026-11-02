# Offline row-label investigation

Source: local Aurora ELF MD5 64c252340d3af6c5d66072611b7bad7a.
Interpretation and exact profile results: ../../reports/UI-ROW-LABELS-2026-11-02.md.

Fresh DumpFirmware.java --asm queries against the existing Aurora project:

| file | queries |
|---|---|
|row-label.txt|@d50f60 @d50018|
|dictionary.txt|@114bd24 @114b9d8 @114ba1c @11fac40 @11fabdc @11fab4c @11fa68c @cc542c|
|dictionary-source.txt|@114b5f8 @114b5a0 @114ba6c data:00cc54c8:0x14 @11fab4c|
|profile-fields.txt|@c5ddcc @c5e190 @c5e298 @c5e350 @c5e3b4 @c5e100 @114bc74 data:011fabd8:4|
|profile-header.txt|@c5dd44 @c5e3f0 data:00c5e18c:4|

All commands used -readOnly -noanalysis -process aurora.elf; no concurrent
project opens. Ghidra's void-return metadata is wrong for several functions;
the ARM r0 result and callers establish pointer/index semantics.

DemoApp literal reads (first PT_LOAD VA-file delta0x10000): cc54cc ->12b7f6c
eng, cc54d0 ->12baafa langprofile, cc54d4 ->14d0da0 Dictionary,
cc54d8 ->14d0dab Languages. In the load call r2 uses cc54d8, r3 uses cc54d4:
section Languages, value Dictionary. Do not reverse those by literal-table order.
ROMProfile and Dictionary hash seed words both contain0xf1d3b597. ARM helper
128570c supplies division remainders in r1 for the hash's modulo3/modulo5 shifts.

verify-label.cjs runs only on the offline, hash-checked profile. It preserves
no vendor payload and requires Node only for this research check. The source
profile/binaries stay ignored in workbench. No TV operations were performed.
