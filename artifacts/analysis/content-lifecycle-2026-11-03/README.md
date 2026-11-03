# Hotel context and content header evidence

Interpretation:
[HOTEL-CONTEXT](../../reports/HOTEL-CONTEXT-2026-11-03.md) and
[CONTENT-HEADER-OWNERSHIP](../../reports/CONTENT-HEADER-OWNERSHIP-2026-11-03.md).
All source sessions are read-only/sequential. No TV connection or wake occurred.
show-path.txt's destroyComponents is the relevant ownership proof; its incidental
getVideoPreview function is not an experiment queue. db_tables is runtime BSS;
use its constructor binding, not a raw file read of an uninitialized pointer.

From repository root:

```sh
make -C openaurora check-settings-hotel check-settings-hotel-ipc check-tab-policy
node artifacts/analysis/content-lifecycle-2026-11-03/verify-binding.cjs
openaurora/build/tests/settings_hotel openaurora/build/modules artifacts/dumps/apdat_files/sqlite_db/settingsDB.db
make -C openaurora TARGET=tv BUILD=build-tv-v3/tab-policy \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/tab-policy/tests/settings_hotel build-tv-v3/tab-policy/modules/mod_settings.so \
  build-tv-v3/tab-policy/modules/mod_ticker.so build-tv-v3/tab-policy/tests/tab_policy_firmware
qemu-arm-static -L artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/arm-none-linux-gnueabihf/libc \
  -E LD_LIBRARY_PATH=openaurora/vendor/lib openaurora/build-tv-v3/tab-policy/tests/settings_hotel \
  openaurora/build-tv-v3/tab-policy/modules artifacts/dumps/apdat_files/sqlite_db/settingsDB.db
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
node artifacts/analysis/tab-context-2026-11-03/verify-loader.cjs
```

The maintained private-mapping reference now additionally requires345 hotel-row
cases. It is offline/qemu-only. The settings DSO test separately runs against
real SQLite and owns/removes its writable fixture; the saved DB is read-only.
