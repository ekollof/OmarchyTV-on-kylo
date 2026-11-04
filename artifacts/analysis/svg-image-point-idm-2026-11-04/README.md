# SVGImage point-create and convertOperationIdToIDM evidence

Interpretation: [SVG-IMAGE-POINT-IDM](../../reports/SVG-IMAGE-POINT-IDM-2026-11-04.md).
Two shipped bodies are copied verbatim at their original addresses and called
against the owned C:

- SVGImage::create(int, EASIPoint const&, UIWindow*) at 0x10c9554 (228 bytes,
  CRC 0x09f01190), with EASIRect::EASIRect() at 0x9a7f94 (52 bytes, CRC
  0x2a2b3d80) copied too and its guard literal redirected.
- LauncherHelper::convertOperationIdToIDM at 0xe4ae0c (116 bytes, CRC
  0xf2156f40), reading its 62-entry signed-16-bit table from a copy at the
  vendor address 0x14efd84.

The svg_image differential is now 80 native/C cases (8 point-overload cases);
the launcher group is 536 comparisons (67 IDM lookups). No new decompile or TV
query was needed.

```sh
make -C openaurora check-svg-image check-launcher-image-id
make -C openaurora TARGET=tv BUILD=build-tv-v3/settings-row-render \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/settings-row-render/tests/svg_image build-tv-v3/settings-row-render/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

host-svg.txt/host-idm.txt, sanitizer-svg.txt/sanitizer-idm.txt (ASan/UBSan),
arm-unit.txt (ARM unit under qemu), firmware-check.txt (full verify.cjs, 35 ok
lines) and tv-test.txt (focused real-TV CPU, exit 0, staging removed, no
helper) record the passes.
