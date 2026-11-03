# Settings-row type-0xd evidence (text-entry box)

Interpretation: [SETTINGS-ROW-13](../../reports/SETTINGS-ROW-13-2026-11-03.md).
The shipped body was read from the ELF with objdump (the saved Ghidra `--asm`
dump is unreliable past the switch head). No new decompile or TV query was
needed. The case body runs verbatim; only the guard pointer, the
"selectionbox" key (two literals) and the empty label are redirected. Every
SVGTextEntryBox call is a controlled stub.

```sh
make -C openaurora check-settings-row-render
make -C openaurora TARGET=tv BUILD=build-tv-v3/settings-row-render \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/settings-row-render/tests/settings_row_render build-tv-v3/settings-row-render/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The maintained hash/CRC reference sweeps types
{0,1,2,3,4,0xd,0x1a,0x16} over a six-key presence grid, requiring 24392
native/C render comparisons. host-check.txt, sanitizer-check.txt
(ASan/UBSan), arm-unit.txt (ARM unit under qemu), firmware-check.txt (full
verify.cjs) and tv-test.txt (focused real-TV CPU, exit 0, staging removed,
no helper) record the passes.
