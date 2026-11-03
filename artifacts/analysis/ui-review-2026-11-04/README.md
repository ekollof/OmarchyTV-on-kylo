# UI review evidence

Interpretation: [UI-REVIEW](../../reports/UI-REVIEW-2026-11-04.md).
source/supplier-source are read-only Ghidra/ARM evidence. percent-before is the
expected local UBSan failure before the arithmetic fix, not a TV failure.
All other test logs are final passing checks. Native zero division is excluded.

```sh
make -C openaurora all check check-ui-list-ipc
UBSAN_OPTIONS=halt_on_error=1 make -C openaurora BUILD=build-ui-review-sanitize \
  CFLAGS='-std=c11 -O1 -g -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer' \
  LDFLAGS='-fsanitize=address,undefined' \
  check-settings-row-render check-settings-row-setters check-svg-image
make -C openaurora TARGET=tv BUILD=build-tv-v3/ui-review \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/ui-review/tests/tab_policy_firmware \
  build-tv-v3/ui-review/tests/svg_image \
  build-tv-v3/ui-review/tests/settings_row_render \
  build-tv-v3/ui-review/tests/settings_row_setters
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs \
  openaurora/build-tv-v3/ui-review/tests/tab_policy_firmware
```

arm-build/reference, arm-image/render/setters, sanitizer and host-full record
the isolated builds and passes. runtime contains symbol versions and identities;
native contains the complete reference result. tv-preflight/upload/identity/test/
cleanup record temporary staging, separate verification, real CPU tests and
removal/no-helper confirmation. The row/resource/raster callees are controlled;
this is not display or independent startup acceptance.
