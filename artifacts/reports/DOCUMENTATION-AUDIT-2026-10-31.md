# Documentation and artifact guidance audit — 2026-10-31

> **Status: local documentation audit.** This does not revalidate all historical
> experiments or establish new TV hardware behavior. TV untouched.

## Problems found and corrected

The previous root documents mixed current status, historical experiments,
proposals and successive corrections. MEMORY opened with pre-root “settled
facts”; AGENTS simultaneously said web boot was enabled and disabled. TODO still
proposed HDMI-only descope despite the owner's full parity requirement. Build
docs described old dlopen dependencies/counts and risky baseline output paths.

Specific red herrings included the palette acquire/commit theory, ordinary dumb
buffer scanout plan, treating generic DRM discovery as SETPLANE, cold STBC
watchdog probes, channel sweeps, assuming IR integration solved, and equating
pixel readback or a successful HAL call with independent hardware acceptance.
The correction map now links each to its actual evidence and remaining limit.

## Resulting organization

- Root README gives the reading order; AGENTS contains policy and stable workflow.
- MEMORY owns the last verified TV/build state, TODO the pending work, FINDINGS
  the conclusions/corrections and HANDOFF the concrete resumption entry points.
- All 38 pre-existing reports are indexed and have scoped status banners. The
  most misleading planning reports were replaced with corrected summaries;
  their originals and the previous top-level docs are preserved under
  artifacts/archive/docs-before-tidy-2026-10-31. Archived AGENTS is named .txt
  so it does not become a nested active agent-instruction file.
- Obsolete descope/watchdog/handshake next steps were removed from active report
  bodies. Later OSD sections now resolve earlier questions instead of leaving
  contradictory “next” paragraphs. Old experiments remain accessible as history.
- Artifact/payload/script/probe/helper indexes distinguish current recovery from
  old images, read-only queries from input-consuming/mutating helpers, and raw
  decompiler output from verified signatures.
- Six /tmp-only DRM contract traces were copied unchanged into durable analysis.
- Source changes are comments only: VIN Open return description/incomplete routing,
  UART reader side effects, and musl build target description. No runtime logic,
  Makefile recipes, binaries, firmware, key material or recovery files changed.

## Local verification and limits

Recounted defined unique dynamic T/W functions: 321 Hal_* in NDAL3 and 80 NDAL_*
in NDAL_EX. Checked file type: vendor Aurora is conventionally stripped but keeps
dynamic names; payload aurora.elf is a shell hook. Counted 964 PNGs and 56715
language-inventory lines. Current hook/keeper/key/recovery/raw image identities
match the artifact map. Extracted Aurora/NDAL3/DRM/nondovi/hwprofile hashes match
the topic evidence.

Checked 57 active Markdown documents and 189 local link targets: no missing
targets. All 39 reports (38 existing plus this audit) are indexed and have status
banners. Six saved trace copies compare byte-identical to their temporary originals;
recovery/hook/keeper hashes remain unchanged. Historical archive links retain
their original context and are excluded
from active-document link validation. No implementation test rerun was needed
for documentation/comment-only changes; prior test claims retain their original
scope/date. No TV network access, hardware operation or service action occurred.

This audit does not certify every old technical assertion. Unresolved generic
GEM initialization, cold OSD state, IR fields and hardware/feature acceptance
remain explicitly open in TODO. Consult original instructions/firmware before
any new device operation. Future evidence should update the canonical entry and
the relevant report rather than append another competing status snapshot.
