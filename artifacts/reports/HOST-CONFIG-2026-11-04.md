# Per-host configuration snapshot — 2026-11-04

`core/host.c` previously appended all initialization files to one global table.
Two hosts could observe each other's settings; later initialization kept old
values and could find the global48-entry table already full. This is a core
host ownership bug, independent of vendor settings semantics.

The configuration entries/count now belong to `oa_host`. Its initialization
clears them with the host state, and `config_get` looks only in that host's
snapshot. Borrowed strings stay valid through other hosts' initialization and
file edits. The existing duplicate policy (first value wins),48-entry bounds,
parser limits, missing-file defaults and runtime skin override remain. Module
reload does not reload configuration. No public module ABI changed.

`tests/registry.c`, already in `make check`, now tests simultaneous hosts with
different files, table capacity, duplicate/empty/default values, isolated skin
override, borrowed pointer stability and reinitialization with a new/NULL/
missing file. Reinitialization explicitly closes the old self-pipe and destroys
the mutex first; it does not promise safe reinitialization of a running host.

## Evidence

`artifacts/analysis/host-config-2026-11-04/` contains:

- `before.txt`: the new regression linked against the committed pre-fix
  host implementation aborts because host2 sees host1's `sample` value.
  Cores were disabled for this deliberate assertion failure. No qemu fault
  or hardware was involved.
- `build.txt`, `host.txt`: strict host build and registry/configuration checks
  pass. Default C11/Wall/Wextra/Werror/pedantic retained.
- `sanitizer-build.txt`, `sanitizer.txt`: focused ASan/UBSan with
  `ASAN_OPTIONS=detect_leaks=1` passes without suppressions.
- `arm-build.txt`, `arm-qemu.txt`: fresh TV hard-float build under
  `build-tv-v3/host-config`, same checks under qemu-arm-static with the
  maintained glibc sysroot, exit0. Registry executable SHA256
  `98b5b0310a7afa75c65542d52651a272eb8b8b16828c439301566ace90360edf`.
- `core-checks.txt`: `make -j4 BUILD=build-host-config check-reload
  check-phases check-power check-control check-skin-selection`, exit0,
  54 ok lines. These exercise the changed private host layout through core,
  module lifecycle, power coordination, Unix IPC and skin selection.

No TV access or deployment. PNG/JPEG files being changed separately were not
included in this milestone. This does not close vendor settings persistence,
runtime configuration reload, native hardware acceptance or whole-system parity.
