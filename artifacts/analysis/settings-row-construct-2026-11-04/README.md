# SettingItemsListBoxRow constructor evidence

Interpretation: [SETTINGS-ROW-CONSTRUCT](../../reports/SETTINGS-ROW-CONSTRUCT-2026-11-04.md).
The shipped constructor at 0xd53d0c is **Thumb** code (symbol bit0 set); the
body is 0x4cd bytes, CRC 0xe68cf4e1. It is copied at its original address in an
8 KiB mapping so its PC-relative children/string/interface calls resolve to
stubs installed in place. The stack-guard literal at 0xd54098 is redirected and
the shared empty-string literal at 0xd540a0 points at an owned empty string.
No new decompile or TV query was needed.

```sh
make -C openaurora check-settings-row-construct
make -C openaurora TARGET=tv BUILD=build-tv-v3/settings-row-render \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/settings-row-render/tests/settings_row_construct build-tv-v3/settings-row-render/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The differential runs mirror-off/on = 2 cases x (47 word fields + one byte),
96 comparisons, and also compares the ordered base/child/UString/interface
calls. host-check.txt, sanitizer-check.txt (ASan/UBSan), arm-unit.txt (ARM unit
under qemu), firmware-check.txt (full verify.cjs, 36 ok lines) and tv-test.txt
(focused real-TV CPU, exit 0, staging removed, no helper) record the passes.
