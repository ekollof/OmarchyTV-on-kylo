# SVG-list scrollbar evidence

Interpretation: [SVG-LIST-SCROLL](../../reports/SVG-LIST-SCROLL-2026-11-03.md).
The shipped body was read from the instruction cache
(artifacts/workbench/aurora.dis); no new decompile or TV query was needed.
Only guard-pointer literals are redirected in the native reference; the +0x6c
total virtual, isCreated/isVisible/setVisible/create/setScrollPosition are
controlled stubs, not a vendor UI. Real RowManager size/first-visible bytes
execute against fixture fields; the EASIRect bytes are pinned by hash but
stubbed at runtime so the rect arguments are compared directly.

```sh
make -C openaurora check-svg-list-scroll
make -C openaurora TARGET=tv BUILD=build-tv-v3/tab-policy \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/tab-policy/tests/svg_list_scroll build-tv-v3/tab-policy/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The maintained hash/CRC checked reference requires 4320 native/C scrollbar
comparisons with exact gating/rect/show/position order and args, plus all
prior groups. Signed size-vs-total gating, the literal x/y/w/h formula with
wrapping arithmetic, conditional show and the hide path are asserted. A
stub-convention defect (r3-clobbering branch to 4-arg stubs) was caught by
the differential; multi-arg installers preserve all argument registers.
updateImpl, row widget internals, row getters/editing, parent refresh,
scrollbar construction/position bodies, startSelectAnimation and the owned
backend/consumer remain open. Binaries stay ignored and isolated under
build-tv-v3/tab-policy.
