# Settings preview widgets — 2026-11-04

Scope: repair the existing captured-panel/live-text preview after the owner
reported missing focused sliders and Modus chevrons during a real GPU preview.
This is not completion of native settings widgets, live settings acquisition,
focused typography, animations, tabs or independent startup.

## Cause and firmware evidence

paint_settings_list replaced the captured row with a flat orange background,
then drew live labels/digits only. That erased the separate slider and
selection-image children. The earlier reconstructed focus assets are unsuitable:
they retain baked glyphs and grey widgets. Restoring those would reintroduce
the doubled-text defect recorded in UI-WORK-REVIEW.

Source ELF MD5 remains64c252340d3af6c5d66072611b7bad7a. Read-only Ghidra
decompilation **and ARM** are saved in ../analysis/display-review-2026-11-04/:

- slider-constructor.txt: SVGSlideBar10c4530 assigns528FA/530UFA/529FI/531UFI.
- slider-source.txt: renderBackground10c41ec positions gRowL/mRowL left-top,
  gRowR/mRowR right-top, then scales gRowM/mRowM between their widths.
  renderSlider10c4af0 reads mSlider width24, sets travel=window.width-24,
  clears the foreground and stretches gSlider into the calculated position.
  calculateSliderPosition10c4a80 in the cached disassembly divides travel by
  max-min, then multiplies value-min in double; zero range produces zero.
- slidebarFA528/UFA530: masks have11px end widths, middle210x30 and a6px
  horizontal track12px below the middle mask top. Track color is#FF0014
  focused, #9B9B9B normal. mSlider=(47,52,71,76); tag2 image has24x24,
  ../png/slidebarfa.png (or ufa), matrix(1 0 0 1 46 54): offset(-1,+2).
- selection-background.txt: SVGSelectionBox::renderBackground10b3798 creates
  left/right SVGImage children from transformed iconL/iconR descriptions.
  selection-source.txt includes updateArrows10b4020: visibility/focus, a
  suppress byte, item count, wrap flag and current index gate the arrows.
  Row renderItems supplies image IDs12/13, verified against image-pack index
  entries select_left/right.png (18x24). FA504 has18px cap widths and
  icon Y121 relative to mask107. Both cap transforms preserve dimensions.

The preview still supplies an explicit749px row client; that is reconstruction
input, not a newly read native instance. Applying verified record460's813px
horizontal mask gives slider window(489,rowY+18,165,49), track(500,rowY+30,
143,6), PNG origin(488+trunc((141/100)*value),rowY+20). Selection window is
(399,rowY+8,362,60), so PNG origins are(399/743,rowY+22). The separate
PNG image content stays at its native size. Screenshot comparison validates
these coordinates for this particular panel; no general SVG raster API follows.

## Implementation and reproducible diagnostic

ui_vendor redraws all four sliders using the same shadow values as their
digits, selecting the actual FA/UFA knob. Unfocused children erase the captured
track first; focused children retain the existing parent background. Filling
the whole49px child orange extended one pixel below the66px parent bar; the
third run exposed that and the final implementation omits that extra fill.
Focused reconstructed choices0/5/7 draw both original white chevrons after
the text erases. General provider-specific arrow gating remains unimplemented.
The0..100 range remains explicit lab/capture data, and ui.edit is default-off.
No actual setting value is written.

Export local vendor assets without embedding their bytes in source:

```
node openaurora/tools/export-slider-assets.cjs artifacts/ui-assets UI_DIRECTORY
```

This requires ImageMagick and validates the four PNG dimensions before writing
OAI1 RGBA. ui.text live-panel drawing rejects missing/wrong-sized widget assets
before clear/present. The real-display diagnostic also preflights the complete
780x1080 settings_panel and widget sizes before loading the skin.

scene-module-probe's OA_SCENE_PROBE_SETTINGS=1 mode uses settings_list/live text,
injects Down only into **its own** engine, and holds normal/focus0/1/2/3/4/7
for6seconds each. capture.cjs captures both BGRA framebuffer buffers as RGB
PNGs during those holds. It never sends stock keys. EGL-WINDOW's existing
full-backing/region/MAU/pan save/restore owns the display lifecycle.

## Evidence interpretation

The owner independently observed the missing widgets. The stock capture
stock-contrast_0.png shows the original focused Contrast; buffer1 retained the
older Modus frame. Do not assume buffer1 is always active or infer a failed key
from that inactive frame. stock-focus_1.png is the original focused Modus.
Screenshots are already RGB PNGs converted from BGRA; do not swap again.

Historical attempts are separated: first-missing-assets lacks settings_panel
in the installed asset bundle; second-missing-slider has the complete panel
but reproduces the owner's missing widgets; third-before-clip restores widgets
but exposes the one-pixel orange extension. Top-level phase PNGs belong to the
final corrected build. Captured references differ in tab availability, mode
text metrics and energy-saving state: do not claim whole-panel parity from
the narrow widget comparison. The skin still uses captured background/tabs,
fitted row text origins and normal glyph shapes for focused ink. Those gates
remain open, as do live resources/native widget and imageCreate integration.

## Verification and final TV state

Strict `make -C openaurora check` passes after the final clipping fix; the
focused ui_settings_text cases cover red track/knob restoration, chevrons,
shadow-value movement at0/95/100, no below-row fill and the existing font/title/
preset/fallback cases. ARM compiler is the durable GCC10.2 toolchain, with
TARGET=tv BUILD=build-tv-v3/display-review and the required strict CFLAGS.
No qemu/device inference was needed: the final module ran on the TV display.

Final scene-module-probe:542560bytes, MD5dced32954abe4ca03087f5e93a3c0225.
Final mod_ui_vendor.so:188488bytes, MD53467dc0004bdf5b34360eb6f4c289c8e;
maximum required GLIBC version2.17. Staged bundle and final replacement module
were size/hash checked in separate connections before execution.

Final GPU run: normal, Modus, four slider selections and Virtual HDR all
captured. The two buffer images are pixel-identical for each held phase.
compare.cjs checks an8085-pixel focused Contrast slider region and432pixels
for each Modus chevron against independent stock captures. Maximum channel
difference is2/255 in all three regions; RMS differences are0.133/0.374/0.394
on the0..255 scale. These are small nonzero differences, not bitwise parity.
Screenshot review confirms restored widgets and no orange pixel overhang.
The owner has not supplied a second independent visual acceptance verdict.

Probe exit0: full16588800-byte framebuffer backing restored, region_match=1,
snapshot_match=1, MAU0 and original pan0,0. Separate connection matched full
framebuffer MD541735daa69dc137743b59efe12db4a19, inspected exact executable
paths for leftover helpers (none), and retained coredumpnoreset. Only then
were /tmp staging and backup removed. No TV service restart, stock key
injection, settings write, network/boot change or persistent deployment occurred.
