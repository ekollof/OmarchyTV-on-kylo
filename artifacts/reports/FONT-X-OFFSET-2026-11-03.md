# Global font bearing explains title and row origins — 2026-11-03

The missing title23px and normal MODE-row18px shifts are the vendor's global
font X offset: negative signed minimum-left bearing across the font. The boxed
renderer now applies that metric and its alignment-dependent viewport policy.
No captured-position constant or widget/resource translation was added.

## Correction to the source interpretation

Prior analysis incorrectly called b42469 a zero getter based on its shared
vtable address and an incorrect reading of saved decompile/ARM evidence. This
claim was not supported. Fresh ARM confirms:

```
b4247c  ldrsb.w r0,[r0,#5]
b42482  rsbs r0,r0
```

This computes `-signed(font_object[5])`. It is not a literal zero. EF41 decoder
b428d4 copies the terminal global glyph-dimension record; object+5 is its left
metric. getLogicalXOffsetLeft/getXOffsetLeft share this implementation. The
logical Y getter b42610 separately DOES return literal0 and is unchanged.

| font | global minimum-left | offset | evidence |
|---|---|---|---|
|7 regular rows|-18|18|shipped EF41 terminal dimensions|
|18 title|-23|23|shipped dimensions AND guarded live header byte e9|
|20 bold rows|-20|20|shipped dimensions|

The live font header read verified the boot ID and process executable before
reading9 bytes at the retained command's font pointer. Its vptr13f9340 and
header00,e9,50,44,43 agree with font18's global metrics. Raw bytes stay local.
The saved graph verifier now reads that actual byte; its earlier claim that
equal vtable entries proved zero was wrong and has been corrected. Related
report/canonical claims are corrected at their source.

Source ELF MD5 remains64c252340d3af6c5d66072611b7bad7a. Fresh sequential
read-only Ghidra ARM/decompiles under analysis/ui-title-x-rule-2026-11-03 cover
b42468, decoder b428d4, TextRenderer ctor1132ab8 and scaleHorizontalPositions
1134790. These establish both the getter and its use, not only a matching number.

## Placement rule and implementation

TextRenderer's constructor adds the physical/logical offset to the viewport's
leading edge for start/center, or subtracts it from the trailing edge for end.
Word creation starts at the logical viewport left. Scaling start1 preserves
that position; center/end convert logical versus physical offsets before their
alignment pass. Under the supported unit-ratio EF41 scope, the offsets cancel
in that conversion because both getters share the same value.

common/font_bitmap.c exposes oa_bitmap_font_x_offset. common/font_box.c uses
the offset in its viewport and initial LineObj origin before existing alignment
and trimming. No font-specific constants appear in implementation. Caller-
positioned raw glyph/line APIs stay unchanged, so existing fitted row blobs are
not double-indented. Collapsed font-offset viewports return ENOTSUP pending
complete empty-rectangle behavior; this is not claimed full vendor text parity.

Actual title583x90/font18/alignment4 now produces local origin23,5 and ink
28,26..280,80. With the existing text-surface position108,5, absolute ink is
136..388/31..85: exactly the current vendor capture's bounds and retained
LineObj position. Normal MODE's surface31,182/font7 yields glyph origin49,184,
accounting for the old fitted49 rather than inventing an18px inset. Indented
labels, other original strings and focused styles still need separate evidence.

## Validation and remaining pixel gate

Synthetic tests check all9 alignments with zero and nonzero global bearings,
pixel placement, positive minimum-left/negative offset, unsupported collapsed
viewport, arithmetic overflow and invalid/atomic error behavior. Real corpus
checks all54 existing row alignment lines and the exact title origin/bounds.
The UI DSO retains title-on/off, missing/bad title font and lab-edit fallback
coverage. Full strict host suite, focused ASan/UBSan with real corpus and ARM/
qemu groups pass. Native memory-only corpus passes on the actual TV.

The corpus can optionally write a local OAI1 title viewport as a sixth argument.
Comparison with saved panel-ready_0.png confirms placement/bounds but does not
close full raster/colour parity: selecting our alpha255 pixels versus the
captured pure-white pixels yields1971 versus1446 pixels and525 mask mismatches
in583x90. This is a narrow mask diagnostic, not a full RGBA differential or an
established cause. ui.title therefore stays default-off. Do not reinterpret
matching bounds as complete antialiasing/compositing or visual acceptance.

## Native identity and restoration

Isolated build-tv-v3/font-x/tests/font_box:162884 bytes,
MD5 22074919b41c8f0e789f0e3615ae3029, maximum GLIBC2.7.
Matching mod_ui_vendor.so MD5 55763db16b699b3240245fe5e56d13c5; not deployed.
Separate successful size/hash connection preceded CPU-only execution. Temporary
staging removed; independent file/executable scan confirms cleanup. Same boot
71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/hook/hosts, SSH22/telnet2323 and
Samba OUTPUT REJECT. No drawing/input/device/settings/persistent mutation,
reboot or service restart; stock operation retained. No helper/job pending.

Next investigate remaining raster/colour/compositing differences and original
indented/focused text before replacing all captures. The title X assignment and
normal MODE X discrepancy are closed by this instruction-derived metric rule;
do not revisit the superseded zero-offset/client-origin hypotheses.
