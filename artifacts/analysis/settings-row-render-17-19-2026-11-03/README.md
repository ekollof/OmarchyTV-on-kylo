# Settings-row types 0x17..0x19 evidence

Interpretation: [SETTINGS-ROW-17-19](../../reports/SETTINGS-ROW-17-19-2026-11-03.md).
The shipped bodies were read from the ELF with objdump. No new decompile or TV
query was needed. All three bodies run verbatim after a full callee audit; only
the guard pointer and their own key/label literals are redirected. Every
widget/rectangle/UString/progress-bar call is a controlled stub.

```sh
make -C openaurora check-settings-row-render
make -C openaurora TARGET=tv BUILD=build-tv-v3/settings-row-render \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/settings-row-render/tests/settings_row_render build-tv-v3/settings-row-render/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The reference now sweeps 27 types over the full 14-key presence mask (16384
masks), the point/word/label/flag pass, the type-0xb image-state pass, the
type-0xc textbox-state pass and a type-0x17..0x19 progress-bar-branch pass,
requiring 886060 native/C render comparisons. host-check.txt,
sanitizer-check.txt (ASan/UBSan), arm-unit.txt (ARM unit under qemu),
firmware-check.txt (full verify.cjs) and tv-test.txt (focused real-TV CPU,
exit 0, staging removed, no helper) record the passes.
