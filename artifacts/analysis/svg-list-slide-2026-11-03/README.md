# SVG-list row editing evidence

Interpretation: [SVG-LIST-SLIDE](../../reports/SVG-LIST-SLIDE-2026-11-03.md).
The shipped body was read from the instruction cache
(artifacts/workbench/aurora.dis); no new decompile or TV query was needed.
Only guard-pointer literals are redirected in the native reference;
setProgressValue, isCreated, setSliderValue, the UString
constructor/append/destructor and textbox setText are controlled stubs, not
a vendor UI. Flag bytes, the type word, the slider/textbox/row offsets and
the store word execute verbatim against fixture state.

```sh
make -C openaurora check-svg-list-widgets
make -C openaurora TARGET=tv BUILD=build-tv-v3/tab-policy \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/tab-policy/tests/svg_list_widgets build-tv-v3/tab-policy/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The maintained hash/CRC checked reference requires 72 native/C slide
comparisons with exact call order/args, written values and stored words,
plus all prior groups. Progress vs slider/text routing, the two flag bytes,
the type word, the +0x1498/+0x13c0 slider selection, the +0x1ca8 text box,
the empty UString format and the +0x2df8 store are asserted; the native
UString stack buffer is a transport temporary and is not compared, and
progress paths store nothing. A page-size accounting slip (272-byte fragment
over a 4K page end), a void 2-arg stub-convention gap and a progress-path
stale-store comparison were caught and fixed. Parent refresh, scrollbar
construction/rendering, startSelectAnimation and the owned backend/consumer
remain open. Binaries stay ignored and isolated under build-tv-v3/tab-policy.
