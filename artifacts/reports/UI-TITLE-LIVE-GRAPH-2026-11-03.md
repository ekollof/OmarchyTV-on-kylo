# Live title command graph — 2026-11-03

Read-only observation narrows the remaining title placement problem to the
retained LineObj: its position is23,5, while the underlying text surface bounds
are108,5,691,95. Actual text is Instellingen, margins are zero, horizontal
justification is start1, vertical center3 and stored horizontal ratio is1/1.
The assignment rule producing X23 is still unproven; no fitted offset or
production renderer change was made.

## Screen association and capture correction

Owner opened the original settings panel and reported it visible. First fb1
captures contained launcher/settings-tile frames, while fb0/fb2 captures were
blank; a created dialog candidate alone did not establish visible content.
After the owner's explicit second ready cue, both new fb1 frames contain the
red settings panel. Cropping the title area x108..607/y0..94 and thresholding
white ink at80% with ImageMagick reports253x55+28+31, i.e. absolute ink
136..388/31..85. This independently confirms the current23px X difference
relative to UI-TITLE-BOX's113..365, without the old5px Y difference.

The early capture discrepancy is dated sampling evidence, not a driver/MAU
defect or reason to change memory selection. A fbregion binary was built
locally while investigating it, but was never uploaded or run. No framebuffer
ioctl was added by this investigation. Raw framebuffer32bpp was decoded BGRA;
saved screenshot PNGs are already RGB.

## Firmware-grounded memory reads

Aurora MD5:64c252340d3af6c5d66072611b7bad7a. Fresh sequential read-only Ghidra
ARM/decompile evidence: artifacts/analysis/ui-title-horizontal-2026-11-03.
The bounded scanner found one created font18 box at078b0600, with the same
57,5,691,95 window and51,0,634,90 local text bounds as the previous sample.
Its previously missing controls are flags1=1/flags2=0/override0/slide0/margin0.

Further bounded /proc/1785/mem reads follow the shipped pointer layouts, without
calling target functions or writing memory. Every saved span is validated
against the saved readable private heap maps by verify-graph.cjs:

| hop | instruction evidence / observation |
|---|---|
|SVGTextBox+1d8 -> UISurface|SVGSurface resize/setPosition and wrapper getters|
|UISurface+4 -> impl; impl+8 -> context; context[0] -> hal::Surface|UISurface::getRect114e700/Impl11612dc|
|Surface vptr13f86e0, slot1c ->b31769|getBounds copies URect+8; actual bounds108,5,691,95|
|Surface+20 -> RootDrawing; root+14 -> first command|Surface constructor/addCommand and DrawCommand::addChild b434c4|
|first command vptr154c44c -> TextCommand|handleTextBlock113260c allocatesac bytes; ctor1131908 stores font+1c/LineObj+20/controls+24|
|LineObj vptr154c52c; X+8/Y+c|getXPos1134eb0/getYPos1134ee0; actual23,5; length12|
|font vptr13f9340 -> BitmapFont|actual+1c/+20 slots point to zero getterb42469; no font logical X inset|
|LineObj+34/+38|stored horizontal ratio1/1|
|textbox UString+ a4/+a8|length12, storage begins exact Instellingen NUL; no hidden prefix|

Saved process bytes/screenshots remain ignored under
artifacts/workbench/title-horizontal-20261103, not in history. The maintained
offline verifier checks pointer links, spans, expected vptrs, fields and known
ELF identity; it performs no TV operation. Run from repository root:

```
node artifacts/analysis/ui-title-horizontal-2026-11-03/verify-graph.cjs
```

These are non-atomic observations: consistency of repeated prefixes/vptrs and
the owner's screen cue strengthen association but are not a stopped-process
snapshot. Stored1/1 is the line's glyph ratio, not a sampled transient
TextRenderer reference/ratio structure (that temporary object has been freed).

## Correction and next source question

The first draw flag+1b1 was called bold in UI-TEXT-SETUP. Its call chain actually
maps SVGSurface's bool -> UITextStyle+24 -> CharClipping -> TextControls+30,
copied into TextCommand+54. Current command value is1 with regular font18.
The font selector remains+1c4. The old bold label is corrected at its source;
do not use this bool to select a bold font or invent a bold-induced inset.

Fresh traces cover TextFactory, TextRenderer output/word creation, LineObj
construction/addChar, clipping, scaling/alignment dispatch and command storage.
FindCallers enumerates8 direct setXPos callsites, saved in x-position-callers.txt.
Start alignment does not itself reposition the line. No constant23 rule has
been found. Next resolve logical viewport/reference operands at word creation/
scaleHorizontalPositions and retained state, including direct field writes;
do not repeat the ruled-out dialog-client/surface-origin shift hypotheses.
Current ui.title stays default-off and X0 until its native creation rule is
recovered; the observed23 is diagnostic evidence, not a portable configuration.

## Execution and cleanup scope

Native scanner32936 bytes/MD5 9d542f8a2cc4121590a18a45552871f1 in isolated
build-tv-v3/title-horizontal. Foreground upload then separate successful native
size/hash connection preceded execution. Scan completes4 heap ranges /
176934912 bytes /104 candidates. Existing strict scanner regressions already
passed in UI-WORK-REVIEW; no scanner source changed this turn.

Scanner staging removed; independent absence/executable scan succeeds. All later
pointer observations use bounded dd reads and leave no helper. Boot remains
71f7eb63-727f-4b9a-a9c7-2b04dc3f9197 and Aurora1785, hook/hosts hashes unchanged.
No keys, drawing, settings writes, ptrace/signals, target function invocation,
device ownership, restart or persistent deployment. Owner can use the TV normally.
No Ghidra/build/capture job remains. Full replacement/visual acceptance stays open.
