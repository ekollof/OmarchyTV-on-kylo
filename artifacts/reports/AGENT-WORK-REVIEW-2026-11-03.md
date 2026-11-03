# Review of subsequent scanner/menu work — 2026-11-03

Reviewed ff89e27 through e7e7333 against the prior e4e42c1 milestone. Fixed
the scanner's added key-reader defects and corrected overstated navigation/
heap-scan conclusions. The154-case navigation inventory and corrected
SubLauncher virtual slots independently check out. No TV access in this review;
the latest other-agent session reports suspension, not current availability.

## Confirmed defects and source fix

The scanner previously followed SVGTextBox+ac without checking the pointer's
mapping, interpreted snapshot virtual addresses as file offsets, and printed
any printable prefix as a complete key after failed reads or length exhaustion.
Its new test only covered a zero pointer and never exercised the reader.

Two original-binary reproductions are preserved under analysis/agent-work-review-
2026-11-03: nonzero-base snapshot reports a valid MODE key unreadable; a file
ending in MODE without NUL reports key="MODE" anyway. Before/after logs show
the corrected outputs. Original source was compiled directly from git e7e7333;
synthetic snapshots and executables stay ignored under workbench/agent-work-
review-20261103. No vendor data or target process is involved in these fixtures.

Reviewed scanner now inventories eligible mappings before following pointers.
Candidates still come only from private readable/writable [heap] regions.
Keys can be followed only inside those regions or private read-only mappings
of /vendor/com.nvt.idtv/aurora.elf, with a128-region cap and existing256MiB
heap scan cap. Arbitrary anonymous/device/other-library mappings are excluded.
Snapshot virtual addresses translate through the supplied base to file offsets.
Keys need printable bytes AND a NUL within40 characters; bad pointers, reads,
nonprintable bytes and missing terminators get distinct failure status, never
partial text. Quotes/backslashes are escaped. Completed-range counters remain
distinct from eligible-byte budgeting, so a failed scan does not claim all
planned ranges were scanned. Failed/changed live mappings remain non-atomic.

Fresh firmware ARM/decompile verifies the field before relying on it:
SVGTextBox::setText(char const*)10c78a0 looks up the resource key and stores
the pointer at+ac at10c78d6. setText(UString const&)10c7828 copies current
UString at+a4 but DOES NOT clear+ac. Thus a key can be stale. Scanner now
always prints the raw8-byte UString metadata even when a key exists; metadata
is not the character buffer or proof of current visible content.

Tests exercise mapped-key bounds, nonzero snapshot translation, EOF/prefix
rejection, nonprintable content,40/41-byte limits, escaped delimiters,32-bit
end-address handling, map permission/path filtering and retained UString
metadata with a nonzero key, alongside the previous geometry/span checks.
Full strict host suite passes before and after the substantive scanner change;
focused ASan/UBSan and ARM/qemu pass, including the final counter correction.
Default CFLAGS retain C11/Wall/Wextra/Werror/pedantic. Native build only:
build-tv-v3/review-latest/ui-object-scan37392 bytes, MD5
dc22cd80cc1d6b0f5e26ba730c3186e9, maximum GLIBC2.7. Not staged/run on TV.

## Evidence reviewed and corrected

Source Aurora MD5 64c252340d3af6c5d66072611b7bad7a. Fresh sequential read-only/
noanalysis Ghidra extracts are durable in the analysis directory. The maintained
verify-findings.cjs checks all154 menu-map entries against fresh case-local
ordered unique doNavigate*/handle*/doModal calls. It also checks ELF vtable
1500cfc+8+10=ee53a9/SubLauncherDialog::setFocus and+18=edf1f1/isShowing.
These findings remain valid. The map is syntactic: goto/delegate paths, guards
and unconditional outcomes are not resolved by that check. Its original
"first call" provenance claim was inaccurate and is corrected.

The two XMLs establish VK/DIAL1043 versus ICE593/0x251 under KeyQuickMenu;
VK1020 versus ICE522/0x20a under KeyUp. Only decimal/hex ICE representations
are equal values. DIAL payload arrival in an app log does not prove internal
ICE handling. The recorded QuickMenu attempts have no visible effect; a
profile/modal gate is a hypothesis, not a confirmed cause. Fresh f31b80 ARM
confirms the remote-code !=0xff availability check, not its live value.

The scanner identity table did not include TabDialog. Its absent match cannot
prove that class was absent or assign tab-strip ownership. Even inventoried
vptr absences cover only eligible heap mappings/candidate types. The live
sublauncher record is useful positive geometry, not process-wide absence.
No-value-adjusting-key/screenshot observations do not establish zero settings
DB writes without a differential. Corrected these claims at their source.

Slider label extra30px remains a conditional comparison of captures and
non-atomic widget geometry, not an implemented/proved row-type branch. The
common label path is useful evidence, but "all other inputs excluded" and
"one scanner pass settles content" were too strong. Matched actual text,
retained draw/parent state and full-list identity remain required.

The key playbook now distinguishes recorded starting focus from a universal
Down-count, stops on unexpected screens, and defers authorization to AGENTS.md.
It no longer conflicts with standing read-only staging permission. Canonical
HANDOFF/MEMORY/FINDINGS/TODO/index now point to this work and the previously
unindexed menu evidence. The unrelated owner FUNDING.yml commit is unchanged.

## Remaining work

No UI rendering, navigation or hardware parity is newly claimed. ui.title is
still default-off. Follow the unresolved launcher operation-result/navigator
and offline key/profile paths before repeating unchanged1043 tests. A later
live full-list query needs a verified screen/instance and complete current
UString evidence; no proof comes from a stale lookup key or absent TabDialog
match in the old scanner. Actual glyph scene/FBO/presentation parity remains
open as recorded in UI-GLYPH-ROUTING. No deployment, staging or helper pending.
