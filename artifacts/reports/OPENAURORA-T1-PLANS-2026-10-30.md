# T1 module plan — superseded planning record

> **Status: Superseded planning record. Follow current TODO/ARCHITECTURE/contracts, not its old probe or channel-sweep list.** See [report index](README.md) and
> [current corrections](../../FINDINGS.md); old plans do not authorize TV changes.

The original survey/opcode tables are preserved in
[the archive](../archive/docs-before-tidy-2026-10-31/reports/OPENAURORA-T1-PLANS-2026-10-30.md).
Use [TODO](../../TODO.md) and [ARCHITECTURE](../../openaurora/docs/ARCHITECTURE.md)
for the active order and acceptance gates. Recover exact firmware evidence before
reusing signatures or opcode payloads from the original plan.

Withdrawn assumptions that previously sent work in the wrong direction:

| old plan | current evidence |
|---|---|
| standard dumb-buffer display backend | CREATE_DUMB allocates 4 KiB; real OSD uses private GEM/ADDFB2/SETPLANE. OSD-BUFFER-CONTRACT. |
| power has no RE/lifecycle work left | Raw mem/WoL test does not coordinate native module suspend/resume. TODO-AUDIT. |
| all Hal_Vin functions void, despite Open returning fd | Current source declares int Hal_Vin_Open; saved prototypes need instruction review; complete routing not implemented. |
| source select/start is the whole switch | Companion VPLT, V4L2 info, audio routing/enumeration remain missing. |
| audio prelude/order and six opcodes close audio | Module init ownership/Protect_Init ABI conflict and audible acceptance remain open. |
| services all proven standalone | dialserver transport is separate; vendor Aurora still executes TV operations. |
| channel sweep is next panel step | Use recovered board/profile/call-chain evidence and PWM-CONTRACT-RECHECK. |
| 643 Hal / 160 NDAL functions | Unique defined dynamic T/W names are 321 / 80; counts don't prove APIs. |
| tests/selecting inputs are view-only | Mutations and UART consumption can disrupt an in-use TV. AGENTS governs authorization. |

No probe list from the old plan is a current instruction. Raw static call tables
remain useful navigation aids, not proof that a copied struct/opcode is safe.
