# Settings-row label/progress/range setters evidence

Interpretation: [SETTINGS-ROW-SETTERS](../../reports/SETTINGS-ROW-SETTERS-2026-11-03.md).
Shipped bodies read from `artifacts/analysis/settings-row-2026-11-03/source.txt`
(Ghidra decompile plus `--asm` dumps, themselves read-only); no new decompile
or TV query was needed. Only the three guard-pointer literals are redirected
in the native reference; isCreated, setText, setPercentage, setSliderRangeValues,
the UString ctor/assign/append/dtor and __aeabi_idiv are controlled stubs, not
vendor implementations. Child offsets, store words, the (1,length-1)-style
search order and the division semantics execute verbatim against fixture state.

```sh
make -C openaurora check-settings-row-setters
make -C openaurora TARGET=tv BUILD=build-tv-v3/settings-row-setters \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/settings-row-setters/tests/settings_row_setters build-tv-v3/settings-row-setters/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The maintained hash/CRC checked reference requires 324 native/C setter
comparisons (child select/created gates/stores match). host-check.txt,
sanitizer-check.txt (ASan/UBSan), arm-unit.txt (ARM unit under qemu),
firmware-check.txt (full verify.cjs run) and tv-test.txt (focused real-TV
CPU, exit 0, staging removed, no helper) record the passes.
