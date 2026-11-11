# Read-only runtime preflight

```sh
openaurora --check --modules /buffer/openaurora/modules
```

`--check` exits 0 when the core and every `mod_*.so` in the selected directory
pass, and 1 if any check fails. It runs before host/control-plane startup. It
checks all module files, including ones not selected by config autoload or
`--load`; it does not apply configuration or start those modules.

The preflight reads ELF metadata, validates machine/class/byte order, ABI note
and exported entry symbol, then uses the executable's own ELF interpreter to
check library paths, symbol versions and relocations. It uses the same trace
settings as `ldd -r`: `LD_TRACE_LOADED_OBJECTS=1`, `LD_WARN=yes`, `LD_BIND_NOW=yes`.
It also requests version diagnostics. Loader diagnostic text is checked because
glibc 2.23 can report undefined symbols while still returning zero.

Each loader trace runs in a child with a ten-second deadline and a restricted
environment. `LD_LIBRARY_PATH` is preserved to match normal runtime lookup;
debug, profiling, preload and audit settings are not passed through. No module
entry point, constructor, init/start callback, event loop, control socket,
heartbeat, settings write or hardware operation is started by this mode.

This is a loader/metadata preflight. It cannot establish working device APIs,
source switching, visible scanout, initialization success or service lifecycles.
The actual loader still checks the returned descriptor during normal startup.

## Module authors

Use the standard export macro after defining the descriptor:

```c
static const oa_module module = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof(oa_module),
    /* name and lifecycle callbacks */
};
OA_MODULE_EXPORT(module)
```

This emits both `oa_module_entry()` and a retained `.note.openaurora` section
describing the compiled ABI. Preserve the note when stripping binaries. Legacy
modules without the note are reported as unverifiable and fail preflight; they
are not executed to discover their ABI. All eleven production modules include
the note. Core and modules must be rebuilt together for ABI 2.

## Real-TV verification, 2026-10-31

After the owner's explicit authorization, the current ARM core and eleven
matching modules were temporarily staged under `/tmp`, size/MD5 checked in a
separate connection, and run only with `--check`. **All twelve artifacts passed
on the TV**, exit 0. The temporary directory was removed. Vendor PIDs and boot
ID were unchanged; the persistent core and boot flags were unchanged.

The feature is in the development build and was tested from temporary staging;
it has not replaced `/buffer/openaurora` or the recovery payload. The installed
older audio/panel/source modules still have standalone unresolved `v4l2_open`
and `v4l2_ioctl` references; the current build includes the complete link closure
and passed the strict real-TV check. Full evidence:
`../../artifacts/reports/RUNTIME-CHECK-2026-10-31.md`.

## Offline verification

`make check-runtime` tests valid modules, missing/incompatible libraries,
unresolved functions (including loader exit 0), old ABI, missing note, missing
entry, invalid ELF and empty/missing directories. A valid fixture has a
constructor and lifecycle callbacks that immediately exit if called: successful
preflight verifies they did not execute. No control socket is created.

ARM core/modules also pass with `qemu-arm-static` in a read-only mount namespace
using the TV's extracted `/lib`, `/etc` (including loader cache) and `/vendor/lib`.
ARM constructor/undefined-symbol/ABI/note/entry fixtures pass their expected
success/failure cases. Direct `qemu-arm-static -L` alone does not redirect this
mode's child `execve` of the absolute ELF interpreter; the namespace supplies
the real guest paths, with the host's existing ARM binfmt handler.

## QEMU device simulation

Three separate checks exercise real ARM modules against modeled device
boundaries; they are not hardware acceptance:

- `tools/qemu-fb-real-check.sh` — real `mod_display` over the ARM fb1 model
  (var/fix/mmap, region/MAU, pan, snapshot).
- `tools/qemu-panel-real-check.sh` — real `mod_plat` + `mod_panel` over a
  permissive logged NDAL device boundary; asserts PWM channel/duty and GPIO
  110/111 writes.
- `tools/qemu-hotplug-real-check.sh` — real `mod_plat` + `mod_source` with the
  `DEV_GPIO_GetDescription` model (pins 274..277 active-low); asserts the
  four-pin pulse uses STBC channels 74..77 with values 2 then 3 and no HAL
  GPIO writes.

The panel boundary returns success for unknown TV ioctls and does not maintain
bus state. See QEMU-TV-SIM-2026-11-10 for the observed calls and limits.
