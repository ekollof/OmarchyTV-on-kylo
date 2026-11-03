# Profile context evidence and reproduction

Interpretation: [TAB-CONTEXT](../../reports/TAB-CONTEXT-2026-11-03.md).
Ghidra extracts are sequential/read-only; provider-binding.txt comes from the
durable instruction cache. Build/test records are offline, not live TV checks.

From repository root:

```sh
make -C openaurora check-profile check-tab-policy check-ui-profile
openaurora/build/tests/profile artifacts/profiles/mb230_swprofile.bin
openaurora/build/tests/ui_profile openaurora/build/modules artifacts/profiles/mb230_swprofile.bin
make -C openaurora TARGET=tv BUILD=build-tv-v3/tab-policy \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/tab-policy/tests/tab_policy_firmware build-tv-v3/tab-policy/tests/profile \
  build-tv-v3/tab-policy/tests/ui_profile build-tv-v3/tab-policy/modules/mod_ui_profile.so \
  build-tv-v3/tab-policy/modules/mod_ticker.so
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
node artifacts/analysis/tab-context-2026-11-03/verify-loader.cjs
```

verify.cjs checks the local source identities and runs qemu with the ARM libc;
it now requires the saved software profile too. Success prints all three
reference groups: availability884736, selection9216 and hash65281/lookups27.
The optional reference harness is for offline qemu only, not deployment.
