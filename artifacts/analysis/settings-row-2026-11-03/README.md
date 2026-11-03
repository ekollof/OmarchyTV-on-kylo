# Setting-row type evidence

See [SETTINGS-ROW-TYPE](../../reports/SETTINGS-ROW-TYPE-2026-11-03.md).
source.txt contains read-only Ghidra/ARM for setType plus pending label,
progress and range setters. render-source.txt saves the actual pending
renderItems and its rebuild-flag reads; do not repeat these extracts by default.

native-layout.txt is the initial28-type ordered visibility extraction under
controlled callees, before the C comparison existed. It is historical discovery
evidence, not the final acceptance run. native-final.txt is the final12480-case
differential plus all previous groups. The maintained harness optionally prints
the same layout with OA_DUMP_ROW_VISIBILITY=1; normal tests do not need it.

```sh
make -C openaurora TARGET=tv BUILD=build-tv-v3/settings-row \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/settings-row/tests/tab_policy_firmware \
  build-tv-v3/settings-row/tests/settings_row_type
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs \
  openaurora/build-tv-v3/settings-row/tests/tab_policy_firmware
qemu-arm-static \
  -L artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/arm-none-linux-gnueabihf/libc \
  openaurora/build-tv-v3/settings-row/tests/settings_row_type
make -C openaurora all check check-ui-list-ipc
make -C openaurora BUILD=build-settings-row-sanitize \
  CFLAGS='-std=c11 -O1 -g -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer' \
  LDFLAGS='-fsanitize=address,undefined' check-settings-row-type
```

arm-build/arm-unit/host-focused/host-full/sanitizer/runtime record completed
checks and ELF identities. tv-preflight/upload/identity/test/cleanup record
temporary staging, separate verification, native CPU execution and removal.
No helper remains; no renderer or hardware operation was exercised.
