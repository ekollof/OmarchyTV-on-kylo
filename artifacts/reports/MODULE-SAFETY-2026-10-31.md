# Offline module lifecycle follow-up — 2026-10-31

> **Status: Current ABI 2 ownership, reload and dependency-order test evidence. Installed/recovery build remains ABI 1; system-sleep coordination and rollback remain incomplete.** See [report index](README.md) and
> [current corrections](../../FINDINGS.md); old plans do not authorize TV changes.

Follow-up to `TODO-AUDIT-2026-10-31.md`. No connection to the TV, deployment,
hardware call, remote key, network change or reboot was made for this work.

## Changes

- ABI 2 adds `oa_module.size` and `oa_host_api.owner_id()` and changes `post()`
  to take an explicit module-instance token. All eleven modules and fixtures
  use the new descriptor layout. Rebuild core and modules together.
- Workers capture their token during loop-thread init. Posting checks the
  active-owner table and queues callback/userdata/owner under a mutex; workers
  never read the loader's module table.
- Stop, unload and failed init/start close post acceptance and purge pending
  callbacks before module cleanup. Dispatch restores the owner so registrations
  made by a posted callback are attributed to its module.
- Dispatch pops one post at a time. An earlier callback that unloads another
  owner can remove that owner's pending code before it is dispatched.
- Post userdata remains module-owned. Stop/destroy must stop and join workers,
  and reclaim canceled userdata. Queue cancellation does not protect an
  uncooperative worker. Forced unload still bypasses service holds.
- Default CFLAGS are `-std=c11 -O2 -g -Wall -Wextra -Werror -pedantic`, as
  requested by the owner. The dlsym address uses a representation copy and
  compile-time size check instead of an ISO-C-forbidden pointer cast. Config
  copies have explicit precision bounds, preserving existing truncation.
- Lifecycle tests respect `BUILD` for executable, modules and recovery fixtures,
  so an alternate build cannot silently test the default module directory.

## Verification

All new tests are offline. `make BUILD=build-strict check` passes with strict
flags: lifecycle/socket smoke suite, three registry regression groups, eleven
recovery cases and six new module-safety groups. The new groups cover:

1. Guarded ABI 1 and short ABI 2 descriptors rejected before inaccessible fields;
   a larger compatible ABI 2 prefix accepted. Sizes: 160 bytes host, 84 ARM.
2. Real worker posting and callback-created timer ownership.
3. Reload through the control-command dispatcher: saved state restored, old
   queued work canceled, new token accepted and old token rejected.
4. Full queue, stop-time worker posting rejection, cleanup and restart.
5. An earlier post unloads another module whose callback is still queued.
6. Failed init/start cancel queued callbacks and reject inactive/unknown tokens.

The same safety suite passes with ASan/UBSan and strict CFLAGS at `-O1`:

```sh
make BUILD=build-sanitize \
  CFLAGS='-std=c11 -O1 -g -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer' \
  LDFLAGS='-fsanitize=address,undefined' check-safety
```

Core, supervisor, all modules and test fixtures also compile with the strict
flags for ARM/glibc, in **`build-tv-v2`**, preserving `build-tv` and the recovery
payload. Safety and registry suites pass under `qemu-arm-static -L
/tmp/opencode/rootfs/root` against the extracted TV libc. Highest GLIBC version
required by the core/modules is 2.17, below the TV's 2.23. Initial offline core MD5
(superseded by the reload follow-up below):
`1738708febf70a9c7602be1421390689`.

These checks establish the tested loader/queue behaviours, not hardware parity
or arbitrary-module isolation. Restore errors and stopped/suspended reload were
subsequently fixed/tested below. Independent native
scanout and live IR interpretation remain open. Installed TV and recovery
artifacts stay on the existing ABI 1 build.

## Scope correction

Owner explicitly excludes **telemetry and advertising** from vendor parity:
no usage/app-click reporting, ACR, tracking/profiling, advertising IDs, sponsored
placements, ad delivery or optional implementations of those subsystems. Current
core/modules contain no such subsystem. Other TV features and local diagnostics
remain in scope. This requirement is recorded in AGENTS, TODO, README and design.

## State-aware reload follow-up

The earlier gaps were reproduced against the real command dispatcher: reloading
a never-started fixture incorrectly called start and left it running. This is
framework lifecycle behavior, not a vendor HAL API. No new device operation or
firmware signature assumption is involved in the fix.

| prior state | successful replacement behavior |
|---|---|
| loaded (including stopped) | init/restore, stay loaded; no start |
| started | init/restore/start |
| suspended | init/restore, stay suspended; defer first start until resume |

The deferred flag is internal host state, not an ABI layout change. On first
resume, the new instance starts once; subsequent suspend/resume uses its normal
callbacks. Start on an ordinary suspended instance now resumes rather than
calling start again. A failed deferred start cleans its registrations and stays
retryable. Stop/unload before deferred activation does not call stop on an
instance that never started. Module init/restore must not activate hardware;
workers still must follow the stop/destroy join contract.

State allocation failure, size above64KiB or a second save result larger than
capacity abort before unloading the old instance. Restore failure, absent restore
with nonempty saved state, changed replacement identity and replacement start
failure return error and discard the candidate. Cleanup purges owned posts,
timers, subscriptions, services and holds before dlclose, including registrations
made by a failed restore. A successful suspended reload also purges queued posts
and rejects further posts until deferred activation.

There is **no transactional rollback** after the old instance has unloaded. A
candidate failure then leaves that module absent. --force still bypasses service
holds; these tests do not isolate malformed callbacks, make cached service
pointers safe under force. Dependency ordering was implemented separately below.

### Regression evidence

tests/reload.c and the actual DSO fixtures exercise nine groups through the
control-command dispatcher and real dlclose/dlopen, including changing the
on-disk symlink target between versions:

1. Never-started/stopped reload stays loaded without activation.
2. Running reload restores state and starts the replacement.
3. Suspended reload defers start, normal resume does not start twice, and failed
   ordinary resume preserves the suspended state for retry.
4. Deferred start can fail/retry or be stopped before activation.
5. Repeated suspended reload purges posts; unloading never-started replacement
   does not invoke stop.
6. Injected state-allocation failure, oversize and growth preserve the old instance.
7. Failed restore registrations/posts are canceled and never dispatched.
8. Failed replacement start leaves no partially active candidate.
9. Missing restore and changed module identity are rejected after actual file replacement.

The full strict host make check passes, including existing ownership/registry,
runtime-preflight and recovery tests. Reload plus safety suites pass ASan/UBSan
with -Wall -Wextra -Werror -pedantic in build-reload-sanitize. ARM build-tv-v2
compiles with the same strict flags; reload, safety and registry suites pass via
qemu-arm-static -L /tmp/opencode/rootfs/root against TV libc. The previous
reload-stage core MD5 is 20a0e51d753e90fcdc1a9b9f388b4bd1 (superseded by the
dependency-order build below); max required GLIBC remains2.17.

Raw logs: artifacts/analysis/module-reload-2026-10-31. This newer core has not
been staged or tested on the TV. Installed ABI1/recovery artifacts are unchanged;
the previous real-TV preflight result belongs to its recorded earlier hash.

## Dependency-ordered lifecycle follow-up

Actual DSO fixtures reproduced another correctness gap: reverse module-slot
order tried to suspend providers while their consumers were still active, then
the CLI reported ok even when those callbacks refused. A suspended reload can
also move a module into an earlier hole, invalidating ascending-slot resume.

The host now captures required-service and lookup-hold dependencies before
callbacks can release/unpublish them. Snapshots retain service names and stable
provider names; a currently published service resolves to its current owner.
Suspended reload carries that snapshot to the replacement. Ordering no longer
depends on the old slot or instance token. The bounded snapshot is internal host
state and does not change module ABI 2.

- Suspend visits consumers before providers; resume visits providers first.
- Active requires[] declarations protect a provider against normal stop/unload/
  reload, even without a pointer lookup. --force retains its documented bypass.
- A published interface from a suspended provider is insufficient to start or
  resume a required consumer. Modules without suspend hooks remain active and
  prevent their providers from sleeping underneath them.
- Cycles reject the transition before callbacks. A failed callback stops the
  batch and returns error through the actual control dispatcher/socket/CLI.
  Earlier successful callbacks stay transitioned; there is no automatic rollback.
  Callbacks must leave their own state consistent on failure for a later retry.

### Additional regression evidence

tests/phases.c loads real A/B/C modules into slots opposite their dependency
order. Nine groups cover:

1. Consumer-first suspend/provider-first resume, including repeated suspend.
2. Declared dependency protection across stop/unload/reload.
3. Snapshot survival after services unpublish and suspended reload moves slots.
4. Consumer suspend failure keeps providers active and reaches the CLI.
5. Provider resume failure keeps consumers asleep; retry resumes in order.
6. An interface still published by a suspended provider cannot activate a consumer.
7. Runtime lookup edges and a cycle rejected before callbacks.
8. A consumer without a suspend hook blocks its provider's sleep.
9. Service replacement under another module identity resolves to the new provider.

The existing host smoke test had asserted ok for a deliberately disabled laptop
power suspend. It now asserts the real refusal and retained started state; the
host no longer hides that module error. Full strict host make check passes.
Dependency, reload and safety suites pass ASan/UBSan; dependency, reload, safety
and registry suites pass ARM/qemu against TV libc. All builds retain -Werror
-pedantic. Dependency-order stage core MD5: a2fac3c6de45c641aca1eb62971b16b0
(superseded by the control-reply build below);
max required GLIBC2.17. Logs: artifacts/analysis/module-phases-2026-10-31,
including the original failing reproduction. No TV access/deployment occurred.

At the dependency-order stage power still entered RAM inside its ordinary
callback. The sleep-cycle/service follow-ups below correct that integration gap.
Dependency order alone cannot make an independent module run last. Hardware
acceptance and transactional rollback remain open in TODO.

## Control reply safety follow-up

A normal status query with eight long dependency names and 48 published services
exceeded the old 2048-byte dispatcher reply. cmd_status added snprintf's desired
length to its cursor even after truncation, then subtracted that value from the
remaining unsigned capacity. ASan reproduced a heap-buffer-overflow at the next
service append (old core/control.c:88). Raw failing output is retained.

List/status now share a bounded append helper that advances only by bytes actually
stored. Empty commands initialize their reply; one-byte buffers stay terminated
and zero-capacity buffers receive no write. The dispatcher and output formatter
use 16 KiB buffers so normal full-table diagnostics no longer disappear at the
old 2048/1024-byte limits. This is internal core code, with no module ABI change.

tests/control_reply.c exercises full 32-module/48-service tables with capacities
0, 1, 2, 16, 127, 2048 and 4096, checking guard bytes and termination. A forked
host event loop serves a real Unix socket: the client compares complete list and
status replies, including the last module/service, against the complete replies.
It tests the socket dispatcher and formatter as well as direct command handling.

Full strict host make check passes. New tests and lifecycle/reload/ownership
regressions pass ASan/UBSan. New tests pass ARM/qemu with TV libc; all builds
retain -Werror -pedantic. Serialization-stage core MD5 is
9cc83abf3e19527d35a8649093ddc34e (superseded below), max required GLIBC2.17. Raw evidence lives in
artifacts/analysis/control-replies-2026-10-31. No TV access or deployment occurred.

At that stage transport acceptance remained incomplete: output was synchronous, partial
writes are not queued, and backpressure/disconnect/overlong-input/cleanup paths
need dedicated implementation and regression coverage. This change closes the
reproduced memory corruption and ordinary full-table reply truncation only.

## Nonblocking Unix control transport follow-up

The Unix client path no longer writes synchronously from the command callback.
Each accepted fd is nonblocking/close-on-exec and has a bounded 32 KiB queue.
Before executing another command the host reserves room for its complete bounded
reply; otherwise it pauses reading that client until output drains. Pending
requests remain in their input buffer/kernel socket, preserving order without
growing memory. send retains short-write suffixes, retries EINTR, waits for
POLLOUT after EAGAIN and uses MSG_NOSIGNAL. A failed reader closes its own client.

Half-close drains complete queued requests/replies, then closes; EOF fragments
do not execute. Requests have a 511-byte pre-newline limit. Overlong or embedded
NUL lines generate one error and are discarded through newline, preventing the
old overlong-command tail from executing as another command. Parsing is shared
with stdin; stdout delivery itself is still synchronous.

Stop removes all control watches and owned fds and resets parser state. Socket
cleanup checks inode/device identity. Startup refuses regular files and live
listeners, replaces refused stale sockets, and unwinds registration failure.
The core reports startup failure before autoload instead of silently running
without the requested control plane. Quit remains immediate; its reply is best
effort when output is blocked.

tests/control_transport.c adds five real-socket groups:

1. Injected short sends/EINTR/EAGAIN, split and pipelined requests, half-close
   before reading responses, complete ordered delivery and EOF.
2. A real stalled reader with a small server send buffer; a second client keeps
   working. A state-changing command behind 200 queued list replies stays paused,
   then executes after the reader drains them. No replies are lost/reordered.
3. EPIPE injection and repeated disconnects with SIGPIPE's default disposition;
   the host remains available. Accepted fd flags and send flags are checked.
4. Oversized lines with a quit suffix and NUL-containing quit commands are
   rejected whole; following valid and exact 511-byte commands execute normally.
5. Repeated stop/start, idempotent stop, full/near-full watch-table startup
   failure, regular/live/stale/replaced socket paths and identity-preserving cleanup.

Full strict host make check passes. New tests plus lifecycle/reload/worker
regressions pass ASan/UBSan. Both reply and transport tests pass ARM/qemu with TV
libc. A test-only fcntl64 wrapper initially requested GLIBC2.28; flag assertions
now use the kernel flag query directly so the tests run against the real libc.
The core uses accept4 and still requires at most GLIBC2.17. All builds retain
-Werror -pedantic. Transport-stage core MD5: cb7cf002e48feda9fa651f8d43960a8c
(superseded by the sleep-cycle build below).
Logs: artifacts/analysis/control-transport-2026-10-31. No TV query/deployment or
recovery/payload change occurred. Blocked/broken stdout console handling remains
open; these results apply to the Unix socket path, not every output mechanism.

## Internal sleep-cycle engine

oa_module_sleep_cycle is an internal host primitive separating preparation from
final entry and restoration. It uses the dependency order and tracks modules
that were started before the request. Preparation failure skips entry and
restores successful transitions; entry failure also restores them. Previously
suspended modules remain suspended. Entry runs under its validated owner token,
with the original caller attribution restored afterward. Results distinguish
preparation failure, whether entry ran, its raw result and aggregate restoration
failure. Failed provider restoration keeps its consumers suspended; normal
resume remains available for recovery once the cycle returns.

The host rejects nested cycles, load/start/stop/unload/reload (including force),
and manual suspend/resume during a cycle, pinning descriptors/contexts while the
backend and callbacks execute. Missing paired hooks or dependency cycles reject
before callbacks. A pending shutdown rejects the request; shutdown arriving
during preparation cancels entry and still restores this cycle's transitions.

tests/phases.c adds nine groups using actual DSO fixtures and a fake entry:

1. Consumer-first preparation, final entry, provider-first automatic restoration,
   caller/entry attribution and lifecycle/nesting guards.
2. Partial preparation failure skips entry and restores only completed transitions.
3. Failed entry preserves its raw result and still restores modules.
4. Restoration failure stays visible; consumers remain suspended and later retry.
5. Modules suspended before the request remain suspended.
6. Missing hooks reject before any callback.
7. Pending shutdown/stale backend owner reject before preparation.
8. Shutdown arriving during preparation skips entry but restores modules.
9. Dependency cycle rejection before callbacks or backend entry.

Full strict host make check passes. Phase/reload/ownership/control regressions
pass ASan/UBSan. All eighteen old/new phase groups pass ARM/qemu with TV libc.
Builds retain -Werror -pedantic. Latest offline core MD5:
6e642fcc0205c76f75e6b0ffbe5d9230 (superseded by the service build below);
maximum GLIBC requirement remains2.17.
Raw logs: artifacts/analysis/sleep-cycle-2026-10-31. No TV access/deployment.

At that engine stage service/CLI wiring was still missing; the follow-up below
implements it. Other modules still need explicit hooks/device ownership; the
engine does not infer quiescence from an absent hook. Full native standby and
device/network recovery acceptance remain open. Public module ABI is unchanged.

## Coordinated power service and CLI

host.sleep v1 is a host-owned registry service with a per-host context and cycle
entry point. mod_power requires/holds it and captures its entry-owner token in
init. This adds no field to the public module or host API layouts. Standby from
the power service or CLI uses the same coordinated path; manual suspend/resume
only run hooks. The power module has no persistent device/worker to quiesce; its
RAM write now exists solely in the final entry callback, after preparation.

The backend syncs, opens power state, writes mem once, closes and counts only
full writes. Failure always returns through restoration. STANDBY precedes
preparation; ON follows successful prior-state restoration. Failed restoration
emits RESUMING. Notifications are inside the lifecycle guard, so they cannot
recursively enter standby or force-unload executing code. Missing hooks or a
disabled guard prevent power I/O. Failed power publication does not unpublish
someone else's interface and releases its coordinator hold.

Seven new integration groups cover the actual module callbacks, core engine and
CLI dispatcher with real dependency DSOs. Linker wrappers intercept every power
open/write/close/sync and assert the quiescence state, so no actual sleep, disk
sync or TV call occurs. Tests cover service/CLI/manual paths, partial preparation
failure, failed open/write and short write, restoration failure/retry, ordered
notifications/reentry guards, disabled/missing-hook rejection, preexisting
suspended state and failed publication/missing-coordinator cleanup. The full host
socket smoke test also invokes standby refusal and reloads the actual power DSO.

Full strict host make check passes. Power, phase, reload, ownership and control
regressions pass ASan/UBSan. Power and phase suites pass ARM/qemu against TV libc.
All builds retain -Werror -pedantic. Current core MD5:
3357277d0cbbba56820487495e930085; power module b08d7592618b577c3731cbff42815ab6;
max required GLIBC2.17. Logs: artifacts/analysis/power-service-2026-10-31.

No TV access/deployment/recovery change. Complete native device/MCU/network prep,
remaining hooks, frontend recovery-state signaling and hardware acceptance remain
open. Manual resume repairs hooks but does not emit a new power/ON notification.
Configuration remains global/append-only across host init; multi-case test
fixtures override config_get to isolate their enable setting. This integration
result does not validate configuration reinitialization or physical standby.
