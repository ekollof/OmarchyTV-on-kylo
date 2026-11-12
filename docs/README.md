# Project documentation

This directory is a navigation layer over the repository's evidence and
implementation documents. It does not replace the canonical artifacts; it
points at them by topic.

## Read first

- [AGENTS.md](../AGENTS.md) — standing owner constraints and working rules.
- [TODO.md](../TODO.md) — pending work and acceptance gates.
- [FINDINGS.md](../FINDINGS.md) — correction map: what is established, what is
  unknown, and superseded claims.
- [MEMORY.md](../MEMORY.md) — last verified TV/deployment/build state.
- [HANDOFF.md](../HANDOFF.md) — entry points and current handoff.

## Reverse engineering

- [Reverse-engineering overview](reverse-engineering/README.md) — evidence
  rules, artifact map and quick lookup.
- [Reproduce on another TV](reproduce/README.md) — owner bootstrap (unsigned
  `/conf` hook), dependency sources, NAND dump/extract, build and restore.
- [Binaries and hashes](reverse-engineering/binaries.md) — every key firmware
  binary/module, MD5, Ghidra project and role.
- [Subsystems](reverse-engineering/subsystems.md) — recovered contracts by
  area, with addresses and report pointers.
- [Tooling and recipes](reverse-engineering/tooling.md) — Ghidra headless,
  extraction, objdump cache, search commands and ABI caveats.
- [Report map](reverse-engineering/report-map.md) — reports grouped by topic;
  the full chronological index is
  [artifacts/reports/README.md](../artifacts/reports/README.md).
- [Glossary](reverse-engineering/glossary.md) — vendor/ABI acronyms.
- [Open questions](reverse-engineering/open-questions.md) — unresolved
  contracts and the reports/TODO entries that own them.

## Implementation and operations

- [openaurora architecture](../openaurora/docs/ARCHITECTURE.md) — module model,
  dispatch, lifecycle, vendor-library reuse and build targets.
- [Lua skinning](../openaurora/docs/LUA.md),
  [recovery](../openaurora/docs/RECOVERY.md),
  [runtime check](../openaurora/docs/RUNTIME-CHECK.md).
- [Artifact map and provenance](../artifacts/README.md) — dump/extract layout,
  verified identities, recovery pair, no-redistribution rules.
- [Report index](../artifacts/reports/README.md) — 341 dated topic reports.
