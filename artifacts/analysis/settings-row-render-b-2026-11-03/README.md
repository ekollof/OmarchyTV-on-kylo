# Settings-row type-0xb evidence

Interpretation: [SETTINGS-ROW-B](../../reports/SETTINGS-ROW-B-2026-11-03.md).
The shipped body was read from the ELF with objdump. No new decompile or TV
query was needed. The case body runs verbatim with a full callee audit before
sweeping; only the guard pointer and the case's own key/label literals are
redirected. Every widget/rectangle/UString call is a controlled stub.

```sh
make -C openaurora check-settings-row-render
make -C openaurora TARGET=tv BUILD=build-tv-v3/settings-row-render \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/settings-row-render/tests/settings_row_render build-tv-v3/settings-row-render/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The maintained reference sweeps 18 types in a full 11-key presence pass, a
point/word/label/flag pass, and a dedicated type-0xb image-state pass,
requiring 74604 native/C render comparisons. host-check.txt,
sanitizer-check.txt (ASan/UBSan), arm-unit.txt (ARM unit under qemu),
firmware-check.txt (full verify.cjs) and tv-test.txt (focused real-TV CPU,
exit 0, staging removed, no helper) record the passes.
