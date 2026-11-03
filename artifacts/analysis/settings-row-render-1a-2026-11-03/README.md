# Settings-row type-0x1a evidence

Interpretation: [SETTINGS-ROW-1A](../../reports/SETTINGS-ROW-1A-2026-11-03.md).
The shipped body was read from the ELF with objdump (the saved Ghidra `--asm`
dump is unreliable past the switch head). No new decompile or TV query was
needed. The type-0x1a tail sub-branch and the case-0x1a button body run
verbatim; only the guard pointer and the two "button" second-level key slots
are redirected. Every widget/rectangle/UString call is a controlled stub.

```sh
make -C openaurora check-settings-row-render
make -C openaurora TARGET=tv BUILD=build-tv-v3/settings-row-render \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/settings-row-render/tests/settings_row_render build-tv-v3/settings-row-render/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The maintained hash/CRC reference now sweeps types {0,1,4,0x1a} over the same
grid, requiring 3032 native/C render comparisons. host-check.txt,
sanitizer-check.txt (ASan/UBSan), arm-unit.txt (ARM unit under qemu),
firmware-check.txt (full verify.cjs) and tv-test.txt (focused real-TV CPU,
exit 0, staging removed, no helper) record the passes.
