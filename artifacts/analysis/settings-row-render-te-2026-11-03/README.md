# Settings-row text-entry cases 0xe/0xf/0x10 evidence

Interpretation: [SETTINGS-ROW-TEXTENTRY](../../reports/SETTINGS-ROW-TEXTENTRY-2026-11-03.md).
The shipped bodies were read from the ELF with objdump. No new decompile or
TV query was needed. All four text-entry cases run verbatim; only the guard
pointer, the "selectionbox" key literals and the empty label are redirected.
Every SVGTextEntryBox call is a controlled stub.

```sh
make -C openaurora check-settings-row-render
make -C openaurora TARGET=tv BUILD=build-tv-v3/settings-row-render \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/settings-row-render/tests/settings_row_render build-tv-v3/settings-row-render/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The maintained hash/CRC reference sweeps types
{0,1,2,3,4,0xd,0xe,0xf,0x10,0x1a,0x16}, requiring 33608 native/C render
comparisons. host-check.txt, sanitizer-check.txt (ASan/UBSan), arm-unit.txt
(ARM unit under qemu), firmware-check.txt (full verify.cjs) and tv-test.txt
(focused real-TV CPU, exit 0, staging removed, no helper) record the passes.
