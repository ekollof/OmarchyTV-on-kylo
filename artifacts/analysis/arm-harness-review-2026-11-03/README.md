# ARM harness review evidence

Interpretation and limits: [ARM-HARNESS-REVIEW](../../reports/ARM-HARNESS-REVIEW-2026-11-03.md).
crash-record.txt is a bounded historical coredumpctl record (no core available).
build-before/reproduce record the initially passing14-case current binary;
they are historical observations, not the final acceptance gate.
Read-only Ghidra/objdump extracts establish animation/string contracts.
gdb-check confirms host ARM ELF support; toolchain-gdb-check records the missing
legacy ncurses dependency. No GDB core or reproduced historical SIGSEGV exists.

Reproduce the final isolated build (no make clean/make tv):

```sh
make -C openaurora TARGET=tv BUILD=build-tv-v3/arm-review \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/arm-review/tests/svg_list_anim \
  build-tv-v3/arm-review/tests/svg_list_widgets \
  build-tv-v3/arm-review/tests/tab_policy_firmware \
  build-tv-v3/arm-review/tests/ui_list \
  build-tv-v3/arm-review/modules/mod_ui_list.so \
  build-tv-v3/arm-review/modules/mod_ticker.so
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs \
  openaurora/build-tv-v3/arm-review/tests/tab_policy_firmware
make -C openaurora all check check-ui-list-ipc
```

fresh-build/native-final/arm-unit-final/arm-widgets-final/arm-list-final record
the final ARM passes. host-final/sanitizer-final record full host/Unix IPC and
focused sanitized animation/widgets/service checks. list-restore-before is a
safe nonzero-exit regression showing malformed restore acceptance (no deliberate
crash); list-restore-after confirms rejection/atomicity and real IPC success.
The native animation corpus requires153 cases; helper-string semantics are a
bounded valid-UTF8 fixture, not a complete native UString oracle. No TV access.
