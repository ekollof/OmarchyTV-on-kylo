# Reloadable settings-navigation service and IPC — 2026-11-03

Subsequent [TAB-WRITEBACK](TAB-WRITEBACK-2026-11-03.md) adds v2 independent
retained setters and OAN2 raw-state snapshots, keeping the v1 prefix and reading
legacy OAN1. The v1/OAN1 description below is the original checkpoint, not the
current service version or a complete content lifecycle contract.

ui_navigation now publishes ui.navigation v1 and exposes its retained state and
queued settings requests through the existing CLI/Unix control socket. No
drawing, input/HAL calls, device ownership, worker or telemetry. Host, focused
sanitizer, ARM/qemu, actual local Unix IPC and host loader-preflight checks pass.
The drawing/focus/modal consumer is still pending; this is not vendor UI parity.

## Source authority and behavior

LAUNCHER-ROUTING supplies all62 signed operation conversions and28 supported
unconditional settings plans. New firmware evidence is the DemoApp constructor
ce0f40: tab+fd8, row+fd0 and open-submenu+1259 all initialize to0. Fresh read-only/
noanalysis Ghidra decompile and selected ARM from the durable objdump cache are
in analysis/navigation-service-2026-11-03. Source Aurora MD5 remains
64c252340d3af6c5d66072611b7bad7a. Independent setter stores were verified in the
preceding milestone, so missing writes retain the other fields.

result() accepts a completed modal result, operation() accepts a launcher
operation ID. Neither accepts IR/ICE/VK keys or supplies their focus/modal gates.
They execute the recovered ordered state/enqueue plan in memory. Unsupported
branches retain ENOTSUP, including4b portal prework and a6 tuner/runtime gates.
No vendor logging/ad/tracking calls are copied. Queue-front means insertion,
not immediate dialog creation or synchronous event emission.

The item field is the stored settings-item/result-domain ID, not a visible row
index: e.g. advanced picture stores109/0x6d. Mapping that ID into a widget row,
and other per-tab retained state such as installation selection, belongs to
the pending TabDialog consumer. Do not index a nine-row list with this value.

The queue stores navigation IDs, not selection snapshots: vendor navigation
messages carry the ID while the fields live on DemoApp. take() therefore copies
CURRENT retained state at consumption, with pending count after removal. Two
queued operations may both observe the later state. An advanced-picture result
followed by network retains item109/open1 with tab2 for both queued requests.
Capturing selection when the first enqueue occurs would also be wrong for
base plans that enqueue before their field setters.

Fresh d0e9b8 ARM/decompile confirms the message constructor stores the route
at+c and initializes its payload/strings; it does not capture selection.
d18868 constructs TabDialog later using the DemoApp pointer, not a captured
selection in that message. Complete TabDialog initialization and state-writeback
policy remain the consumer's next contract, not an implemented part of this service.

Project queue capacity is16. An unsupported/full request changes neither fields
nor queue; ENOSPC is an explicit bounded-allocation deviation from the vendor's
heap-backed message allocation. take() is destructive and requires a single
dispatcher; callers use state() for read-only inspection. No dispatcher is yet
attached to the skin, so requests remain queued until an explicit consumer takes
them. These methods are loop-thread only and return errno values.

## Reload/standby and control interface

Ordinary stop/start retains context state. Reload copies state plus pending IDs
in OAN1: magic4, open byte, count byte,2 reserved zero bytes, little-endian32 tab,
signed item and queued navigation IDs. Exact length16+4*N, maximum80 bytes.
Restore validates version, flags/count/reserved bytes, tab and navigation IDs
before replacing state; signed item values preserve all32 bits, matching the
plain vendor setter. Native structs are never serialized. A short save buffer
is untouched and returns the required size so the host can reject it.

Normal consumer holds block stop/unload/reload. The provider marks itself
unavailable BEFORE service/removed fan-out, so even a callback holding a cached
interface cannot enqueue during removal. The cached pointer must then be
dropped as usual. No callback/worker keeps module code alive after cleanup.

Paired suspend keeps state readable and rejects submissions/takes with EBUSY.
A reload while suspended defers publication to activation on resume; a failed
publication preserves a peer and retries without losing restored state. CLI
mutation is rejected with ESHUTDOWN during host shutdown; state remains readable.

The following development-host commands use existing stdin/Unix IPC. The TV
has no new deployment from this work:

```
load ui_navigation
start ui_navigation
nav-operation 2
nav-state
reload ui_navigation
nav-take
```

nav-state is non-destructive. nav-result takes a signed decimal modal result;
nav-operation takes an unsigned decimal launcher ID. nav-take consumes a request.
The parser rejects missing/extra/force arguments, overflow and non-decimal text.
No command sends keys or directly dispatches/draws a dialog. ABI remains3;
service version1 is new. Source now builds22 modules; historical/installed ARM
stages were not changed.

## Validation and remaining integration

Three real-DSO host groups cover defaults, domain/argument errors, ordered
retained fields, current-state take, pending-state reload, queue capacity,
shutdown gating, suspend/deferred activation, peer publication failure/retry,
normal holds, removal callback refusal, exact snapshot validation, signed row
state and cleanup. Focused ASan/UBSan and the same ARM/qemu DSO groups pass.
Full strict host suite and host --check including the new module pass.

Optional check-ui-navigation-ipc starts only the local ui_navigation module,
with an empty dedicated configuration and private Unix socket. A real client
submits/routes/queries/reloads/suspends/resumes/takes requests, then gracefully
terminates its own child and verifies socket removal. No synthetic display or
TV connection is involved. It passes; the durable test is tests/ui_navigation_ipc.cjs.

Isolated build-tv-v3/navigation-service: mod_ui_navigation.so26488 bytes,
MD5 da07746c756b32322156219eceb36142, maximum GLIBC2.4. Test executable482168
bytes, MD5 feeab5416f0c2be1e926db176ece119c, maximum GLIBC2.17. ARM/qemu is
not target-loader or hardware acceptance. No TV connection/wake, staging,
deployment, settings/network/persistent change or helper/job pending. The last
imported TV session reports suspension, not a new state poll.

Next attach a lifecycle-safe focus/modal/navigation consumer and replace the
captured SCREEN_SETTINGS Select->full-list shortcut using actual state gates.
This service does not invent an active-item result from a raw Select, construct
TabDialog, route provider/tuner actions or render tabs/content. Those obligations,
the guarded branches, key/profile path and real visible acceptance remain open.
Complete CEC/audio/independent startup and all other TODO scope remain required.
