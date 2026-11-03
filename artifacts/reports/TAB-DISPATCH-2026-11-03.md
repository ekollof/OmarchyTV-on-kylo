# Tab fallback dispatch and key delivery — 2026-11-03

Subsequent [TAB-LEAF](TAB-LEAF-2026-11-03.md) implements six dispatched bodies.
Remaining base settings map/list/mute and real backend/consumer are still open;
its base.txt already contains the source map for continuation.

`common/tab_dispatch.c` implements the shipped TabDialog fallback message map
and joins it to the recovered pre-handler. It does not yet attach the owned
content/renderer adapter, dequeue navigation requests, or provide the remaining
base SettingsDialog/leaf handler implementations. No TV access was needed.

## Firmware authority

Aurora ELF MD5 `64c252340d3af6c5d66072611b7bad7a`. Read-only Ghidra
decompile/instructions are in analysis/tab-dispatch-2026-11-03/source.txt and
matcher.txt. The maintained ARM reference executes these source fragments:

| address | bytes | CRC32 | role |
|---|---:|---|---|
| d6fe14 | 652 | 050d1bd2 | TabDialog::handleMessage |
| cc4f38 | 96 | 90529f1b | uiDoesMessageMatchMapEntry |
| 114925c | 68 | eb6eb524 | UIMessageTarget::handleKeyPressMessage |

Only each stack-guard pointer literal is redirected. Lower handlers and pure
message getters are controlled stubs; no vendor initialization or devices are
used. Private mappings refuse to overwrite occupied addresses. The maintained
driver checks the full local ELF/profile hashes before executing anything.
Vendor code remains a local input, not embedded in our source or committed.

The matcher first compares the virtual message type. Only a matching type
queries the virtual ID; a map ID of -1 is a wildcard, but **this TabDialog map
contains no wildcard entries**. Key messages report type0x1004 and ID equal to
their key field. Our dispatcher accepts already decoded, immutable full-width
type/ID values. It does not emulate repeated virtual getter invocations or the
general wildcard matcher; those are unnecessary for this concrete map. Wrong
message types and high-bit ID lookalikes must reach the base message handler.

## Routing and consumption

All entries below require type0x1004. Every callback receives the exact original
borrowed event; this layer allocates, copies and retains no event or callback.

| IDs, hexadecimal | target |
|---|---|
| 22d,251,2bb,25b,210 | qualified SettingsDialog::onMenuPress; parent consumes regardless of return |
| 20a,20b | TabDialog::onUpDownPress |
| 20d,20c,2c9,2c8,2da | TabDialog::onRightLeftPress |
| 220,221 | TabDialog::onVolumePress |
| 21f,28b | SettingsDialog::onQuitAndIgnoreMessage |
| 230,24c | SettingsDialog::onGuidePress |
| 200 through209 | TabDialog::onDigitPress |
| 2a7 | TabDialog::onRemovableDeviceDisconnected |
| other IDs or message types | qualified SettingsDialog::handleMessage |

The menu route bypasses TabDialog's same-named wrapper. Extra Left/Right IDs
must be forwarded unchanged; do not rewrite them to20c/20d. Other route results
are propagated as handled truth. The C API separates errno from normalized
handled0/1. A chosen callback is an execution boundary, not proof that a UI or
hardware effect succeeded or that permission was granted.

`oa_tab_handle_key` follows the caller's pre-handler-first order. An unhandled
key enters the fallback map; a consumed key does not. This function is only for
already decoded key delivery, not arbitrary UIAnyMessage input. Both stages
must reference the same borrowed event. Invalid callbacks/output or mismatched
event pointers return EINVAL without executing either stage. A pre-handler
ENODATA is propagated without fallback or changing handled; earlier pre-handler
actions remain, as documented in TAB-PREHANDLE. The adapter must pin its code,
context and event, and supply permission/lifetime gates and nonblocking actions.

## Verification and scope

**65,600** actual shipped map/matcher comparisons cover IDs0..fff and four
full-width outliers, four message types including1004/11004 and four callback
results0/1/255/ffffffff. Each matches selected target, exactly one callback,
original event identity and handled truth. **16,384** additional comparisons
execute the shipped key caller/map/matcher with the separately verified C
pre-handler installed as its virtual method. This verifies their join, not an
independent second proof of the pre-handler body; TAB-PREHANDLE owns its65664
independent native-method comparisons. Join cases cover keys0..3ff and four
pre/base result values, comparing state, ordered pre-actions and fallback.

Strict full host suite, focused ASan/UBSan, ARM/qemu unit test and all prior
reference groups pass. Isolated build-tv-v3/tab-policy test44532 bytes, MD5
e25592dbcfbb7146f83018ffba44815c; reference205780 bytes, MD5
b36b2bd2db145f79b9ccd13b5999bea4. No new service/module, ABI change, persistent
deployment, TV wake, hardware test or independent visible acceptance.

Next recover the remaining qualified base/leaf handlers and compose the owned
renderer/content/runtime-context adapter. Bind the navigation consumer only
after the required context, modal/focus/permission and lifetime contracts are
implemented. A pure map match does not justify replacing the diagnostic screen
shortcut with unconditional key dispatch.
