# Ordered settings event controller — 2026-11-03

Subsequent [TAB-PREHANDLE](TAB-PREHANDLE-2026-11-03.md) implements the outer
pre-handler body separately and checks it against shipped ARM. Its fallback
message map/backend/visible integration remains open; the original continuation
below is this earlier checkpoint's scope, not a request to redo that recovery.

common/tab_events.c implements the recovered TabDialog onUpDownPress and
onRightLeftPress bodies behind backend query/action callbacks. It replaces
neither the full pre-handler nor the captured-screen shortcut yet. The same
controller can later call the skin/content backend while retaining source order
and fresh state reads. No display, input, SQL, hardware or telemetry code is
inside this component; no new module/service/configuration is added.

## Firmware authority and behavior

Shipped Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Existing exact decompile
and instructions, reused rather than rerun:

- analysis/tab-writeback-2026-11-03/content-writeback.txt: onUpDownPress d6f874.
- analysis/content-lifecycle-2026-11-03/show-path.txt: onRightLeftPress d6fdc0.

Up/Down first reads retained open-submenu state. When nonzero, it calls, in
order: set item0, updateActiveList, clear open-submenu. It then reads tab-button
activation **after** those actions. If active and the ICE key is Down20b, absent
content causes showSubMenu, followed by focusSubMenu. Existing content skips
show but still requests focus. Up20a does not take this Down branch. The native
method returns handled1 even when no action is taken.

Left/Right reads the activation gate once. When active, it calls hideSubMenu,
dispatches the original message to the group, updates glyph/title, then calls
showSubMenu. It does not recheck activation after hide and ignores the group
handler's result. It also returns handled1 for an inactive gate. There is no
extra key filter inside this body; selection of the handler is the outer
router's job. Do not call it unconditionally for every received key.

The new C API uses positive errno status (0 valid/consumed, EINVAL missing
ops/callbacks), not the native handled return value. Queries must reflect current
backend state, not a submission-time plan. Actions are void requests; calling
focus does not prove password/permission/showing checks succeeded. Callbacks
may change state synchronously; the controller intentionally reads it again
where the firmware does. No callback or context is retained after return.

ICE codes remain full32-bit arguments, distinct from eight-bit RC/UART codes.
GROUP_KEY also receives the original borrowed opaque message unchanged; it is
not reconstructed from its key and is never forwarded to unrelated actions.
The native differential checks message identity as well as action arguments.
Adapters must keep it alive during the call and copy any understood payload
needed for asynchronous work rather than retaining the borrowed pointer.
Callbacks implement the established show/hide/focus/group/title/list lifecycle;
they must retain code/context ownership and obey the host's nonblocking rules.
GPU swaps/destroys stay in the owning worker/helper, not these loop callbacks.
The consumer must separately hold/release ui.profile, settings.hotel and
ui.navigation as appropriate; this common component does not acquire them.

## Differential verification

The optional tests/tab_policy_firmware.c now executes both actual shipped
functions in private qemu mappings. Source fragment fingerprints before guard
redirection: d6f874 length128 CRC32 5d5b95db; d6fdc0 length84 CRC32 1e26b4d6.
Instructions stay unchanged. Only guard literals are redirected; callees are
controlled state/logging stubs, not vendor init or TV operations. No vendor
bytes are embedded or committed. The maintained driver verifies source hashes
before execution, and mappings refuse fixed overwrites.

**1,008** cases compare exact action/argument/message traces and final state against
the C controller:4 raw open values ×3 activation values ×2 content-presence
states ×3 list-update effects ×7 full ICE keys ×2 methods. List-update effects
can enable/disable buttons and change content presence. Hide deliberately changes
the gate, catching an incorrect second gate check. Native methods always return1
in the corpus. Dependencies remain stubbed, so this is method-body behavior,
not native permission, group selection, rendering or hardware acceptance.

Host unit cases additionally cover invalid callback tables before any action,
ordering, existing/missing content, gate changes and full-width key forwarding.
Strict host, focused ASan/UBSan and ARM/qemu policy checks pass. The prior
availability/group/profile/setter/hotel-row reference corpora remain green.
Full host suite passes, recorded in analysis/tab-events-2026-11-03/host-check.txt.
Final isolated ARM build-tv-v3/tab-policy pure test21512 bytes/MD5
e032185de9554ef077d02570bb4a60aa; reference harness113600 bytes/MD5
8f69a99ddbc17289e2433a15e3a6847e. No production-module or installed ABI changes.

No TV connection, wake, staging, deployment, keys, drawing or persistent changes.
The controller methods are implemented but **not attached to ui_vendor** yet.
Next complete the outer pre-handler/focus/permission/modal flow and connect
backend callbacks with real owned content/context. Existing SCREEN_SETTINGS
Select->list behavior remains diagnostic. Complete parity stays open.
