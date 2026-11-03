# SVG-list scrollbar bodies evidence

Interpretation: [SVG-LIST-SCROLLBAR](../../reports/SVG-LIST-SCROLLBAR-2026-11-03.md).
The two shipped bodies were read from the instruction cache
(artifacts/workbench/aurora.dis); no new decompile or TV query was needed.
Only guard-pointer literals are redirected in the native reference;
isCreated, renderBackground, renderBar and the common construction step are
controlled stubs, not a vendor UI.

```sh
make -C openaurora check-svg-list-scrollbar
make -C openaurora TARGET=tv BUILD=build-tv-v3/tab-policy \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/tab-policy/tests/svg_list_scrollbar build-tv-v3/tab-policy/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The maintained hash/CRC checked reference requires 294 native/C scrollbar
comparisons with exact call order/args and resulting bar state, plus all
prior groups. create forwards (created,rect,parent) unchanged;
setScrollPosition stores vis/size/first only when created and signed first
stays within signed vis, draws the background only with a clear +0xc8 byte,
then always draws the bar. A fragment-length accounting slip (line count
reused as byte count) was caught and corrected to next-symbol extents. Row
getters/editing, parent refresh, scrollbar construction/rendering internals,
startSelectAnimation and the owned backend/consumer remain open. Binaries
stay ignored and isolated under build-tv-v3/tab-policy.
