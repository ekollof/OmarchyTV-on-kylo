# Retained settings state and lifecycle evidence

Interpretation: [TAB-WRITEBACK](../../reports/TAB-WRITEBACK-2026-11-03.md).
Source extracts are sequential read-only Ghidra/ARM, with source ELF MD5 in each.
Call-reference inventories are not complete indirect-call/field-store proofs.
All test logs are offline; no TV connection or wake was performed.

From repository root:

```sh
make -C openaurora check-ui-navigation check-ui-navigation-ipc
make -C openaurora TARGET=tv BUILD=build-tv-v3/tab-policy \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/tab-policy/tests/ui_navigation build-tv-v3/tab-policy/modules/mod_ui_navigation.so \
  build-tv-v3/tab-policy/modules/mod_ticker.so build-tv-v3/tab-policy/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
node artifacts/analysis/tab-context-2026-11-03/verify-loader.cjs
```

The maintained hash-checking reference driver now also requires1792 shipped
retained-setter cases. Its private-mapping harness is offline/qemu-only, not
a native deployment. The DSO test independently checks1792 OAN2 round-trips.
