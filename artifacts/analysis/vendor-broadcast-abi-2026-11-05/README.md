# Vendor broadcast ABI grounding evidence

Interpretation: [VENDOR-BROADCAST-ABI](../../reports/VENDOR-BROADCAST-ABI-2026-11-05.md).

- `check.txt`: full host suite including the new EIT codec/range test.
- `full-host-summary.txt`: host **414 ok**, `verify.cjs` **45 groups**.
- `arm-build.txt`: cross-build of `build-tv-v3/tests/eit_time`.
- `arm-run.txt`: the same test executed under qemu-arm-static.
- `identities.txt`: host/ARM test hashes and the analysed `aurora.elf` hash.

No TV operation or vendor execution is involved. The report records the
decompiled EIT/tuner/PVR interfaces and the contract deltas.
