# Runtime library preflight — 2026-10-31

> **Status: Current loader/ABI verification evidence, including staged real-TV pass. No hardware startup/scanout/behavior guarantee or persistent ABI 2 install.** See [report index](README.md) and
> [current corrections](../../FINDINGS.md); old plans do not authorize TV changes.

Owner requested an on-TV dependency audit and a read-only `--check` mode, then
explicitly authorized temporary staging to test it on real hardware. Persistent
TV deployment, boot settings, services, networking and UI controls were not changed.

## Installed-build audit

Executed the TV's `/lib/ld-linux-armhf.so.3` in relocation-trace mode against the
installed core and eleven modules. Library files were found. The older installed
`mod_audio.so`, `mod_panel.so` and `mod_source.so` each reported unresolved
`v4l2_open` and `v4l2_ioctl` from `libNDAL_EX.so` when checked independently.
The loader still returned 0. These are real strict standalone-check failures,
not missing library files. The installed build uses lazy relocation; no normal
module startup was run for this audit.

The current Makefile's vendor dependency closure already explicitly links
`libv4l2.so.0` and `libv4lconvert.so.0` for all five vendor modules. The rebuilt
modules pass strict relocation checking. The older installed artifacts were
left in place, because the authorization was for diagnostics and temporary staging.

## Implementation

`core/check.c` implements the new mode before host initialization. It reads the
executable's PT_INTERP and uses that exact loader for the core and every module.
Trace children use `LC_ALL=C`, `LD_TRACE_LOADED_OBJECTS=1`, `LD_WARN=yes`,
`LD_BIND_NOW=yes`, `LD_VERBOSE=1`, and the caller's optional LD_LIBRARY_PATH.
Profiling/audit/debug/preload variables are not inherited by trace children.
Output and exit status are checked, with a ten-second per-child deadline.

Before tracing each module, file reads verify ELF machine/class/byte order,
the compiled ABI/size note and an exported defined `oa_module_entry` function.
`OA_MODULE_EXPORT()` emits the note and entry symbol without changing module
descriptor layout or runtime ABI 2. Legacy/no-note modules fail as unverifiable.
No dlopen, module entry, constructor or lifecycle callback is used for discovery.
No control socket or heartbeat is created and no HAL, display, input, networking
or settings API is called. Hardware functionality remains outside this check.

## Tests

- Strict host `make BUILD=build-strict check` passes the lifecycle, registry,
  safety, display, runtime-preflight and eleven recovery integration cases.
- Six runtime-preflight groups cover constructor/startup avoidance and no socket,
  ABI/note/entry/ELF validation, undefined functions despite loader exit 0,
  missing DT_NEEDED dependency, wrong symbol version, and empty/missing directories.
- ARM core/modules compiled with `-Wall -Wextra -Werror -pedantic`. Maximum
  required GLIBC version remains **2.17**, within the TV's 2.23.
- The full ARM preflight passes in qemu using a read-only namespace with the
  TV's actual libraries and loader cache. Omitting `/etc/ld.so.cache` initially
  exposed a missing sqlite lookup in the incomplete test namespace; mounting
  the real cache reproduced the TV's successful resolution. No library/path
  assumption was changed to disguise that test-environment difference.
- ARM fixtures separately verify constructor avoidance (success), undefined
  function, old ABI, missing ABI note and missing entry symbol (expected failure).

## Real-hardware run and cleanup

Staged only the core and eleven production modules, **401,200 bytes**, under
`/tmp/openaurora-check.9xcvwi`. All twelve MD5s and byte counts matched in a
separate successful SSH connection before execution. Core MD5:
`424ef6ea213cb706acb51a7adb39f778`.

```text
/tmp/openaurora-check.9xcvwi/openaurora --check --modules /tmp/openaurora-check.9xcvwi/modules
12 artifacts: OK
Runtime preflight PASSED
exit: 0
```

Afterward, deleted only the staged files and removed the temporary directories.
The owner requested use of `tools/tvssh.sh`; it was inspected and used for
cleanup/status. It reported the TV already awake, so no WoL packet was sent.

| observation | before | after |
|---|---|---|
| boot ID | `ebb0aaaa-72a2-4410-9f8d-95b9249f8a90` | same |
| uptime, seconds | `2640.29` | `2709.33` |
| vendor aurora PIDs | `1899 1737` | same |
| persistent core MD5 | `a898384df076572cf377749f25427689` | same |
| native openaurora process after check | — | absent |
| temporary staging | created for test | removed |
| safe/crash guard; native boot | existing safe state | oa_safe + coredumpnoreset present; oa_boot absent |

This establishes actual TV loader/ABI compatibility for the rebuilt artifacts,
not native display or hardware parity. The known-good installed ABI 1 build and
recovery images remain unchanged. `--check` is available in the new development
build and has been exercised on the TV, not persistently installed there.

Logs: `artifacts/analysis/runtime-check-2026-10-31/` contains installed lookup
paths, installed symbol errors, the complete successful on-TV trace, cleanup
observations and the qemu success/failure traces.
