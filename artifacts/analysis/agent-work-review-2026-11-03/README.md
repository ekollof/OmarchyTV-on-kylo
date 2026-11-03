# Subsequent-agent review evidence

Interpretation: ../../reports/AGENT-WORK-REVIEW-2026-11-03.md.
No TV access. Source ELF MD5 64c252340d3af6c5d66072611b7bad7a.

Fresh sequential DumpFirmware.java read-only/noanalysis queries:
- text-and-quick-gate.txt: --asm @10c78a0 @10c7828 @f31b80
- navigation-dispatch.txt: @d1aa78

```
node artifacts/analysis/agent-work-review-2026-11-03/verify-findings.cjs
make -C openaurora check-ui-object-scan
```

Before/after snapshot/prefix logs reproduce the original nonzero-base and
incomplete-string defects with synthetic fixtures. Original executable was
compiled from git e7e7333 with strict flags. Local fixtures:773-byte snapshot
with vptr153b904 at0, key pointer10000300 at+ac, MODE/NUL
at300; prefix case772 bytes, key pointer300, MODE without terminator. Workbench
keeps these binary fixtures and builds; only own text evidence is committed.

scanner-host/sanitize and arm-qemu logs cover key boundaries/errors, escaping,
snapshot mapping, permissions, raw UString metadata and prior span tests.
host-full-before/after are full-suite logs. ARM build and qemu use the durable
gcc-arm-10.2 glibc toolchain/sysroot, not emulated TV hardware. No native TV run
or target-loader/device acceptance is claimed.
