# Owned settings model evidence

Interpretation: [SETTINGS-MODEL](../../reports/SETTINGS-MODEL-2026-11-03.md).
text-source/equality-source/format-source contain read-only Ghidra/ARM evidence.
The void-looking decompiler empty/equality results are corrected by instructions.

```sh
make -C openaurora all check check-ui-list-ipc
make -C openaurora BUILD=build-settings-model-sanitize \
  CFLAGS='-std=c11 -O1 -g -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer' \
  LDFLAGS='-fsanitize=address,undefined' check-settings-model
make -C openaurora TARGET=tv BUILD=build-tv-v3/settings-model \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/settings-model/tests/settings_model
qemu-arm-static \
  -L artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/arm-none-linux-gnueabihf/libc \
  openaurora/build-tv-v3/settings-model/tests/settings_model
```

host-focused/host-full/sanitizer/arm-build/arm-unit record final passing checks.
runtime records actual ELF interpreter, GLIBC versions and size/hash. tv-preflight,
tv-upload, tv-identity, tv-test and tv-cleanup record the separate staging,
identity check, native CPU execution and cleanup. No helper or staging remains.
The TV run used real owned providers and a CPU event recorder, not a renderer;
there was no device operation or screen change.
