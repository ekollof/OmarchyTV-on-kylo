# Title text uses boxed layout — 2026-11-03

The optional ui.title diagnostic now draws through oa_font_text_draw_box rather
than the raw glyph-only function. This integrates the recovered alignment4
(horizontal start, vertical center) and local clipping in the complete583x90
text viewport. The window/box coordinates are unchanged; no fitted offset was
added. ui.title remains default0 because horizontal/complete drawing parity is
still unverified.

## Firmware and correction

Fresh read-only ARM/decompile traces are under analysis/ui-title-draw-2026-11-02
(investigation began before midnight). Aurora MD5 remains
64c252340d3af6c5d66072611b7bad7a. renderForeground10c7a00 copies text bounds,
selects font via height/lines/type and forwards alignment and draw controls to
SVGSurface::drawText. Existing UI-TEXT-VIEWPORT establishes its alignment bridge;
TextRenderer::doVerticalPositioning1132f64 freshly confirms the center formula.

The previously recovered LogoTextbox.svg composition yields bounds108,5,691,95,
height90/alignment4/font18. Actual EF41 logical font height is80; therefore
local line Y=(90-80)/2=5. The old diagnostic painted at local Y0, bypassing this
policy. This accounts for the historical5px vertical ink difference without
changing parent, mask or surface origin. The earlier inference that glyph
bearing invariance forced a shifted widget box is corrected at its source.

The real-font corpus now renders Instellingen in that composed surface and
records local ink5,26..257,80; composed absolute ink is113,31..365,85. The
historical vendor capture's ink136,31..388,85 agrees vertically and still
differs horizontally by23px. This is a comparison with a dated capture, not
new display acceptance or proof of the current title state.

Fresh horizontal source traces cover TextControls b442bc, Line::scaleHorizontal-
Positions1134790/checkHorizontalScaling1134874, TextRenderer constructor1132ab8,
createNewLine11331b8/createNewWord1133210/processUTF8Characters11333e0/process
11338dc, and SVGTextBox::setText(UString)10c7828. The setter copies text; word
creation receives logical viewport origin; scaling uses runtime reference/ratio
and font-offset operands. These do not justify a universal23px inset. Runtime
ratios, draw controls, original text and presentation association remain open.

## Implementation and tests

The skin now allocates/draws the full title viewport and links common/font_box.c
and text_origin.c, reusing their existing validation/alignment/clip contracts.
The font-only/row rendering path and optional title fallback remain separate.
The synthetic UI test now supplies a positive logical font height, verifies the
full viewport blit and transparent top-left pixel (the old Y0 path painted it),
and adds a zero-height-title-font case: live row text survives while the title
falls back to its capture. Title-on/off, missing pack and lab preset cases pass.

Full strict host make check, focused ASan/UBSan including actual pack/font corpus,
and ARM/qemu synthetic/corpus/UI lifecycle checks pass. New native corpus also
passes on the TV:9 alignments/guard/validation tests,54 existing row lines and
the actual font18 title. It reads shipped files and uses process memory only;
no display/device/input/settings/network mutation or module deployment occurred.

Native artifacts in openaurora/build-tv-v3/title-box:

| artifact | identity |
|---|---|
|tests/font_box|154344 bytes; MD5 4da2f04d1f5f857d2a69b69598cbf9da; maximum GLIBC2.7|
|modules/mod_ui_vendor.so|MD5 e812251a49b3daa269b77988e1f3b42c; not deployed|

Foreground upload, separate successful native size/hash connection, then execution
and removal. Independent follow-up verifies no staging file or executable helper.
Boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197 and liveAurora1785 unchanged; firmware,
hook/hosts hashes match prior state, SSH22/telnet2323 listen, Samba OUTPUT REJECT
216.183.117.0/24 retained. This is a current view-only access sample, not a reboot
persistence test. Stock UI never lost ownership.

Next trace horizontal reference/ratio, draw controls and compositing; retain
default-off title until independent visual acceptance. Complete FA fonts/widgets,
native settings interaction, cold startup and other TV feature gates remain open.
