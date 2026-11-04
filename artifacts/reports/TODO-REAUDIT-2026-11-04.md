# TODO follow-up audit — 2026-11-04

Reviewed all183 entries checked at172cd57, using the earlier182-item audit,
the changes since2d48e2f, source/test inspection and fresh execution.
**182 retained as bounded milestones; the added image.decode entry reopened.**
This is not certification of the whole replacement. Research, prototypes,
controlled-call comparisons and dated hardware observations retain their
explicit boundaries. No TV access, deployment or hardware change occurred.

## Corrections

- **PNG acceptance was checked too early.** Aurora calls its built-in
  `cabot_png_*` codec. The committed service calls shared libpng's simplified
  API. Matching nine downloaded images against ImageMagick does not compare
  against Aurora's class or establish all conversions. Saved firmware evidence
  at b30918 shows packing/gray expansion, strip16, gray/palette conversion,
  tRNS/filler and conditional BGR; the recorded routine does not request gamma
  conversion. Different libpng APIs cannot be assumed equivalent for the
  untested formats. No vendor-class differential exists yet.
- `tests/image_decode.c`'s alleged non-PNG test uses
  `<thumbs>/image_decode.c`, which is absent. It repeats a missing-file test.
  It tests stop/start, not the host reload path, held-provider rejection,
  suspend/resume, library/publication failure or error cleanup after parsing.
  The committed engine retains the provider allocation and later frees it
  itself instead of calling the interface's release method while held.
  Separate uncommitted edits address these areas but are not accepted here.
- **Recorder alpha is fixed, complete renderer fidelity is not.**
  `tests/ui_nav_display.c` uses `oa_canvas_over` and publishes display.alpha;
  `tests/ui_sources.c` asserts transparent/opaque/half-alpha pixels. Retained
  that composition milestone. However, `d_set_scene_transform` only saves
  arguments and `d_present` dumps the untransformed canvas. Corrected TODO and
  UI-FB-SIM so these captures cannot be mistaken for animated output.
- Removed the stale claim that no CEC module integration exists: later mod_cec
  does route injected frames. Native device/transport acceptance stays open.
- The old audit inventory uses positional slots and rejects the newly inserted
  PNG entry. Its historical182-item output is retained unchanged. The follow-up
  ledger explicitly accounts for that addition instead of silently shifting IDs.

## Verification and provenance

Evidence: `artifacts/analysis/todo-reaudit-2026-11-04/`.

- `committed-host.txt`: fresh isolated worktree at172cd57,
  `make -j4 BUILD=build-audit check`, exit0. Includes PNG corpus/UI cards,
  alpha recorder, lifecycle, IPC, runtime and wrapped-device suites. Default
  flags include `-std=c11 -Wall -Wextra -Werror -pedantic`. The first attempt
  lacked the ignored workbench corpus in the isolated checkout; after linking
  that local input, the complete command passed. No production files changed.
- `native-reference.txt`: all42 maintained firmware-reference groups pass.
  Command: `node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
  openaurora/build-tv-v3/work-review/tests/tab_policy_firmware`.
  Identified executable SHA256:
  `c2e7fd85dc6d181247958a7d2446662577c6bf9d3c335b7ed83013e94514535f`.
  This reruns the prior reference executable, not a new build of the pending
  PNG changes. Common reference-tested source did not change since the audit.
- `worktree-build.txt`: the initial current-worktree focused PNG build fails
  under Werror at image_decode.c:111 (misleading indentation). Source edits
  continued arriving during this audit; this log is a dated snapshot, not a
  claim that later edits still have that compiler error. No unfinished source
  is included in the documentation commit or certified by the passing HEAD run.
- `checked-claims.md`: all183 original checks mapped to current disposition,
  with the earlier per-item evidence anchors retained. `ledger.cjs` verifies
  the explicit inserted-entry mapping and anchor existence; that validation
  alone is not behavioral proof.

The remaining182 checks are supported at their bounded research/helper/test/
historical scope by the earlier per-item review, unchanged implementations,
the recorder correction and these reruns. This does not turn a historical
hardware pass into current independent acceptance. Full UI interaction,
rendering, animations, native ownership, audio/CEC electrical behavior,
standby/cold startup and permanent system replacement remain unchecked.
