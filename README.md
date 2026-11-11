# tvhack / openaurora

Vestel MB230 / Telefunken D65U546A, firmware 0.51.52.3. The target is a
bug-for-bug compatible, modular Aurora replacement with socket/CLI module reload,
followed by a native OmarchyTV front end. Telemetry and advertising are excluded;
the remaining TV features stay in scope.

Start with [AGENTS.md](AGENTS.md) for rules, [MEMORY.md](MEMORY.md) for the last
verified TV/build state, and [TODO.md](TODO.md) for work. [HANDOFF.md](HANDOFF.md)
contains resumption pointers, not another independent status history.

- [Current findings and corrections](FINDINGS.md): conclusions, limits and known red herrings.
- [Report index](artifacts/reports/README.md): which reports are current evidence,
  historical experiments or superseded plans.
- [Reverse-engineering docs](docs/reverse-engineering/README.md): binary/hash
  inventory, recovered subsystem contracts, tooling recipes and a topic map of
  the 336 reports.
- [Reproduce on your own TV](docs/reproduce/README.md): owner root/access
  bootstrap, dependency sources, NAND dump/extract and recovery.
- [Artifact map](artifacts/README.md): provenance, recovery image identities and raw evidence.
- [Helper roles](tools/README.md): queries, UI mutations and offline simulation.
- [Architecture](openaurora/docs/ARCHITECTURE.md), [runtime preflight](openaurora/docs/RUNTIME-CHECK.md),
  [native recovery supervisor](openaurora/docs/RECOVERY.md).
- [Preserved pre-cleanup documentation](artifacts/archive/README.md): chronology only.

The TV is in use. View-only queries are authorized; no takeover or deployment.
The replacement is incomplete. Passing loader checks, framebuffer readback and
historical on-panel paint do not prove independent startup or complete parity.
