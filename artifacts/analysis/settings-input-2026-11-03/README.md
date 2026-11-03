# Settings input evidence

Interpretation: [SETTINGS-INPUT](../../reports/SETTINGS-INPUT-2026-11-03.md).
handlers.txt contains read-only Ghidra method extracts; trailing log whitespace
was removed. The base map was already saved in ../tab-leaf-2026-11-03/base.txt.
verify-metadata.cjs checks local ELF identity and bounded token/vtable reads.
No TV/device access, initialization or deployment.

```sh
make -C openaurora check-settings-input
node artifacts/analysis/settings-input-2026-11-03/verify-metadata.cjs
make -C openaurora TARGET=tv BUILD=build-tv-v3/tab-policy \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/tab-policy/tests/settings_input build-tv-v3/tab-policy/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The maintained ELF/profile-hash and fragment-CRC checked reference additionally
requires65600 settings map routes and86400 input-method cases. The native
map/matcher and input bodies execute with controlled lower callees, not a
complete vendor UI object. host-check/sanitizer-check/arm-check record the other
checks. Native artifacts remain isolated under ignored build-tv-v3/tab-policy.
