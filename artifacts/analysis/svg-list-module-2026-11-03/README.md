# SVG-list module evidence

Interpretation: [SVG-LIST-MODULE](../../reports/SVG-LIST-MODULE-2026-11-03.md).
First consumer of the verified SVG-list layers as a reloadable DSO service
with Unix socket/CLI control. No new firmware bytes; behavior is the
composition of previously verified contracts plus owned OAL1 state encoding.

```sh
make -C openaurora check-ui-list
make -C openaurora check-ui-list BUILD=build-list-sanitize \
  CC='ccache cc -fsanitize=address,undefined -fno-omit-frame-pointer' \
  CFLAGS='-std=c11 -O1 -g -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined' \
  LDFLAGS='-fsanitize=address,undefined'
make -C openaurora TARGET=tv BUILD=build-tv-v3/list-service \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/list-service/tests/ui_list build-tv-v3/list-service/modules/mod_ui_list.so \
  build-tv-v3/list-service/modules/mod_ticker.so
qemu-arm-static -L artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/arm-none-linux-gnueabihf/libc \
  openaurora/build-tv-v3/list-service/tests/ui_list openaurora/build-tv-v3/list-service/modules
make -C openaurora check-ui-list-ipc
```

Host, focused sanitizer, ARM/qemu and actual socket IPC pass. The IPC check
starts only ui_list (no display/input/HAL), drives keys over a real Unix
socket, verifies reload persistence, suspend EBUSY with queryable state,
suspended-reload ENODEV, resume, bad-arg EINVAL and SIGTERM clean exit with
the socket file removed. Widget getters/editing, parent refresh, scrollbar
construction/rendering, startSelectAnimation and visible parity remain open.
Binaries stay ignored and isolated under build*/.
