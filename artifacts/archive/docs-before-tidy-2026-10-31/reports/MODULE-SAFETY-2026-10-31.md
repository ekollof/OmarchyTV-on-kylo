# Offline module lifecycle follow-up — 2026-10-31

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
required by the core/modules is 2.17, below the TV's 2.23. Offline core MD5:
`1738708febf70a9c7602be1421390689`.

These checks establish the tested loader/queue behaviours, not hardware parity
or arbitrary-module isolation. Restore errors and reload of stopped/suspended
modules still need defined semantics and regression coverage. Independent native
scanout and live IR interpretation remain open. Installed TV and recovery
artifacts stay on the existing ABI 1 build.

## Scope correction

Owner explicitly excludes **telemetry and advertising** from vendor parity:
no usage/app-click reporting, ACR, tracking/profiling, advertising IDs, sponsored
placements, ad delivery or optional implementations of those subsystems. Current
core/modules contain no such subsystem. Other TV features and local diagnostics
remain in scope. This requirement is recorded in AGENTS, TODO, README and design.
