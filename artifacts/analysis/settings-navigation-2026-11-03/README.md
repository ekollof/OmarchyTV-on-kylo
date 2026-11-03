# Settings navigation evidence

Interpretation: [SETTINGS-NAVIGATION](../../reports/SETTINGS-NAVIGATION-2026-11-03.md).
navigation.txt contains the implemented method's read-only source/instructions.
list-keys.txt contains the next three widget methods, still unimplemented.
Trailing Ghidra log whitespace was removed. No TV/device access.

```sh
make -C openaurora check-settings-navigation
make -C openaurora TARGET=tv BUILD=build-tv-v3/tab-policy \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/tab-policy/tests/settings_navigation build-tv-v3/tab-policy/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The maintained source/profile-hash and fragment-CRC checked reference now
requires101384 navigation comparisons, plus every earlier group. Direct cursor
stores are observed as logical state writes; getters/widget actions are stubs,
not a full vendor UI object. host-check/sanitizer-check/arm-check record the
strict host and pure policy checks. Binaries remain ignored/isolated.
