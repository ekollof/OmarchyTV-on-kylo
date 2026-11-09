# Historical documentation archive

build-tv-v3-before-mixer-20261101.tar preserves the previous ABI3 core
fc9eb71f924dd850c7ba4f1c9b8a02cd, 13 modules, supervisor, old stage.md5 and its
two test fixtures. Every manifest entry verified after archiving; the adjacent
.tar.sha256 identifies it. Current build-tv-v3 is the refreshed16-module stage
documented in MIXER-MODULE-2026-11-01. This archive is not a deployment baseline.

The docs-before-tidy-2026-10-31 directory preserves the top-level handoff/log/
checklist/instructions, dated reports and selected implementation docs as they were
before the documentation cleanup. **These are historical text, not active agent
instructions, current state, or an executable task list.** Old “settled facts,”
“pick up here,” checkmarks and “next” paragraphs can contradict later evidence.

Use ../../AGENTS.md, ../../MEMORY.md, ../../TODO.md and ../../FINDINGS.md.
Topic reports retain experiment details at their existing paths with status
banners and a report index. No firmware, raw analysis logs, captures, keys,
recovery files or implementation binaries were removed or renamed.

## Archived reports and plans

Superseded, corrected or abandoned **reports/plans** are moved under this
directory with a top `SUPERSEDED BY: <current doc>` line and one row in the
registry below. Raw analysis evidence stays under `artifacts/analysis/`; only
the report/interpretation moves. Remove the moved item from the active report
index and topic map, or mark it historical there.

Archived items are not current evidence. Cite them only as archaeology, name
the replacement, and re-verify any contract from the firmware before reuse.

| archived item | archived | reason | superseded by |
|---|---|---|---|
| `docs-before-tidy-2026-10-31/` | 2026-10-31 | pre-cleanup handoff/log/checklist/report bundle; historical text only | current AGENTS/MEMORY/TODO/FINDINGS and the active report index |

When adding a row, keep the dated filename, record the exact reason and the
current replacement, and confirm the active index/map no longer presents the
item as current.

Read old entries only to recover provenance, an experiment's exact conditions,
or an incident timeline. Cite the date and check the current correction map
before reusing a conclusion.

AGENTS.before-tidy.txt is deliberately not named AGENTS.md. Relative links in
archived originals retain their old context and are not active navigation links.
