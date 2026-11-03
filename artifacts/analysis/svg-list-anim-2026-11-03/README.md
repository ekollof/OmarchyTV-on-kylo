# SVG-list startSelectAnimation evidence

Interpretation: [SVG-LIST-ANIM](../../reports/SVG-LIST-ANIM-2026-11-03.md).
The shipped body was read from the instruction cache
(artifacts/workbench/aurora.dis); no new decompile or TV query was needed.
Only the guard-pointer literal is redirected in the native reference;
getAnimationEnabled, selectedItem/getItemRow, the row getRect slot, the
standard-name/size/getAnimation calls, the animationController/startAnimation
pair, the five UString entry points and atof are controlled stubs, not a
vendor UI. Gates, rect words, the +0x24c/+0x25c snapshot, the +0x280
container, the +0x98 target, the +0x4 owner, the +12 name/+28 duration fields
and the 1000.0f scale execute verbatim against fixture state.

```sh
make -C openaurora check-svg-list-anim
make -C openaurora TARGET=tv BUILD=build-tv-v3/tab-policy \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/tab-policy/tests/svg_list_anim build-tv-v3/tab-policy/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The maintained hash/CRC checked reference requires 14 native/C anim
comparisons (gate/snapshot/search/start match). host-check.txt,
sanitizer-check.txt (ASan/UBSan), firmware-check.txt (full verify.cjs run)
and arm-build.txt (ARM unit under qemu) record the passes.
