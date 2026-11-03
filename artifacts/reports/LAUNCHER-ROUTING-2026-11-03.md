# Launcher result conversion and settings routing — 2026-11-03

The firmware result-to-settings contract is now recovered and implemented in
common/launcher_route.c. It converts all62 launcher operation IDs and produces
ordered action plans for28 unconditional settings branches. Full strict host,
focused ASan/UBSan, ARM/qemu and independent firmware corpus checks pass. It is
a lower component for the modular navigation pipeline, not a new running UI
service, actual key-route acceptance or complete OperationFinder replacement.

## Firmware chain and gates

Aurora MD5 64c252340d3af6c5d66072611b7bad7a. Fresh sequential Ghidra read-only/
noanalysis decompile AND ARM under analysis/launcher-routing-2026-11-03:

- MainLauncherMenuDialog::onSelectPress e48404 calls focusLauncher(false),
  which checks its child pointer/showing/nonempty state and focuses the child.
  That callback alone is not the full Select dispatch. preHandleKeyPressMessage
  e4a3f4 has earlier branches involving sublauncher/edit/availability state.
- In its active-item Select branch, e4a98c obtains the focused launcher ID,
  e4a992 converts it, and e4a99a saves the result. Channel/provider work and
  focus/availability guards precede endDialog on the MAIN launcher with that
  result. This supplies a parent-modal result path; it is not proof which
  recorded key/screens satisfy all those guards.
- Child LauncherDialog::onSelectPress edde60 separately converts selected
  operation at eddeac and ends its SVGDialog base at eddeb4 when sublauncher
  focus is absent. Do not use the child callback in isolation to predict the
  parent pre-handler's key result.
- DemoApp::onMenuKey cf7120 calls main SVGDialog::doModal, retains the return,
  destroys the launcher and passes the result to OperationFinder::handleOperation
  at cf7548 when DemoApp+1276 permits it. FindCallers independently identifies
  this and cc78d0 as direct handleOperation callers.
- OperationFinder d3dc54 queues navigation13 for settings operations, unlike
  QuickMenu's navigation12. DialogNavigator maps13 to
  doNavigateTabDialog(true,false),12 to false,false. These are different entry
  variants; neither message alone proves content/title initialization succeeds.

Settings results are not portal application IDs: isPortalApplication d3d7dc
accepts only7e/fa; additional79/7a portal-related entry checks do not match the
implemented settings results. The model never ports portal agreements, logging,
tracking, advertising or recommendation behavior.

## Conversion and ordered actions

LauncherHelper::convertOperationIdToIDM e4ae0c checks its input as unsigned
<=61. ARM e4ae36 uses LDRSH, not an unsigned halfword. Literal e4ae4c points
at14efd84. All other inputs return-1; table holes also return-1. The complete
mapping is reproduced as compatibility metadata. Operation IDs, dialog results,
navigation message IDs and ICE/VK keys are separate domains.

oa_launcher_settings_plan accepts a RESULT, not an operation ID. Examples:

| operation -> result | ordered settings actions |
|---|---|
|2 ->6c picture|enqueue-front13, set tab0, set row0|
|3 ->6e sound|enqueue-front13, set tab1, set row0|
|6 ->5b network|enqueue-front13, set tab2; previous row retained|
|7 ->49 system|enqueue-front13, set tab4, set row0|
|result6d advanced picture|set open-submenu1, tab0, row6d, enqueue-front13|
|result6f advanced sound|set open-submenu1, tab1, row6f, enqueue-front13|

The other implemented deep entries follow the actual state setters and grouped
case labels; the independent corpus verifies all28 in order. Base entries do
NOT write the open-submenu flag. Missing writes mean retain state, not reset to
zero. Fresh setter ARM confirms independent plain stores: open flag at1259
(ccca0c), selected item atfd0 (ccca44), selected tab atfd8 (cccaac); none resets
the other fields. Enqueue-front is queue insertion, never immediate/reentrant dialog creation;
the plan must complete before dispatch. Plans are in-process values, not a wire
encoding or reload snapshot. No allocations, callbacks, hardware or queue writes
are executed by the helper.

Result4b has optional existing-portal work before its installation selection;
a6 depends on runtime/tuner state. Both return ENOTSUP, as do other unmodeled
operation branches. Invalid/unsupported requests preserve output. These gates
remain required future work, not a deleted feature. The component deliberately
does not copy SvgAppLoggingManager/NovoLoggingManager calls in vendor branches.

## Verification and remaining UI work

tests/launcher_route verifies unsigned bounds/holes, base versus deep ordering,
retained row/flag behavior and atomic unsupported/invalid cases. Its --dump
output is checked independently against the hash-verified shipped table and
fresh OperationFinder case bodies; source-derived commands retain their order,
including grouped labels. Both host and ARM/qemu dumps match all62/28 entries.
The script rejects unexpected guards in modeled cases. This is not a live
function trace or coverage of unsupported functional side effects.

Full strict host suite exits0, focused sanitizer and ARM/qemu tests pass.
Native test build only, build-tv-v3/launcher-route/tests/launcher_route:
26996 bytes, MD5 33b69aae50bed048b2c73a592d7c1acf, maximum GLIBC2.4.
No TV connection, wake, keys, staging, deployment or hardware operation.
The latest imported TV session still only reports suspension; it was not polled.

The helper is not integrated with the skin/service/IPC layer. Existing
SCREEN_SETTINGS Select -> SCREEN_SETTINGS_LIST is a diagnostic shortcut and
does not implement this retained modal/focus/result flow. Replace it through
the actual focus/modal state machine and a modular navigation consumer; do not
wire the new plan directly to every Select or claim shortcut tiles become the
full list in place. Installation/runtime branches, provider actions, complete
TabDialog content/interaction and visible acceptance remain open. No helper/
Ghidra/build job pending after this milestone. ui.title remains default-off.
