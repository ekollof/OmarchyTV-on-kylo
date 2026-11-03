# setSelectionBoxImageId internals evidence

Interpretation: [SETTINGS-ROW-SELIMAGE](../../reports/SETTINGS-ROW-SELIMAGE-2026-11-03.md).
The shipped setSelectionBoxImageId body at 0xd54374 (140 bytes, CRC
0xcf7f04a2) is now copied verbatim into the row mapping instead of being
stubbed, so the renderItems case-0x3 differential exercises the real
lookup/destroy/create sequence. Only its two literals are redirected (the
stack guard to the harness guard, and the "image" key slot to an owned
string). No new decompile or TV query was needed.

```sh
make -C openaurora check-settings-row-render
make -C openaurora TARGET=tv BUILD=build-tv-v3/settings-row-render \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/settings-row-render/tests/settings_row_render build-tv-v3/settings-row-render/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The presence mask gained a 15th key ("image"), so the sweep is now 32768
masks and 1770796 comparisons. host-check.txt, sanitizer-check.txt
(ASan/UBSan), arm-unit.txt (ARM unit under qemu), firmware-check.txt (full
verify.cjs) and tv-test.txt (focused real-TV CPU, exit 0, staging removed,
no helper) record the passes. verify.cjs's qemu timeout was raised from 30 s
to 180 s because the doubled sweep now takes ~33 s.
