# openaurora — architecture

Goal: a modular replacement for the stock `aurora.elf`, and then a native OmarchyTV UI
built on top of it. This document is the design; `TODO.md` tracks progress.

The owner explicitly excludes telemetry and advertising from vendor parity.
Do not implement usage/app-click reporting, ACR, tracking/analytics SDKs,
advertising identifiers or profiling, sponsored placements, or ad delivery.
Do not carry these over behind opt-in switches. Local diagnostics and ordinary
TV/network functionality remain in scope; no other TV features are descoped.

Native takeover recovery is now implemented by a separate `oa-supervise` process;
see [RECOVERY.md](RECOVERY.md). Independent OSD scanout is still unverified.
The current plane-record contract and correction to the DRM init assumptions are
in `../../artifacts/reports/DRM-PLANE-CONTRACT-2026-10-31.md`.
Further kernel/driver work is in `../../artifacts/reports/OSD-BUFFER-CONTRACT-2026-10-31.md`:
131/132 are palette operations, CREATE_DUMB allocates only 4 KiB, and fbdev
presentation commits through FBIOPAN_DISPLAY even without buffer rotation.
The missing default commit is fixed and tested offline; cold scanout remains open.

## Why modules at all

`aurora` is one 30 MB binary that owns the panel, the remote, CEC, the tuner, the browser and the
UI at once. Every experiment costs a reboot and a reflash, and a bug in one part takes the whole TV
down. Splitting it means:

* a failed UI module is replaced without touching the display or the tuner;
* hardware can be brought up one piece at a time, and each piece is independently testable;
* the UI can be swapped (stock-like → OmarchyTV) without recompiling the TV.

## Dispatch model and remaining reload constraints

**Host callbacks run on a single event-loop thread.** Modules may create workers;
the only host API those workers may call is `post()`. Capture `owner_id()` during
`init()` and pass that token with every post. The queue records the instance
owner, restores it during dispatch, and removes its pending work before
stop/destroy. Posts arriving after that point are rejected. Dispatch pops one
callback at a time so an earlier callback can cancel another owner's work.

Modules must stop and join their workers before stop/destroy returns. Post
userdata is borrowed: retain it until delivery or stop, and reclaim undelivered
userdata during cleanup. Queue cancellation cannot make an unjoined worker safe.
Socket/CLI lifecycle commands run between dispatches; modules must not unload
themselves while their own code is on the stack.

Consequences, all enforced by the host:

| rule | enforcement |
|---|---|
| registrations are owned by the module that made them | `h->cur_module` set around every entry point; `oa_host_cleanup_owned()` after unload |
| a provider cannot vanish under a consumer | `lookup()` records a hold; `unload` refuses while another module holds a service |
| consumers learn about removal | `service/removed` event, fired before the provider's memory goes away |
| reload keeps state | `save()` before unload, `restore()` after load (≤64 KB, per module) |
| ABI version and sizes checked early | ABI 2 checks descriptor magic/version/size before callbacks; the host API is also size-tagged |
| no C++ across the boundary | C structs only |

The loader also refuses to stop a provider while consumers hold its service; otherwise a
stop followed by unload could bypass protection. Full hold tables fail lookup, and failed init
registrations are swept before dlclose. These cases have regression coverage.
`--force` deliberately bypasses service holds and may leave consumers with stale
pointers; it is not a safe-reload guarantee.

ABI 2 changes `post()` and adds the descriptor size and `owner_id()` API. Rebuild
the core and all modules together. ABI 1 descriptors are rejected before the new
size field is read. Larger ABI 2 descriptors are accepted using the known prefix;
shorter descriptors are rejected. This is compatibility validation for cooperating
modules, not isolation from malformed or malicious pointers. The installed TV
build remains ABI 1; the new ARM build stays in `build-tv-v2/` for offline checks.

A segfault inside a module still kills the process. Real isolation would mean
one process per risky module and is deliberately out of scope for now (noted in the roadmap).

## Reuse the vendor HAL, don't rewrite it

The firmware image ships **unstripped** vendor libraries (`/vendor/lib`). `libNDAL3.so` alone exports
121 `Hal_*` entry points covering PWM/backlight, GPIO, audio and video — i.e. precisely the parts of
T1 that would otherwise mean reverse-engineering an AD82586C audio DSP and a PWM controller.

So the rule is: **the HAL is a dependency, the glue is ours.** Modules `dlopen()` the library and
`dlsym()` the entry points, which keeps them loadable (and testable) on a laptop where the library
does not exist — they log "no vendor HAL" and keep running. Details, the exact backlight call chain
and the risks (no reference counting, init order, firmware coupling):
`artifacts/reports/OPENAURORA-VENDOR-HAL-2026-10-30.md`.

## Layout

```
include/openaurora/module.h     the ABI: oa_host_api (host → module), oa_module (module → host)
include/openaurora/events.h     topic strings + payload structs
include/openaurora/services.h   well-known service interfaces
core/host.c                     loop, timers, watches, events, registry, post()
core/module.c                   dlopen/dlsym, lifecycle, unload/reload
core/control.c                  unix socket + stdin command plane
core/main.c                     argument parsing, autoload
modules/*/                      one directory per module, built as mod_<name>.so
```

A module exports exactly one symbol, `oa_module_entry()`, returning a filled `oa_module`.
Use `OA_MODULE_EXPORT(descriptor)` to emit that function and a retained ELF ABI
note. `--check` validates the note and libraries without executing constructors
or module callbacks; see [RUNTIME-CHECK.md](RUNTIME-CHECK.md). The note describes
the compiled ABI; normal startup still validates the returned descriptor.

## Lifecycle

```
        dlopen
          │
        init(api, &ctx)          allocate, keep the api pointer, touch nothing
          │  ── restore(blob) ── optional, on reload only
        start(ctx)               take hardware, publish services, subscribe, add timers
          │
        suspend(ctx) ⇄ resume(ctx)   standby hooks (reverse order on suspend)
          │
        stop(ctx)                drop lookups, release hardware, unsubscribe
          │
        destroy(ctx)             free everything init() allocated
          │
        dlclose
```

Anything the module registered and forgot is dropped by the host afterwards, so a buggy `stop()`
cannot leak table entries.

## Control plane

```
openaurora --modules build/modules --sock /tmp/openaurora.sock --load ir --load ui_console
```

then, over the socket or stdin:

```
list                         status ir
load ir                      start ir          stop ir
unload ir [--force]          reload ir [--force]
suspend | resume | loglevel 3 | quit
```

`reload` is the interesting one: save state → stop → destroy → dlclose → dlopen → init → restore →
start, all while the process keeps running.

## Parity scope and implementation order

A Ghidra census of the stock binary (84,547 functions, see
`artifacts/reports/OPENAURORA-PARITY-2026-10-30.md`) shows the parity surface is dominated by the
broadcast stack, not by the UI: tuner/EPG/PVR/teletext/CI+ together outweigh everything that draws
pixels. The tiers below order implementation; they do not remove features from
the bug-for-bug compatibility target. Telemetry and advertising are the explicit
exception described above.

**T1 — a working display (always required)**

| module | replaces | source of truth |
|---|---|---|
| `display` | DirectFB + `ntkndpdrm.ko`/KMS, OSD planes, z-order, alpha | `OSD-INIT-RE`, `UI-STACK` |
| `ir` | `DEV_UART_IR_*`, `libdirectfb_nvt_uart_ir.so` | `IR-UART` (done) |
| `power` | `StandbyController`, MCU suspend/resume | partial |
| `panel` | `DEV_PANEL_*`/`DEV_PWM` → `Hal_Pwm_*` in libNDAL3 | **skeleton done**, channel needs calibrating |
| `audio` | `DEV_AMP_*` → `Hal_Audio_*`; AD82586C driver stays in the HAL | not started |
| `source` | `ExternalAVSourceManager` / `ICE_External*`, `InputManager` | `SOURCES-CEC` |
| `settings` | `UserPreferences`, `ProfileConfiguration`, `NvramStorage` (~2,000 fns) | `LauncherItem` work |
| `services` | DIAL 56789/56790, WS 7681, DLNA 2870 | largely done standalone |

**T2 — remaining TV features**

`cec` (`NovoCECController` + `ice_cec.c`), `apps` (OMI/`OperaEngine`), `pq` (`PLAT_PQ`,
`PLAT_FRC`), `network`, `bt` (`BluetoothController`), `teletext`/`mheg` (`TeletextIF`,
`MHEGClient`, `fvp::FreeviewPlay`), `tuner`+`epg` (`TunerManager`, `ServiceFinder`), `pvr`
(`DvrController`, `PvrRecordingScheduler`), `ci` (`CHIL`, `INCA`, `CAStackServer`, `CIStackProxy`).

**T3 — later integrations**

`portal`/NEBULA, `voice` (Alexa AVS), `hotel`, global search, OTA, FRC.

HDMI use is an early milestone. Tuner, EPG, PVR, teletext and CI+ remain part of
the replacement target. Cloud integrations must implement their user-facing
function without importing telemetry or advertising paths.

## Phase 2 — native OmarchyTV UI

Once `display` + `ir` + `settings` exist, the UI module draws the OmarchyTV home screen directly
(no Chromium, no DIAL round-trip, no 46-second boot). The current web launcher remains the
reference for look and behaviour, and the CGI/DIAL layer stays available as a fallback.

## Build

```
make                 # host build into build/ (vendor HAL stubbed)
make check           # lifecycle tests
make arm             # ARM + musl, stubbed — sanity/size only
make vendor          # extract libNDAL3.so / libNDAL_EX.so from the firmware dump
make tv TV_CROSS=... # ARM + glibc, links the vendor HAL
make install PREFIX=/usr
```

## Toolchain: glibc, matching the TV

The TV runs **glibc 2.23** (`aurora.elf` INTERP `/lib/ld-linux-armhf.so.3`; `/proc/<pid>/maps` shows
`/lib/ld-2.23.so` + `/lib/libc-2.23.so`; no musl anywhere in the image). The vendor libraries are
glibc binaries, so the TV build must be glibc too — a musl process cannot `dlopen()` them.

**Working toolchain:** *Arm GNU Toolchain 10.2-2020.11, `arm-none-linux-gnueabihf`, glibc 2.31*.

```
make tv TV_CROSS=/path/to/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc
```

2.31 is safe; the hard limit is **2.34**, where `crt1.o` starts requiring
`__libc_start_main@GLIBC_2.34`, which the set does not have. Verify any candidate build with:

```
readelf -V build-tv/openaurora | grep -oE 'GLIBC_[0-9.]+' | sort -uV | tail -1   # must be <= 2.23
```

One trap we hit: `fcntl()` is versioned `GLIBC_2.28`, so the core uses `pipe2()` and
`SOCK_NONBLOCK` instead. Current maxima: core 2.17, modules 2.4.

**Verifying without the TV** — run the ARM build against the TV's own libc under qemu:

```
qemu-arm-static -L /path/to/extracted/rootfs build-tv/openaurora --help
LD_LIBRARY_PATH=vendor/lib qemu-arm-static -L /path/to/extracted/rootfs \
    build-tv/openaurora --modules build-tv/modules --sock '' --load panel
```

That is how `mod_panel` was proven: it loads, reaches the vendor HAL, and the HAL prints
`Can not open /dev/nvt/ntkled` because a laptop has no LED device node.

## Linking the vendor HAL safely

* Only modules listed in `VENDOR_MODULES` (currently `panel`) carry the dependency; the rest stay
  pure and load anywhere.
* `libNDAL3.so` needs data symbols from `libNDAL_EX.so` (e.g. `gs32Ndp_PQ_fd`) and imports that the
  vendor *app* provides (`v4l2_open`), so modules are `dlopen`ed with **`RTLD_LAZY`**: function
  symbols resolve on first use, data relocations are still eager — hence `libNDAL_EX` must be linked.
* `scripts/fetch-vendor.sh` pulls the closure from the dump: `libNDAL3`, `libNDAL_EX`, `libasound`,
  `libmhash`, `libnteec`.
* RPATH is `/vendor/lib:/vendor/lib/libs` because that is where the set keeps them.

## Testing philosophy

The IR, display and CEC modules are testable *without a TV* because they fail soft: `ir` logs
"no such device" and keeps running, so the lifecycle stays exercisable on a laptop. Everything that
needs real hardware gets an explicit `suspend`/`resume` hook so it can be tested by hand.
