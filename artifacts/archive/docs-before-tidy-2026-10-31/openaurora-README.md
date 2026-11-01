# openaurora

A modular replacement for the stock `aurora.elf` on the Telefunken/Vestel MB230 set, built as
hot-loadable C modules.

The first target is **bug-for-bug compatibility with the stock firmware, except
telemetry and advertising**. Those subsystems are deliberately excluded, including
ACR, usage/app-click reporting, tracking/profiling, advertising IDs, sponsored
placements and ad delivery. They will not be ported as optional modules either.
Local diagnostics remain available. Once the hardware modules are in
place, the OmarchyTV home screen is implemented as a module on top of them — natively, instead of a
web page inside aurora's browser.

```
make            # build core + modules into build/
make check      # lifecycle tests: load, start, reload (with state handoff), unload, quit
./build/openaurora --check --modules build/modules   # read-only loader/ABI preflight
./build/openaurora --modules build/modules --sock /tmp/oa.sock --load ir --load ui_console
```

Default CFLAGS include `-Wall -Wextra -Werror -pedantic` with C11. Keep those
checks when supplying custom CFLAGS (for example, for a sanitizer build).

`--check` checks library lookup, symbol versions/relocations and compiled module
ABI metadata without running constructors or module startup. It passed on the
real TV for the rebuilt core and all eleven modules from temporary staging;
the installed TV build remains unchanged. See [docs/RUNTIME-CHECK.md](docs/RUNTIME-CHECK.md).

Then, from another shell:

```
python3 tests/client.py /tmp/oa.sock list
python3 tests/client.py /tmp/oa.sock "reload ui_console"
```

See `docs/ARCHITECTURE.md` for the module ABI, the safety rules, and the parity roadmap.
Progress is tracked in the repository `TODO.md` under **P8**.

Native boot recovery uses a separate `oa-supervise` process and a host-loop
heartbeat; see [docs/RECOVERY.md](docs/RECOVERY.md). Independent OSD scanout is
still unproven. The latest offline plane findings are in
`../artifacts/reports/DRM-PLANE-CONTRACT-2026-10-31.md`.
The subsequent [OSD buffer investigation](../artifacts/reports/OSD-BUFFER-CONTRACT-2026-10-31.md)
corrects palette and dumb-buffer assumptions and fixes a missing default fbdev
frame commit. This fix is tested offline and has not been deployed.

The offline development build now uses module ABI 2: core and modules must be
rebuilt together. It adds module-owned worker posts and descriptor-size checks.
The TV and its recovery payload still use the known-good ABI 1 build; nothing
from this ABI change has been deployed.

## Status

| part | state |
|---|---|
| core (loop, registry, events, timers, watches, control plane) | working |
| `ticker` — clock service + state across reload | working |
| `ir` — remote over `/dev/ttyS2` | reader exists; live key mapping/repeat/release unconfirmed |
| `ui_console` — text UI, proves the UI lifecycle | working |
| `display` — OSD layer via fbdev (clear/fill/blit/present) | **verified on the panel** |
| `power` — standby via `/sys/power/state` | raw suspend/WoL proven; native module lifecycle incomplete |
| `settings` — settingsDB read/write | **verified on the set** (read-only default) |
| `panel` — backlight via the vendor PWM HAL | reaches the driver; level overridden by aurora |
| `audio` — volume/mute via the vendor audio HAL | runs; audibly unverified (soundbar owns volume) |
| `source` — input switching via `Hal_Vin` | runs; switching gated behind `source.enable` |
| `ui_fb` — native home screen from the set's own content | **1.6/255 mean error vs the stock portal** |

These are implementation milestones, not complete feature parity. The
2026-10-31 audit reopened incomplete core ownership/ABI, display, input, power,
audio, source and settings work in `../TODO.md`; evidence is recorded in
`../artifacts/reports/TODO-AUDIT-2026-10-31.md`.
