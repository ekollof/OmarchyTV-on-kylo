# ARM harness and recent list-work review — 2026-11-03

Review of0493f91 and its preceding list commits, requested after repeated QEMU
segfaults. The current14-case binary passed at the start of this review. The
last recorded SIGSEGV was19:12:21 against build-tv-v3/tests/tab_policy_firmware;
no core was retained, and the executable was replaced afterward. Therefore
the exact historical fault is **not established**. Do not infer a TV fault,
repeat old experiments or claim every later correction caused that SIGSEGV.
Memory was available and no OOM event was found in the queried window.

## Corrected defects and assumptions

- **Animation strings:** the old API mixed borrowed character pointers with
  destructor callbacks and treated substring as pointer+1. New callbacks use
  separate owned opaque UString handles, receive context explicitly and release
  only acquired handles. Empty names and character indexing are bounded by the
  provider. Shared ASCII stubs were not proof of UString semantics.
- **Source order:** compare operands are substring,standard. Duration parsing
  is followed by float conversion and multiplication by1000, all before
  substring/name destruction. Native count comparisons are signed32; negative raw
  container counts skip iteration. These operations are now independently
  observable in the outer-method trace, rather than omitted from comparison.
- **NULL name:** UString(char*) initializes empty then calls append; append's
  c8671c branch skips NULL. A NULL name must therefore become empty, not an
  invented invalid-data error. Empty-name length-1 wraps, and the source walk
  clamps to stored bytes. Length uses UTF-8 characters, not byte strlen.
- **Wrong page:** the scrollbar create trampoline at10c3f50 was installed via
  the10c4000 mapping, writing before that allocation into an adjacent mapping.
  It now uses its actual10c3000 owner. Every typed/raw trampoline installer
  checks the recorded owning allocation and full write extent first. Regression
  checks cover this underflow and an over-end write. Direct fragment memcpy
  sites still require explicit extent review; this is not a universal sanitizer.
- **Reload data:** a stopped ui.list accepted an OAL1 window sizeffffffff.
  Its tests checked malformed snapshots only while published, so the publication
  guard hid the missing payload checks. Restore now validates window bounds,
  handle capacity, selection and handled domain before mutating state. Rejection
  is atomic. A virtual total100/window5 remains accepted; total is not mistaken
  for the16-handle window capacity. These are owned-format rules, not new
  firmware limits. Empty-list state is also accepted.
- **Temporary object:** setSlideValue's char-array text object had no alignment
  guarantee. It now uses max_align_t alignment and an explicit64-byte provider
  storage bound. This is our callback-storage contract, not vendor object size.
- **Documentation:** ui.list actually selects vertical routing (query returns0),
  contrary to its old horizontal description. Runtime behavior is unchanged.

## Firmware and ABI evidence

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. New read-only source extracts
are animation-source.txt, string-source.txt, string-walk-source.txt and the
bounded objdump excerpt append-null-source.txt in the evidence directory.
The372-byte animation fragment retains CRC32 `10f1780d`; only its guard literal
is redirected. The current float ABI was already correct: BLX at10bbfd4 enters
ARM at PLT6b1274 and returns double in d0; the call at10bc002 passes float in s0.
Do not substitute a guessed base PCS or a Thumb trampoline there.

`/usr/bin/gdb` successfully selects ARM and reads the fresh ARM ELF/symbols.
The bundled arm-none-linux-gnueabihf-gdb exists but fails to load
libncursesw.so.5. Use working host GDB instead of installing ABI-incompatible
library symlinks. No remote GDB session or old-core backtrace is claimed here.

## Verification

Fresh isolated BUILD=build-tv-v3/arm-review passes153 actual ARM animation-body
comparisons and all earlier firmware groups. Cases include signed count exits,
NULL/empty/multibyte names, first-match/null-slot searches, duration precision/
underflow/overflow/NaN/Inf and exact started float bits. Parsing, comparison
operands and destructor roles/order are traced. The string fixture owns bounded
objects and models **valid UTF-8 only** from the recovered walk contract. Both
sides use that controlled helper: this does not independently validate the full
native UString library or malformed UTF-8. Native outer instructions, snapshots,
search/start order and ABI execute unchanged against those controlled callees.

Host ASan/UBSan tests additionally cover allocation failure at each string stage,
missing duration/controller cleanup, unchanged output on no-start/error and
context forwarding. Host/full and actual Unix IPC tests pass. ui.list and widget
tests also pass under ARM/QEMU and focused ASan/UBSan, including unpublished
corrupt-restore rejection, atomicity and legitimate empty/larger-total states.

Final isolated ARM identities:

| artifact | bytes | MD5 |
|---|---:|---|
| tests/svg_list_anim | 68848 | af5a22b097f815056718febb0ec0e1f0 |
| tests/tab_policy_firmware | 727940 | 155459ad850f1846299b9d45c2e94390 |
| modules/mod_ui_list.so | 79760 | 3cc7ac7bb20227ef76d0146571649fe2 |

The verifier prints exact binary path/SHA256 and accepts an explicit path.
Harness stdout is line-buffered so a later failure retains the completed groups.
No TV access, wake, staging/deployment, settings, privacy/access/network, display
or firmware/boot-hook changes occurred. No helper/Ghidra/test job remains running.

AGENTS.md now records the bounded first-fault workflow, working GDB choice,
instruction-state/VFP checks, mapping ownership and string-contract pitfalls.
Actual animation rendering/timing, primitive string/provider implementation,
display binding/startup and full replacement acceptance remain open. Existing
ui.list remains an inert rendering consumer; none of these tests proves visible
motion or hardware health.
