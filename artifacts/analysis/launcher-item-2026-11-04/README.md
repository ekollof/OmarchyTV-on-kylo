# LauncherItem resource conversion evidence

Interpretation: [LAUNCHER-ITEM](../../reports/LAUNCHER-ITEM-2026-11-04.md).
The shipped Thumb body at 0xe4794c (500 bytes, CRC 0xd1bcb8c5) is copied
verbatim at its original address. The literal pool sits just past the copied
range, so the canary pointer, a non-zero gthread probe and the two path strings
are written into the mapped page. UString constructors/isEmpty/assign/append
and the shared_count destructor are stubs; both the native trampolines and the
owned ops call the same trace helpers.

8 native/C cases: source pattern {A,B} x thumbnail name empty {0,1} x control
block {none,present}. Each compares the ordered call trace, the 164-byte
output (plus untouched bytes to 256) and the control-block refcount.

```sh
make -C openaurora check-launcher-item
make -C openaurora TARGET=tv BUILD=build-tv-v3/settings-row-render \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/settings-row-render/tests/launcher_item build-tv-v3/settings-row-render/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

host-check.txt, sanitizer-check.txt (ASan/UBSan), arm-unit.txt (ARM unit under
qemu), firmware-check.txt (full verify.cjs, 38 ok lines) and tv-test.txt
(focused real-TV CPU, exit 0, staging removed, no helper; the 32-bit refcount
assertion runs there, unlike the 64-bit host unit) record the passes.
