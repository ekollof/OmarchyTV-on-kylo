# Later-work review evidence

Current conclusions: ../../reports/WORK-REVIEW-2026-11-04.md.

- scrollbar-firmware.txt and scrollbar-constructor.txt: fresh read-only
  Ghidra/ARM evidence. +6c is the four widget states, not orientation.
- image-launcher-firmware.txt: image byte-flag and launcher source mappings.
- native-reference.txt: final isolated ARM executable identity, all42 groups,
  including14,576 scrollbar geometry/state,32 skin,36 render and80 image cases.
  Launcher ordinary fields compare after the three documented advertising
  exclusions; those exclusions are policy differences, not firmware parity.
- host.txt and sanitizers.txt: final full strict host and focused ASan/UBSan.
  baseline-host.txt is the unmodified baseline; smaller logs are intermediate
  checks and should not supersede the final files.
- arm-cli-runtime.txt:32-bit CLI/service overflow rejection and DB integration.
  arm-cli-build.txt identifies that build. Other ARM build logs plus
  arm-identities.txt identify source builds; build products stay local.
- stage.md5/stage-sizes.txt and tv-stage.txt: final staged TV unit/DSO sizes
  and hashes. tv-runtime.txt: five CPU units, live read-only54-row DB query,
  and loader-only no-HAL display_fb dependencies. *_initial records predate
  the final four-state correction and are retained only as dated evidence.
- tv-cleanup-final.txt is the final no-helper/removal/no-reset confirmation.
  tv-cleanup.txt belongs to a stalled pre-auth local connection, terminated
  without a result. The bounded retry needed tvssh's WoL handling and succeeded.

No TV display draw, stock key injection, HAL initialization or takeover was
performed by this review. Hardware restoration/startup/visual gates remain
open. No helpers, pending jobs or uploads are left running.
