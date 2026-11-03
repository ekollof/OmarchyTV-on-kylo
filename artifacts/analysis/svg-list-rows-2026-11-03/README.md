# SVG-list manager evidence

Interpretation: [SVG-LIST-ROWS](../../reports/SVG-LIST-ROWS-2026-11-03.md).
The six shipped fragments were read from the instruction cache
(artifacts/workbench/aurora.dis); no new decompile or TV query was needed.
Only guard-pointer literals are redirected in the native reference; updateImpl
and the row +28 focus virtual are controlled stubs, not vendor widgets.

```sh
make -C openaurora check-svg-list-rows
make -C openaurora TARGET=tv BUILD=build-tv-v3/tab-policy \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/tab-policy/tests/svg_list_rows build-tv-v3/tab-policy/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The maintained hash/CRC checked reference requires 324 native/C manager
comparisons (field layout, focus switch/store, partial forward), plus all
prior groups. Layout +12 first/+16 count/+20 selected-rel/+28 rows is
asserted by real native field reads, focus by ordered row (un)focus calls
and the stored rel, partial by the (index,false) forward. updateImpl, row
widget internals, row getters/editing, parent refresh, scrollbar/animation
bodies and the owned backend/consumer remain open. Binaries stay ignored and
isolated under build-tv-v3/tab-policy.
