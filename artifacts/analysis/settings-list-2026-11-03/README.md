# Settings list evidence

Interpretation: [SETTINGS-LIST](../../reports/SETTINGS-LIST-2026-11-03.md).
Implemented source is ../settings-navigation-2026-11-03/list-keys.txt.
lower-list.txt is a read-only Ghidra extract of the next three methods, still
unimplemented. Trailing log whitespace was removed. No TV access.

```sh
make -C openaurora check-settings-list
make -C openaurora TARGET=tv BUILD=build-tv-v3/tab-policy \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/tab-policy/tests/settings_list_input build-tv-v3/tab-policy/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The maintained hash/CRC checked reference additionally requires65600 map and
138240 body cases, plus all prior groups. Direct record stores are observed
after return; lower helpers are controlled fixtures, not a full vendor UI.
Host/full, focused sanitizer and ARM logs record the other checks. Binaries
remain ignored and isolated under build-tv-v3/tab-policy.
