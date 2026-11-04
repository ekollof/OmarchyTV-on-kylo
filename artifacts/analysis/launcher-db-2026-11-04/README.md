# settingsDB LauncherItem read-path evidence

Interpretation: [LAUNCHER-DB](../../reports/LAUNCHER-DB-2026-11-04.md).

- Shipped row decode `copyLauncherInfoToItem` 0xfac334 (0xf4 bytes, CRC
  0xa089d47e) copied verbatim; 8 native/C cases (varied scalars/text) compare
  the 0xa0 item and the ten UString assignment destinations/sources.
- Member map recovered from `launcher_item_info_struct` 0xfabf50: 16 scalar
  and 10 text columns (sizes 300/60/150); the dumped DB blobs are exactly
  those sizes.
- Adapter `common/launcher_db.c` reads the real table read-only; provider
  `getLauncherItems` 0x107a0c4 is a vtable forward, so the adapter and
  provider selection logic are one C path.

```sh
make -C openaurora check-launcher-item check-launcher-db check-launcher-db-module
make -C openaurora TARGET=tv BUILD=build-tv-v3/settings-row-render \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/settings-row-render/tests/launcher_item build-tv-v3/settings-row-render/tests/launcher_db build-tv-v3/settings-row-render/tests/tab_policy_firmware
make -C openaurora TARGET=tv BUILD=build-tv-v3/lcdb-module \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/lcdb-module/tests/launcher_db_module build-tv-v3/lcdb-module/modules/mod_settings.so
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

host-db.txt, host-module.txt, sanitizer-db.txt/sanitizer-item.txt (ASan/UBSan),
arm-db.txt/arm-item.txt/arm-module.txt (ARM under qemu), firmware-check.txt
(verify.cjs, 39 ok lines), arm-firmware.txt, tv-preflight.txt, tv-upload.txt
(staged hashes), tv-tests.txt (four TV runs incl. the live probe:
all=54 parent0=27 parent1=24) and tv-cleanup.txt record the passes. The TV was
asleep and tvssh.sh sent the owner-preferred WoL before the read-only runs.
