# Tab dispatch evidence

Interpretation and limits: [TAB-DISPATCH](../../reports/TAB-DISPATCH-2026-11-03.md).
source.txt and matcher.txt are read-only Ghidra extracts; trailing log whitespace
was removed. The method/table and ARM/qemu tests use the existing local ELF.
No TV access, initialization, staging or deployment.

```sh
make -C openaurora check-tab-dispatch
make -C openaurora TARGET=tv BUILD=build-tv-v3/tab-policy \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/tab-policy/tests/tab_dispatch build-tv-v3/tab-policy/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The maintained hash-checked driver requires65600 map/matcher comparisons and
16384 pre/fallback joins, plus all older reference groups. Join pre-handling uses
our separately verified C method as the source caller's virtual implementation;
the pure map uses real shipped map and matcher with controlled lower handlers.
host-check.txt, sanitizer-check.txt and arm-check.txt record the other checks.
