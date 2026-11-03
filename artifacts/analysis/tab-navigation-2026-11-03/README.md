# Offline TabDialog review

Interpretation and remaining gates:
[TAB-NAVIGATION](../../reports/TAB-NAVIGATION-2026-11-03.md).
Text files retain fresh read-only Ghidra/ARM evidence and build/test output;
they do not record a live TV test. Source binary stays ignored/local.

From repository root:

```sh
make -C openaurora TARGET=tv BUILD=build-tv-v3/tab-policy \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/tab-policy/tests/tab_policy_firmware build-tv-v3/tab-policy/tests/tab_policy
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
make -C openaurora check-tab-policy
```

The firmware differential is optional ARM/qemu development tooling, excluded
from default host check and production module builds. Do not deploy it to the
TV. Exit77 means its private reference virtual addresses were unavailable;
never replace MAP_FIXED_NOREPLACE with a destructive fixed mapping.
