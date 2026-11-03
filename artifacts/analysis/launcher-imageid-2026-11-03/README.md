# LauncherHelper::getImageId evidence

Interpretation: [LAUNCHER-IMAGEID](../../reports/LAUNCHER-IMAGEID-2026-11-03.md).
The shipped body at 0xe4aaf4 is copied verbatim (792 bytes, CRC 0xa4caa578)
into a fixed mapping and called for every operation id (plus out-of-range
values) across seven widget states. The only literal it reads is the stack
guard at 0x1791d68, which is backed by an owned page with a fixed canary.
No new decompile or TV query was needed.

```sh
make -C openaurora check-launcher-image-id
make -C openaurora TARGET=tv BUILD=build-tv-v3/settings-row-render \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/settings-row-render/tests/settings_row_render build-tv-v3/settings-row-render/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The differential compares 469 op/state pairs; host-check.txt also records the
render ops hook wired to the real mapping. sanitizer-check.txt (ASan/UBSan),
firmware-check.txt (full verify.cjs), arm-firmware.txt (the 469-comparison
group with the 886060 render group) and tv-test.txt (focused real-TV CPU,
exit 0, staging removed, no helper) record the passes.
