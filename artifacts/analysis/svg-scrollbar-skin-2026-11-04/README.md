# Scrollbar skin minimum evidence

Interpretation: [SVG-SCROLLBAR-SKIN](../../reports/SVG-SCROLLBAR-SKIN-2026-11-04.md).

calculateMinBarSize 0x10c397c (0x174 bytes, CRC 0xd9fdda32) is copied verbatim.
The body caches the supplier image per orientation (+0x74/+0x84), resolves
getWidgetStandardName IDs 0x20/0x22 and 0x21/0x23, calls the renderer vtable
slot +0x14 for the background names and adds the moving-cap rectangle heights
to +0xb8. The recovered names are bar_gTopM/bar_mTopM and bar_gBotM/bar_mBotM;
the apps-grid caps are 20 and 16 px, so min_bar=36.

The 16 native/C cases vary orientation, cached image, supplier return and
bottom-cap presence; the fake supplier/renderer/RectangleContainer/name stubs
produce the same supplier/name/background/rect trace as the owned ops.

```sh
make -C openaurora check-svg-scrollbar
make -C openaurora TARGET=tv BUILD=build-tv-v3/settings-row-render \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/settings-row-render/tests/svg_scrollbar_skin build-tv-v3/settings-row-render/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

host-check.txt, sanitizer-check.txt (ASan/UBSan), arm-unit.txt (ARM under qemu),
firmware-check.txt (verify.cjs, 41 ok lines), arm-firmware.txt, tv-test.txt
(focused TV CPU, exit 0, staging removed) and tv-cleanup.txt record the passes.
The renderBackground/renderBar bodies, apps-grid screen, display/scanout
binding and independent startup remain open.
