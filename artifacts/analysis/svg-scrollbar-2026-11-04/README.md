# SVGScrollBar geometry/state evidence

Interpretation: [SVG-SCROLLBAR](../../reports/SVG-SCROLLBAR-2026-11-04.md).

Two shipped Thumb bodies are copied verbatim at their original addresses:
setScrollPosition 0x10c4138 (0x70 bytes, CRC 0x71cda0c0) and
calculateBarSizeAndPosition 0x10c3af0 (0xbc bytes, CRC 0x37806af8). The
geometry body's UIWindow::getWindowRect vtable call is supplied by a fake
object slot; the 0x1285478 signed divider and setScrollPosition's
isCreated/renderBackground/renderBar calls are stubs. 3120 native/C cases
(2880 geometry + 240 state) compare every field and reject writes outside
+0xb0..+0xc8. The apps-grid track is 1750,180,8x720, so the host unit uses a
720 px height.

```sh
make -C openaurora check-svg-scrollbar
make -C openaurora TARGET=tv BUILD=build-tv-v3/settings-row-render \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/settings-row-render/tests/svg_scrollbar build-tv-v3/settings-row-render/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

host-check.txt, sanitizer-check.txt (ASan/UBSan), arm-unit.txt (ARM under qemu),
firmware-check.txt (verify.cjs, 40 ok lines), arm-firmware.txt, tv-test.txt
(focused TV CPU, exit 0, staging removed) and tv-cleanup.txt record the passes.
The skin min-bar size, render bodies and apps-grid screen remain open.
