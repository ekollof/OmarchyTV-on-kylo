# SVG widget-state evidence

Interpretation: [SVG-WIDGET-STATE](../../reports/SVG-WIDGET-STATE-2026-11-04.md).
Four shipped Thumb bodies are copied verbatim at their original addresses and
called against the owned C:

- SVGWindow::setFocus 0xeeebd4 (92 bytes, CRC 0x085d6c76)
- SVGWindow::setActive 0xeee4ec (84 bytes, CRC 0x7b1ef395)
- SVGTextBox::setFocus 0x10c8a28 (80 bytes, CRC 0x117744b9)
- SVGTextBox::setElipses 0x10c6f48 (60 bytes, CRC 0x1606bd56)

Stack-guard literals are redirected; UIMessageTarget::setFocus, isCreated,
drawTextBox and performTextSlideAnimation are stubs. 36 native/C cases:
16 window-focus/active, 16 textbox-focus (states x argument x created), 4
elipses (mode 9/5 x argument). Every case compares the ordered call trace and
the resulting state word/byte after both objects are filled with 0xAA.

```sh
make -C openaurora check-svg-widget-state
make -C openaurora TARGET=tv BUILD=build-tv-v3/settings-row-render \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/settings-row-render/tests/svg_widget_state build-tv-v3/settings-row-render/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

host-check.txt, sanitizer-check.txt (ASan/UBSan), arm-unit.txt (ARM unit under
qemu), firmware-check.txt (full verify.cjs, 37 ok lines) and tv-test.txt
(focused real-TV CPU, exit 0, staging removed, no helper) record the passes.
All four bodies are Thumb; force-thumb is required for disassembly.
