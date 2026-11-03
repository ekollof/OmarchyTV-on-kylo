# Row refresh / checked-value evidence

See [SVG-LIST-REFRESH](../../reports/SVG-LIST-REFRESH-2026-11-03.md) for contracts
and limitations. source.txt is read-only Ghidra decompile/ARM evidence;
parent-source.txt identifies virtual+0x60 as SettingItemsListBox::drawRow and
saves its larger pending implementation. No hardware or TV query was used.

Reproduce from the workspace root:

```sh
make -C openaurora TARGET=tv BUILD=build-tv-v3/list-refresh \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/list-refresh/tests/svg_list_update \
  build-tv-v3/list-refresh/tests/svg_list_widgets \
  build-tv-v3/list-refresh/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs \
  openaurora/build-tv-v3/list-refresh/tests/tab_policy_firmware
make -C openaurora all check check-ui-list-ipc
make -C openaurora BUILD=build-list-refresh-sanitize \
  CFLAGS='-std=c11 -O1 -g -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer' \
  LDFLAGS='-fsanitize=address,undefined' check-svg-list-update check-svg-list-widgets
```

arm-build/native/host-focused/host-full/sanitizer/arm-update/arm-widgets logs
record the passing build, native body comparisons, host/IPC, sanitizer and
qemu unit checks respectively. Binary hashes/sizes are in the report. The
controlled primitives do not establish real parent/provider/rendering behavior.
