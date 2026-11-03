# SVG-list move evidence

Interpretation: [SVG-LIST-MOVE](../../reports/SVG-LIST-MOVE-2026-11-03.md).
The eight shipped primitives were read from the instruction cache
(artifacts/workbench/aurora.dis); no new decompile or TV query was needed.
Only guard-pointer literals are redirected in the native reference; RowManager
(size/selectedItem/getFirstVisibleItem/focusItem), updatePartial,
updateScrollBar and startSelectAnimation callees are controlled stubs, not a
vendor UI instance.

```sh
make -C openaurora check-svg-list-move
make -C openaurora TARGET=tv BUILD=build-tv-v3/tab-policy \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/tab-policy/tests/svg_list_move build-tv-v3/tab-policy/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The maintained hash/CRC checked reference requires 5040 native/C move
comparisons with exact call order/args, plus all prior groups. Window
up/down shift without animating; only to-top/to-bottom animate. The receiver
split (list box versus this+0xf8 manager) is asserted by the differential.
Primitive move-target implementations, RowManager internals, row getters/
editing, parent refresh and the owned backend/consumer remain open. Binaries
stay ignored and isolated under build-tv-v3/tab-policy.
