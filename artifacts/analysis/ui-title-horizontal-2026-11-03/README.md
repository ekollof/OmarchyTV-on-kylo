# Current title observation and static traces

See ../../reports/UI-TITLE-LIVE-GRAPH-2026-11-03.md for interpretation, the
non-atomic scope, exact pointer-field provenance and native cleanup.

Fresh DumpFirmware.java --asm queries (sequential Aurora project, -readOnly
-noanalysis, source MD5 64c252340d3af6c5d66072611b7bad7a):

| file | queries |
|---|---|
|draw-command-composition.txt|@11318a8 @10c87e8|
|factory.txt|@11326f0 @11327a0|
|animated-composition.txt|@10c8668|
|surface-position.txt|@114e700 @11612dc @1161330 @eedf70|
|hal-surface-rect.txt|@b31768|
|text-command-position.txt|@113260c @1131908 @1131a90|
|surface-commands.txt|@b318e8 @b319a4 @11321dc|
|command-fields.txt|@b434c4 @1134eb0 @1134ee0|
|alignment-dispatch.txt|@1134710 @1134600|
|logical-truncation.txt|@11349b0|
|line-object-fields.txt|@113528c @113522c @1135110|
|add-object.txt|@113420c @11332ab (containing processEmbeddedMarkup function)|
|line-mutation.txt|@11358a0 @11352cc (containing addChar) @1135a64|
|output-word-splitting.txt|@11343f8 @1134398 @1135678 @1132d24|

x-position-callers.txt uses FindCallers.java --calls @113522c, not a whole-ELF
grep. The8 direct references are evidence of setter calls, not a proof of no
other direct writes to the X field. Ghidra prototypes remain unreliable.

view-only native scan and metadata output are preserved as text. Private raw
object reads, complete maps and screenshot PNGs remain ignored in workbench/
title-horizontal-20261103. verify-graph.cjs consumes those snapshots locally
and validates the graph/mapped spans; it never follows pointers on a live TV.
verification.txt preserves its actual result. The early screenshot timing was
wrong; panel-ready_0/1.png are the later red-panel captures from the owner cue.
