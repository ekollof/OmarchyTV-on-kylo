# SVG-list implementation evidence

Interpretation: [SVG-LIST-IMPL](../../reports/SVG-LIST-IMPL-2026-11-03.md).
The two shipped bodies were read from the instruction cache
(artifacts/workbench/aurora.dis); no new decompile or TV query was needed.
Only guard-pointer literals are redirected in the native reference; the row
+0x1C/+0x28/+0x2C/+0x34 and owner +0x58/+0x5C/+0x60 virtuals plus UI
isVisible/gotFocus are controlled stubs, not vendor widgets. Real manager
field reads, row-array indexing and the rotate/update control flow execute
verbatim against fixture state.

```sh
make -C openaurora check-svg-list-impl
make -C openaurora TARGET=tv BUILD=build-tv-v3/tab-policy \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/tab-policy/tests/svg_list_impl build-tv-v3/tab-policy/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The maintained hash/CRC checked reference requires 6952 native/C
comparisons with exact call order/args plus resulting manager/array/selection
state, and all prior groups. Start clamping, unfocus gating, the full/partial
distinction, per-row show/update/draw, hide sweeps, writeback/clamp/refocus
and the empty-message path are asserted, as is the rotate pair-swap with
full position exchange. A harness page-size accounting slip (executable
region shorter than the mapped stub) and an r3-clobbering branch to 4-arg
stubs were caught and fixed; the +0x28/+0x1C slot reading is corrected at
source (row focus is +0x1C setFocus; +0x28 is sret getWindowRect). Row
getters and editing, parent refresh, construction/position bodies,
startSelectAnimation and the owned backend/consumer remain open. Binaries
stay ignored and isolated under build-tv-v3/tab-policy.
