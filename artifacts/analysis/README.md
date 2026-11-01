# Raw analysis evidence

These logs are observations/decompiler output, not independently verified API
declarations. Use ../reports/README.md and ../../FINDINGS.md for current conclusions.

- osd-2026-10-31: palette/pan, kernel registration/callbacks/MAU, PWM, private
  GEM/pools, front/back surfaces and commit traces. Six older DRM contract logs
  originally referenced only under /tmp/opencode are now preserved here too.
- runtime-check-2026-10-31: old installed unresolved-symbol evidence, ARM/qemu
  fixtures, rebuilt real-TV check and after-state verification.
- module-reload-2026-10-31: state-aware reload/error regression fixtures, strict
  host suite, sanitizer and ARM/qemu reload/safety/registry logs; no TV run.
- module-phases-2026-10-31: dependency-order reproduction and regression fixtures,
  callback/CLI failure semantics, strict host/sanitizer/ARM-qemu logs; no TV run.
- control-replies-2026-10-31: ASan status heap-overflow reproduction, bounded
  serialization and full-table Unix socket reply tests; no TV run.
- control-transport-2026-10-31: nonblocking queue/backpressure, partial-write,
  disconnect, malformed-input and cleanup regressions; no TV run.
- power-2026-10-31: outer standby, idtv device/network stages, wake policy and
  monitor/UART synchronization and target ntkethmac policy/PM decompile plus
  instructions; no TV run.
- sleep-cycle-2026-10-31: internal final-entry/transition-ledger restoration,
  nesting/lifecycle guard and shutdown-cancellation regressions; no TV run.
- power-service-2026-10-31: host.sleep service/CLI integration, ordered pinned
  notifications, entry/restoration failures and cleanup with wrapped power I/O;
  host/sanitizer/ARM-qemu evidence, no physical standby or TV run.
- ui-animation-2026-10-31: controller/effect/pack-format instructions, 39-descriptor
  TSV, literal pools and exact full-pack host/sanitizer/ARM reader agreement.
  Extracted records are firmware evidence, not proof of native effect rendering.
- ui-resource-bindings-2026-10-31: all 702 resource IDs/names matched to index CRC32,
  named animation descriptors and widget/category state mapping. Advertising
  descriptors are explicitly excluded from implementation; other callers/policy
  still need review. Host/sanitizer/ARM name-reader agreement; no TV run.

- ui-clock-2026-10-31: FROST corrected-time/rollover instructions, constructor
  enablement and seven real-DSO clock/lifecycle groups on host/sanitizer/ARM-qemu.
  Clock prerequisite implemented; native animation controller remains pending.

- cec-audio-2026-10-31: scalar audio ABI/prelude/CEC transport instructions,
  nine wire/fake-HAL host/sanitizer/ARM-qemu groups, and relocation checks with
  empty /tmp. Transport integration and hardware acceptance remain open.

- cec-transport-2026-10-31: private V4L2/kernel CEC path, packet marshalling and
  four host/sanitizer/ARM-qemu groups, lost status/copy/filter/close/reopen hazards.
  Native transport ownership/integration still open; no TV run.

- cec-ownership-2026-11-01: PreInit emulation/actual cdev registration, authority
  and audio init/getter side effects; blocking raw backend with three injected-I/O
  host/sanitizer/ARM-qemu groups. Authority/workers/module/hardware integration open.

Ghidra saved function signatures often say void incorrectly. Some large-driver
imports have external relocation errors; do not treat those placeholders as
resolved destinations. Core dumps, pointers and physical addresses are specific
to the binary/process/session recorded. Separate ELF section offsets from Ghidra
addresses, especially .init.text. Diagnostic success is not hardware acceptance.

Keep logs unchanged for provenance. New interpretations belong in the relevant
report with source hash, function/instruction evidence, test scope and unresolved
questions; update the correction map rather than adding another competing plan.
