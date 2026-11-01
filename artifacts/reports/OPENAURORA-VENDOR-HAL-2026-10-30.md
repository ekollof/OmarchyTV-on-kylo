# Vendor HAL reuse — original survey, corrected 2026-10-31

> **Status: Superseded initial design. Current direct-link strategy/closure is in ARCHITECTURE; PWM-CONTRACT-RECHECK owns the PWM ABI.** See [report index](README.md) and
> [current corrections](../../FINDINGS.md); old plans do not authorize TV changes.

**Superseded design.** Original text is preserved in
[the documentation archive](../archive/docs-before-tidy-2026-10-31/reports/OPENAURORA-VENDOR-HAL-2026-10-30.md).
Use [ARCHITECTURE](../../openaurora/docs/ARCHITECTURE.md) for implementation and
[PWM-CONTRACT-RECHECK](PWM-CONTRACT-RECHECK-2026-10-31.md) for PWM declarations.

The valid decision is to reuse this TV's fixed vendor libraries, with verified
ABI/init ownership, rather than reconstruct device drivers from scratch.
PLAT/DEV/ICE implementations are inside Aurora, not exported library APIs.

Corrections to the initial plan:

- TV modules directly link the closure in Makefile/fetch-vendor.sh; host/musl
  builds use OA_NO_VENDOR stubs. Do not revive per-panel dlopen fallback.
- Unique defined dynamic T/W names recount as 321 Hal_* in libNDAL3 and 80
  NDAL_* in libNDAL_EX, not the older 121/643/160 figures.
- NDAL_EX data and V4L2 imports need the complete closure. RTLD_LAZY alone does
  not establish loadability; rebuilt files pass --check on real TV.
- PWM Open consumes six words, Set four, with scalar returns; do not copy the
  old five-/three-word pseudocode or saved void prototypes.
- Board PWM constants/call chain are already recovered. Do not repeat blind
  channel sweeps; independent brightness/power and ownership remain unverified.
- Existing audio glue does not prove complete amp/DSP routing. Vendor code
  reuse still requires tracing caller semantics and observing actual effects.

Raw firmware and libraries stay local; do not redistribute them.
