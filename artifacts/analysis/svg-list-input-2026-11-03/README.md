# SVG-list input evidence

Interpretation: [SVG-LIST-INPUT](../../reports/SVG-LIST-INPUT-2026-11-03.md).
The exact source was already saved in ../settings-list-2026-11-03/lower-list.txt;
no new decompile/TV query was needed. Native references redirect guard literals
only and use controlled lower callees, not a vendor UI instance.

```sh
make -C openaurora check-svg-list-input
make -C openaurora TARGET=tv BUILD=build-tv-v3/tab-policy \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/tab-policy/tests/svg_list_input build-tv-v3/tab-policy/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The maintained source/profile-hash and fragment-CRC checked reference now
requires999216 SVG key cases,49344 message joins and68640 row-value cases plus
all prior groups. host-check/sanitizer-check/arm-check record the other strict
checks. Artifacts stay isolated/ignored under build-tv-v3/tab-policy. The owner
requested stopping after this verified, documented, committed/pushed item.
