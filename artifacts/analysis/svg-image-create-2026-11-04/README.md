# SVGImage::imageCreate evidence

Interpretation: [SVG-IMAGE-CREATE](../../reports/SVG-IMAGE-CREATE-2026-11-04.md).
The shipped body (0x10c9430, 288 bytes, CRC 0x5c029f6e) is copied verbatim
into its original page so encoded calls resolve to in-place stubs. Its guard
literal at 0x10c9550 is redirected. The three targetHandle BLs are retargeted
to a stub in the same page because 0x1149000 is held by an earlier test group.
No new decompile or TV query was needed.

```sh
make -C openaurora check-svg-image
make -C openaurora TARGET=tv BUILD=build-tv-v3/settings-row-render \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/settings-row-render/tests/svg_image build-tv-v3/settings-row-render/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The differential adds 16 created/uncreated x 0xb0 x field-set x parent cases;
the svg_image group is now 72 native/C cases. host-check.txt,
sanitizer-check.txt (ASan/UBSan), arm-unit.txt (ARM unit under qemu),
firmware-check.txt (full verify.cjs, 35 ok lines) and tv-test.txt (focused
real-TV CPU, exit 0, staging removed, no helper) record the passes.
