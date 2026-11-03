# Initial tab selection and unavailable-request fallback — 2026-11-03

The unavailable-tab fallback left open by TAB-NAVIGATION is now resolved for
initial TabDialog construction. `oa_tab_initial_selection` takes independent
availability and description masks plus a requested raw tab ID. It produces
the created mask/count, creation-order selected index, focused raw tab and
initial content tab. It neither draws nor consumes queued navigation.

## Firmware evidence

Same shipped Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Fresh sequential
read-only/noanalysis Ghidra and ARM evidence is in
`../analysis/tab-selection-2026-11-03/`:

- group.txt: UIGroup constructor11484fc initializes selected index+2c to0;
  setSelectedChildTarget11485d4 validates signed index against child count,
  changes+2c only for an accepted index and propagates the group's current focus.
  setChildrenFocus1148424 focuses only the selected child when enabled.
  getFocusTarget114854c scans actual child focus flags; it returns null if none
  are focused, rather than returning the selected index's child unconditionally.
- attachChildTarget1149538 appends and propagates the current group focus;
  UIMessageTarget::setFocus1148f98 invokes its callback then stores byte+20.
- focus-keys.txt: UIGroup vtable154df40 confirms slot+20 at actual vptr+8 is
  setChildrenFocus. SVGButton::setFocus10af36c delegates through SVGWindow::
  setFocuseeebd4 to the base focus flag. The extra style/animation changes do
  not reject that focus assignment. gotFocus1149390 returns byte+20.

Combine this with the earlier verified TabDialog init d70550: tabs are visited
in raw-ID order0..4. Only available tabs with descriptions are attached. A
matching requested ID selects its **creation-order index**, after attachment.
Otherwise no explicit selection is written, retaining index0. init later
focuses the group. Therefore the first created tab is the fallback; e.g.
unavailable picture0 with sound1 present focuses sound1 at group index0.

Empty groups are distinct: selected index is still0 but there is no focused
tab. TabDialog constructor initializes content-tab+4ac to0, and showSubMenu
d6f140 updates it only when getFocusTarget returns nonnull. Thus empty initial
groups retain content tab0. The model returns focused_tab=-1/content_tab=0;
this does not certify successful picture-content construction or display.
The focused_tab sentinel is our explicit API representation, not a vendor field.

Bits outside0..4 and null output are EINVAL, with output left unchanged. Unknown
requested IDs are allowed and take the same fallback path as missing tabs.
The inputs must describe the actual initial snapshot; later dynamic availability,
key handling, retained content selection and modal writeback require their own
contracts. This helper is not an implementation of every UIGroup feature.

## Actual ARM verification

The optional tests/tab_policy_firmware.c now also executes four shipped group
functions in private mapped memory under qemu. Fragment CRC32s before literal
redirection:

| address | bytes | CRC32 |
|---|---:|---|
| 1148424 setChildrenFocus | 112 | bf9398eb |
| 114854c getFocusTarget | 88 | 8c289305 |
| 11485d4 setSelectedChildTarget | 84 | 3418dcb3 |
| 1149390 gotFocus | 52 | d3c5e535 |

Instructions are unchanged; only stack-guard pointer literals are redirected.
Vector begin/end/count and child focus callbacks are bounded test stubs; the
attachment/init loop is reproduced from inspected ARM, not the entire vendor
framework. No platform/HAL initialization or device operations occur. The
mapping refuses to overwrite existing process mappings. No vendor bytes are
embedded in source or committed as binaries.

All32 availability masks ×32 description masks ×9 signed requested IDs give
**9,216 matching cases**, including no children and missing requested tabs.
Each case also verifies rejected negative/count indices leave selection/focus
unchanged. The preceding884,736 shipped availability comparisons still pass.
Host pure policy, focused ASan/UBSan and ARM/qemu policy checks pass. Full host
check passes and is recorded separately; this is not native UI or target-loader
acceptance. Final isolated ARM pure test25220 bytes/MD5
d797381250fd2b7955961b249bf27454; optional reference harness49624 bytes/MD5
47edeca6b311b14de6e6d8d842ca8e29. These replace the earlier milestone's test
builds only, with no production module or installed ABI changes.

Reproduce using analysis/tab-navigation-2026-11-03/README.md's isolated
build-tv-v3/tab-policy build and verify.cjs. This continuation did not connect to,
wake or change the TV. No hardware/native context, queue consumer, content
validity/permission, rendering or visible parity milestone is claimed.

Next acquire the actual hotel/profile/app/entry context and description inputs,
recover content/writeback and focus permission contracts, then attach the
lifecycle-safe queue consumer. Do not replace the diagnostic screen shortcut
with an unconditional operation-to-draw path.
