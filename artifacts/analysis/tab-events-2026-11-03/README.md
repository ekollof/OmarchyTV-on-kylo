# Ordered tab handlers

Interpretation: [TAB-EVENTS](../../reports/TAB-EVENTS-2026-11-03.md).
The source evidence is reused from tab-writeback/content-lifecycle as linked in
the report. All new tests are offline; no Ghidra process or TV connection needed.

```sh
make -C openaurora check-tab-events
make -C openaurora TARGET=tv BUILD=build-tv-v3/tab-policy \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/tab-policy/tests/tab_events build-tv-v3/tab-policy/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The maintained hash-checked qemu reference now requires1008 tab-event traces
as well as all previous groups. Its private mappings/callback stubs are not a
full widget framework, display emulator or native deployment.
