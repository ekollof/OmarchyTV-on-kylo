# UI clock evidence — 2026-10-31

Interpretation: [UI-ANIMATIONS](../../reports/UI-ANIMATIONS-2026-10-31.md).
All work was offline; no TV access, deployment or recovery changes.

- oa-animation-clock-enable.txt: read-only Aurora Ghidra instructions/decompile
  for controller constructor/enable flag, FROST_Time and FROST_TimeSubtract.
  Source Aurora MD5 64c252340d3af6c5d66072611b7bad7a.
- oa-ui-clock-arithmetic.txt: unsigned quotient/remainder helpers and TimeAdd.
  Saved void signatures are incorrect; use register/instruction behavior.
- oa-ui-clock-host.txt: complete strict host suite, including initial clock tests.
- oa-ui-clock-host-cases.txt: latest seven clock DSO regression groups.
- oa-ui-clock-sanitize.txt: clock, phase, power, reload and safety checks under
  ASan/UBSan; oa-ui-clock-sanitize-cases.txt reruns the final seven clock groups.
- oa-ui-clock-arm-build.txt / oa-ui-clock-arm-test-build.txt: ARM hard-float
  build and final test rebuild. oa-ui-clock-arm.txt: final seven groups run with
  qemu-arm-static -L /tmp/opencode/rootfs/root.

Latest ABI 2 ARM build: core MD5 5f071f85e21eb311ac15d65ed9d8efb7 (max
GLIBC2.17), ui_clock 4381b6a8c3befd90688c5bc3808e3ed7, ticker
d554a09d84fee5d887ccdd408f9a5fd6. Twelve modules built; this is not the historical
11-module real-TV preflight. Installed/recovery ABI 1 remains unchanged.

Arithmetic matches supplied sample traces. Absolute vendor clock phase/call
history, animation bootstrap callers and native controller/effect acceptance
remain open. The one-second idle sampling policy is our implementation choice.
