# SVG-list rowview/update evidence

Interpretation: [SVG-LIST-UPDATE](../../reports/SVG-LIST-UPDATE-2026-11-03.md).
The four shipped bodies were read from the instruction cache
(artifacts/workbench/aurora.dis); no new decompile or TV query was needed.
Only guard-pointer literals are redirected in the native reference; the
+0x6c total virtual, updateFull, updateScrollBar and startSelectAnimation
are controlled stubs, not a vendor UI. Real getFirstVisible bytes execute
verbatim against fixture fields; the rowview getters execute verbatim
against a fixture row array.

```sh
make -C openaurora check-svg-list-update
make -C openaurora TARGET=tv BUILD=build-tv-v3/tab-policy \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/tab-policy/tests/svg_list_update build-tv-v3/tab-policy/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The maintained hash/CRC checked reference requires 100 native/C rowview
comparisons (slot arithmetic, wrapping last-visible, item bounds/handles)
and 20 native/C update comparisons (virtual total, stored count word,
first/total refresh operands, scrollbar/animation order), plus all prior
groups. Store and first-visible reads are C-side only; their effects are
asserted through the stored word and captured operands. A harness defect
(direct calls through memcpy-initialized fn-pointer locals miscompiled at
-O2) was caught; entry-point calls now go through a volatile pointer.
Widget-level row getters/editing, parent refresh, scrollbar
construction/rendering internals, startSelectAnimation and the owned
backend/consumer remain open. Binaries stay ignored and isolated under
build-tv-v3/tab-policy.
