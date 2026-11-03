# Setting-list draw dispatcher evidence

Contract/limits: [SETTINGS-DRAW](../../reports/SETTINGS-DRAW-2026-11-03.md).
Firmware evidence is the prior read-only
`../svg-list-refresh-2026-11-03/parent-source.txt`; no need to repeat its Ghidra
run. The harness validates the complete2448-byte body, embedded branch table,
empty literal and OK resource name. No vendor bytes are embedded in source.

```sh
make -C openaurora TARGET=tv BUILD=build-tv-v3/settings-draw \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/settings-draw/tests/tab_policy_firmware \
  build-tv-v3/settings-draw/tests/settings_draw
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs \
  openaurora/build-tv-v3/settings-draw/tests/tab_policy_firmware
qemu-arm-static \
  -L artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/arm-none-linux-gnueabihf/libc \
  openaurora/build-tv-v3/settings-draw/tests/settings_draw
make -C openaurora all check check-ui-list-ipc
make -C openaurora BUILD=build-settings-draw-sanitize \
  CFLAGS='-std=c11 -O1 -g -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer' \
  LDFLAGS='-fsanitize=address,undefined' check-settings-draw
```

arm-build/arm-unit-build record fresh isolated builds; native records18441
cases plus every prior reference group. arm-unit/host-focused/sanitizer record
final units, including the added failure-after-type-writeback regression.
host-full records full host/actual Unix IPC before that final unit-only test
addition (implementation unchanged); final focused logs include it. All passed.
String/vector/resource/lower-widget primitives are controlled, not native
provider or rendering acceptance. No TV access.
