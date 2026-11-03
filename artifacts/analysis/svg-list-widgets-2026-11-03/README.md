# SVG-list widgets evidence

Interpretation: [SVG-LIST-WIDGETS](../../reports/SVG-LIST-WIDGETS-2026-11-03.md).
The two shipped bodies were read from the instruction cache
(artifacts/workbench/aurora.dis); no new decompile or TV query was needed.
Only guard-pointer literals are redirected in the native reference; the
embedded text entry getter, isCreated, selected-item and row/entry objects
are controlled fixtures, not a vendor UI. The +0x18d8 offset and the
+0x28c cell/array/entry/id chain execute verbatim against fixture state.

```sh
make -C openaurora check-svg-list-widgets
make -C openaurora TARGET=tv BUILD=build-tv-v3/tab-policy \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/tab-policy/tests/svg_list_widgets build-tv-v3/tab-policy/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The maintained hash/CRC checked reference requires 21 native/C widget
comparisons with exact call order/args and return values, plus all prior
groups. Numeric forwards the embedded entry handle; the selected id gates
on isCreated and (item+1)==0 before the cell/array/entry/id walk. A
+0x28c single-vs-double indirection misread was caught by the differential
(fault-PC capture) and corrected to cell-then-array. Subsequent
[SVG-LIST-SLIDE](../../reports/SVG-LIST-SLIDE-2026-11-03.md) implements
setSlideValue (progress vs slider/text routing, two flag bytes, type word,
+0x1498/+0x13c0 slider selection, +0x1ca8 text box, +0x2df8 store). Row
editing beyond setSlideValue (setChecked etc.), parent refresh, scrollbar
construction/rendering, startSelectAnimation and the owned backend/consumer
remain open. Binaries stay ignored and isolated under
build-tv-v3/tab-policy.
