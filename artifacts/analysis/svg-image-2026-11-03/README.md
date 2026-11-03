# SVGImage create/destroy evidence

Interpretation: [SVG-IMAGE](../../reports/SVG-IMAGE-2026-11-03.md).
The shipped SVGImage::create (0x10c9638, 136 bytes, CRC 0xbdf31b8d) and
SVGImage::destroy (0x10c8ef4, 60 bytes, CRC 0xcb48ce8f) bodies are copied
verbatim to their own addresses so their PC-relative calls hit stubs installed
in place. EASIRect::offset (0xd2af88, 74 bytes, CRC 0xbc62c27a) is copied too,
so the parent client-rect offset is the shipped arithmetic. The image
supplier's virtual call goes through a fake object whose vtable slot 2 points
at a register-preserving stub. No new decompile or TV query was needed.

```sh
make -C openaurora check-svg-image
make -C openaurora TARGET=tv BUILD=build-tv-v3/settings-row-render \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/settings-row-render/tests/svg_image build-tv-v3/settings-row-render/tests/settings_row_render build-tv-v3/settings-row-render/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

host-check.txt, sanitizer-check.txt (ASan/UBSan), arm-unit.txt (ARM unit under
qemu), firmware-check.txt (full verify.cjs, 35 ok lines) and tv-test.txt
(focused real-TV CPU of the ARM unit, exit 0, staging removed, no helper)
record the passes.
