# Display review evidence

Current conclusions: ../../reports/UI-DISPLAY-REVIEW-2026-11-04.md.
Top-level phase PNGs and tv-runtime.txt are the **final** widget/clipping fix.
compare.cjs/compare.txt validate independent stock widget crops and both frame
buffers; capture.cjs acquires held phases from the real TV.

- first-missing-assets/: first preview lacked the settings_panel asset.
- second-missing-slider/: complete panel, but focused sliders/chevrons absent;
  older whole-panel/buffer comparison numbers belong to this attempt only.
- third-before-clip/: restored widgets, but a49px orange child fill extended
  one pixel below the66px focus row. Its old crop metrics are superseded.
- stock-focus_1.png: original Modus focused; stock-contrast_0.png: original
  Contrast focused. The other buffer can retain an older frame.
- slider-source/constructor, selection-source/background and slider-rects:
  firmware source evidence, not calls made on the TV.
- slider-host.txt: final focused host regressions; host-full.txt: full host
  checks; slider-arm.txt: final ARM build. final-identities.txt and
  slider-stage.md5 identify final local/deployed content. final-symbol-versions
  is the final module's symbol-version report.
- slider-installed.txt records the third, pre-clipping build;
  slider-final-stage.txt records the separately verified final
  replacement module. slider-final-after.txt and slider-cleanup.txt establish
  restoration and removal. No helpers or uploads are pending.
- Remaining tv-*.txt, stage.md5, before PNGs and build*.txt are earlier
  preflight/upload/restoration records from the first two attempts; they do
  not identify the final build. slider-after-first.txt precedes the final
  one-pixel clipping correction.

All screenshot PNGs are already RGB, decoded from BGRA framebuffer bytes.
Vendor assets/binaries remain local under artifacts/workbench and are not
included here. The captured background is still a preview asset, and matching
widget crops does not prove full UI or native settings parity.
