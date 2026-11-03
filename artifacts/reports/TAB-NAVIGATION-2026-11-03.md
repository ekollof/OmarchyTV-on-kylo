# TabDialog availability and focus prerequisites — 2026-11-03

Review of the pending NAVIGATION-SERVICE consumer found that unconditional
five-tab construction or immediate picture-content focus would violate the
firmware. `common/tab_policy.c` now implements availability and the initial
focus request, separately from actual widget creation and permission checks.
No TV access, wake, drawing, keys, device ownership or deployment was needed.

## Firmware authority

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Fresh read-only/noanalysis
Ghidra decompile plus ARM instructions are retained under
`../analysis/tab-navigation-2026-11-03/`:

- `init-content.txt`: constructor d6f360, init d70550, showSubMenu d6f140.
- `availability.txt`: isTabAvailable d6ef80 and setTabMenuItemsActive d6f028.
- `keys-focus.txt`: preHandleKeyPressMessage **d700a0**, focusSubMenu d6f61c,
  unfocusSubMenu d6f304. **d6faec is updateGlyphAndTitle**, not the key handler.
- `hotel-availability.txt`, `hotel-config.txt`, `hotel-profile.txt`: hotel
  wrapper/config/profile getter chain. Ghidra's void prototypes are wrong;
  verify returned r0 against the instructions.

The constructor takes `from_launcher` into byte4b7 (navigation13 true,
QuickMenu/navigation12 false). Profile-derived byte37a0 is another input;
its exact field name remains unresolved. Do not equate it with the separate
getWifiAvailability check used when creating network content.

Availability, by raw tab ID:

| tab | predicate |
|---|---|
| 0 picture | hotel inactive:1; otherwise low byte of picture flag XOR1 |
| 1 sound | 1 |
| 2 network | byte37a0 nonzero, then installation conditions |
| 3 installation | hotel inactive or OSD mode2, then system conditions |
| 4 system | outside media browser, from_launcher nonzero; low byte of inApp XOR1 |
| other, including negative | 1; consumers must independently restrict IDs0..4 |

The helper retains the firmware's raw XOR byte result; it does not normalize
255/2 to boolean before XOR. The mask helper interprets nonzero availability
only for valid tabs0..4. QuickMenu's false entry flag normally limits it to
picture/sound; it is not equivalent to launcher entry.

Hotel activation combines the configuration availability byte (+1d) with
ProfileConfiguration::isHotelModeEnabled. The latter tests the profile integer
equal to1, not merely nonzero. Picture availability is configuration byte+2d.
Hotel OSD mode is a separate getter. These inputs are not fabricated from the
settings SQLite database or screenshots; no native context provider exists yet.

## Initialization and focus are separate

init iterates five possible tabs, applies availability and requires each
description before creating its button. Requested tab matches select the
button's creation-order index, not the raw tab ID. Fallback if the requested
tab is missing/unavailable remains unresolved; do not assume the first tab.
The main title remains parent+140; an optional settingsTitle description can
create an additional textbox. init activates tab buttons, focuses their group,
updates title/glyph and calls showSubMenu.

It requests focusSubMenu only when open-submenu is nonzero **or signed item>0**.
That condition is implemented in oa_tab_requests_content_focus; it is not
permission to focus. Content interface validity (+18), installation password
and network stream/agreement/ordinary privacy gates still apply. Ordinary
network permissions remain in scope; tracking/advertising do not.

showSubMenu builds distinct picture/sound/network/installation/system content.
Network additionally checks Wi-Fi availability. Installation/system use other
per-tab retained menu state, not just the shared settings-item field. Existing
content is hidden/replaced and failed validity destroys the new content.
Actual content validity, per-tab state and writeback remain unimplemented.

The old Select/per-type-row list was incorrectly attributed to d6faec.
The real d700a0 handler forwards focused content Left/Right, handles content
edge/focus behavior and close/repost keys. It has no direct Select case
implementing those per-type subdialogs. Some content-vtable extrema and
description-dependent branches still need resolution; the old summary is not
a complete key reducer or a live key recipe.

## Verification and limits

The optional ARM-only tests/tab_policy_firmware.c executes the **actual shipped
168-byte function** under qemu. No vendor bytes are embedded. The source ELF is
hash-checked by verify.cjs; the fragment's CRC32 is550b31ac. It maps only private
process memory at original addresses, refusing fixed overwrites, stubs getter
calls and redirects only the stack-guard pointer literal. Instructions and jump
table stay unchanged; no vendor init, device or TV process is involved.

884,736 comparisons pass: all valid/default tab branches, six OSD modes,
all256 picture/app XOR bytes, independent state combinations and boolean/raw255
truth inputs. This proves the predicate over that synthetic getter corpus,
not native getter acquisition, content acceptance, key routing or UI parity.
Host invalid-input/mask/focus tests, ASan/UBSan and ARM/qemu pure-policy tests
also pass. Full host suite passes, recorded in host-check.txt. Isolated ARM
build-tv-v3/tab-policy artifacts (not deployed): pure test18508 bytes/MD5
b6bba60983e815ec3b4108b1408536b2; optional reference harness41068 bytes/MD5
55a1e784ea3fd9dfe22b1b09127a8832. These are test artifacts, not a new production
module or target-loader acceptance.

Reproduce with the explicit isolated ARM build in the analysis README and
`node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs`.
The helper is not yet called by ui_vendor; queued navigation still needs one
lifecycle-safe consumer with real context, content validity and writeback.
The captured SCREEN_SETTINGS Select shortcut is still diagnostic.
