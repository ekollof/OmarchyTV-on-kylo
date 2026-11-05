# Review evidence — 2026-11-05

Read ../../reports/AGENT-REVIEW-2026-11-05.md for scope and dispositions.
full-host.txt is the final fresh strict make check; control-host.txt is a focused
run. control-sanitizer/decoder-sanitizer logs are leak-enabled ASan/UBSan.
decoder-arm.txt includes the actual production DSO on every vendor input, not
only external libraries. control-arm.txt is the actual ARM/qemu bounds unit.
grid-firmware.txt is fresh readOnly/noanalysis Ghidra output. Raw Ghidra logs
retain emitted trailing spaces. No firmware binaries or image inputs committed.

TV logs separate before/upload/verified/tests/after. tv-manifest and tv-sizes
are the independently checked staged identities, not installed artifacts.
qemu-raw-sha256 matches the TV foxxum results. corpus-sha256 identifies the
private files now preserved under artifacts/workbench/decoder-corpus.

Reproduce offline from the repository root:

```sh
make -C openaurora BUILD=build/agent-review-2026-11-05 -j4 check
make -C openaurora TARGET=tv BUILD=build-tv-v3/agent-review-2026-11-05 \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  check-vendor-decoder
```
