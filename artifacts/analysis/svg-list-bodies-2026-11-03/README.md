# SVG-list bodies evidence

Interpretation: [SVG-LIST-BODIES](../../reports/SVG-LIST-BODIES-2026-11-03.md).
The seven shipped bodies were read from the instruction cache
(artifacts/workbench/aurora.dis); no new decompile or TV query was needed.
Only guard-pointer literals are redirected in the native reference; lower
move/window/notify/parent callees are controlled stubs, not a vendor UI
instance.

```sh
make -C openaurora check-svg-list-bodies
make -C openaurora TARGET=tv BUILD=build-tv-v3/tab-policy \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/tab-policy/tests/svg_list_bodies build-tv-v3/tab-policy/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The maintained hash/CRC checked reference requires 26048 native/C body
comparisons with exact call order/args and handled truth, plus all prior
groups. A native differential caught a real page-down scroll/wrap inversion
in the first implementation; the shipped direction is now encoded. The
differential also proved movement/window/animation primitives run on the
list box while only manager queries and focusItem run on this+0xf8.
Primitive move/selection/window/notify implementations, row getters/editing,
parent refresh and the owned backend/consumer remain open. Binaries stay
ignored and isolated under build-tv-v3/tab-policy.
