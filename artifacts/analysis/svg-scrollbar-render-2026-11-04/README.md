# Scrollbar render bodies evidence

Interpretation: [SVG-SCROLLBAR-RENDER](../../reports/SVG-SCROLLBAR-RENDER-2026-11-04.md).

The range 0x10c3658..0x10c3f08 (renderBackground, calculateBarSizeAndPosition
and renderBar) is copied verbatim at its original address. The three
stack-guard pools (0x10c3970, 0x10c3aec, 0x10c3ba8, 0x10c3efc) are redirected;
a fake window-rect vtable, fake supplier/renderer tables, the transform/
container/surface stubs, the EASIRect constructor and the signed divider are
installed. 8 native/C cases vary background/bar, cache/supplier, empty bar,
min clamp and first position, and compare the full trace, cached image and
+0xc8 background-rendered flag.

```sh
make -C openaurora check-svg-scrollbar
make -C openaurora TARGET=tv BUILD=build-tv-v3/settings-row-render \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/settings-row-render/tests/svg_scrollbar_render build-tv-v3/settings-row-render/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

host-check.txt, sanitizer-check.txt (ASan/UBSan), arm-unit.txt (ARM under qemu),
firmware-check.txt (verify.cjs, 42 ok lines), arm-firmware.txt, tv-test.txt
(focused TV CPU, exit 0, staging removed) and tv-cleanup.txt record the passes.
The ops are abstract: the vendor mask/surface/renderer calls are stubbed, so
this is a call/geometry differential, not pixels. The apps-grid screen,
scrollbar rasterizer, display/scanout binding and independent startup remain
open.
