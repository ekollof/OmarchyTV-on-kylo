# Settings-row renderItems (0/1/4 paths) evidence

Interpretation: [SETTINGS-ROW-RENDER](../../reports/SETTINGS-ROW-RENDER-2026-11-03.md).
The shipped body was read from the ELF with objdump (`render_objdump.txt`
method, saved ASM verified instruction by instruction); the saved Ghidra
`--asm` dump contains phantom instructions past the switch head and was
abandoned as a source. No new decompile or TV query was needed. Only the
guard-pointer literal is redirected in the native reference; the base render,
UString temporaries, getRectangle, isCreated, setSVGName, focus/elipses/font,
setWindowRect, setText, both creates and the EASIRect copy/offset/default
helpers are controlled stubs or verbatim-copied firmware math, not vendor
implementations. Child offsets, setSVGName slots/IDs, the 0x1b immediate
default, the switch bound (type-2 > 0x18) and the jump table at 0xd5454c
(table[2] = 0xd544a4, read from the ELF) all execute verbatim against
fixture state.

```sh
make -C openaurora check-settings-row-render
make -C openaurora TARGET=tv BUILD=build-tv-v3/settings-row-render \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/settings-row-render/tests/settings_row_render build-tv-v3/settings-row-render/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The maintained hash/CRC checked reference requires 2264 native/C render
comparisons (0/1/4 paths, shared tail, gates, defaults). host-check.txt,
sanitizer-check.txt (ASan/UBSan), arm-unit.txt (ARM unit under qemu),
firmware-check.txt (full verify.cjs run) and tv-test.txt (focused real-TV
CPU, exit 0, staging removed, no helper) record the passes.
