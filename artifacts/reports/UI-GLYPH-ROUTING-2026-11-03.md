# Text dispatch and background comparison — 2026-11-03

The firmware's character dispatcher explicitly chooses native GPU text when
the supplied draw context has a graphics context. The saved context has that
configuration. An added opaque-background pbuffer comparison still fails the
saved vendor title equality mask; transparency alone does not close parity.
This completes this diagnostic item, not title/UI parity or service integration.

## Actual firmware branch

Source Aurora MD5 64c252340d3af6c5d66072611b7bad7a. Fresh read-only/noanalysis
Ghidra decompile and ARM in analysis/ui-glyph-routing-2026-11-03:

- TextCommand::doExecute11321dc and renderText1131f94 forward the supplied
  AnyDrawContext into drawCharacter1131a90. The direct callers are11320d4 and
  11323d4; FindCallers confirms both rather than assuming a symbol-name match.
- At1131bbe..1131bc8, drawCharacter calls context vtable+0xc and branches on
  its returned pointer. Nonnull goes to that object's vtable+0x48 at1131bf8;
  null goes to context glyph/grayscale CPU slots+38/+3c at1131c1e onward.
- DrawContext vptr13f9404 slot+c is b43681, the object+28 getter already
  recovered in UI-FONT-GPU. Saved DrawContext03b24f60 has nonnull019f7ff8.
  That singleton's vptr13f8800 slot+48 is b34671/OpenGL drawText. Its saved
  dimensions1920x1080, font program8 and initialized1 remain consistent.
- drawGreyScaleGlyph b43ea0 is the CPU fallback with byte coverage/pixel API;
  drawGlyph b43d14 is the packed-bit fallback. Their existence does not imply
  that this configured context selects them for the title.

verify-route.cjs checks shipped vtables and the saved context bytes offline.
This is stronger than merely finding an initialized GL singleton, but it is
still conditional dispatch/configuration evidence, not a function trace of the
particular captured title. The saved heap objects are dated observations. No
new target function, ptrace, keys, UART reader or raw CEC query was used.

## Comparison condition and maintained probe

egl-glyph-probe now accepts an optional explicit R,G,B,A tuple for the title
base. Default remains transparent0,0,0,0. Strictly invalid/range/trailing/missing
components are rejected before EGL opens. Synthetic coverage checks now compare
both transparent and opaque-black parents: with mask128, transparent two-pass
red is about64/alpha128; opaque-black red is about128/alpha255. This verifies
the effect of the parent/FBO alpha rather than treating the glyph as isolated.

The native title experiment uses explicit178,29,43,255. RGB178,29,43 is sampled
from saved panel-ready_0.png at110,40/389,40/110,60/389,60, away from glyph ink.
Earlier UI-FONT-LIVE raw framebuffer evidence establishes an opaque panel at
100,500, but does not independently establish alpha at this title position.
The opaque title base is therefore a comparison condition, not a recovered
complete parent paint stack. No production colour or geometry was fitted.

| native title input | white core | differing core pixels vs saved vendor |
|---|---:|---:|
|one-pass transparent, previous item|1971|525|
|two-pass transparent, previous item|1228|1150|
|two-pass explicit opaque178,29,43|1318|1072|
|saved vendor reference|1446|0|

Crop583x90 at108,5; white means RGB all255. These are narrow masks, not full
colour/alpha or independent visible acceptance. An opaque base affects the
result but does not fix it. Do not conclude the native glyph API is unused,
change the established23,5 title origin, or assert that font bytes are corrupt.
Next continuation needs actual parent/active FBO/presentation state and a
matched complete scene comparison. No further hardware experiment is queued.

## Verification and final TV state

Strict optional host Mesa GLES check and focused ASan/UBSan pass, including the
new opaque-parent case. Four malformed tuple invocations return usage status2
before EGL. Native synthetic and real font18 title corpus pass on the actual
Mali pbuffer. No window, swap, framebuffer write or display ownership operation.

Native build-tv-v3/glyph-background/egl-glyph-probe:161920 bytes, MD5
e59d750f830df849618c4898281778bd, maximum GLIBC2.7. Foreground upload and
separate size/hash connection precede execution. Output8294400 bytes, MD5
be8e8e0ae0abe3e2aeb1ee9937fa4614; downloaded hash matches. Private raster and
core PNG remain under ignored workbench/title-horizontal-20261103.

Temporary executable/output removed and independent executable/file checks
confirm no helper/staging. Boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197, fb1
pan0,1080/backing41735daa69dc137743b59efe12db4a19, hook/hosts unchanged;
SSH22/telnet2323 and Samba OUTPUT REJECT retained. No reboot, restart, settings,
network or persistent deployment. ui.title stays default-off. Owner requested
handoff/commit/stop; no helper/build/Ghidra job is left pending.
