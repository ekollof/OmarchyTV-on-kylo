# TV and offline helper roles

Only 192.168.178.87 is in scope. Read ../AGENTS.md for standing authorization.

| helper | effect / use |
|---|---|
| tvssh.sh | preferred SSH wrapper; probes reachability, WoL if necessary, uses /usr/bin/ssh -F /dev/null |
| ensure_awake.py / wol.py | wake management; not pure offline reads; owner permits tvssh's handling when needed |
| tvshot.py | reads framebuffer over SSH; 32bpp defaults BGRA; screenshot does not prove visible scanout |
| launcher.sh | status vs open/close/sleep differ; latter change UI/power and are not authorized while in use |
| portal_tile.sh | status vs add/remove differ; latter edit live settings DB; not currently authorized |
| wspoll.c | historical client with concrete WebSocket framing/short-read/state bugs; do not execute until repaired/tested; UI-LIVE-OBJECTS used host Node WebSocket API instead |
| tvsim.py / tvsimctl.sh | offline browser/control-plane simulation; no TV hardware emulation |
| pcstatus.py | optional host helper, not TV parity evidence; do not probe other LAN hosts |
| re/rea.sh | optional pinned REA 6.9.0 second-opinion layer; bootstraps under artifacts/workbench/rea and analyzes hashed copies |

`re/rea.sh` is optional; see [re/README.md](re/README.md) and the REA section of
[the tooling guide](../docs/reverse-engineering/tooling.md). It never opens the
canonical `ghidra_*` projects or contacts the TV.

Presence of an action in a helper is not permission to run it. Prefer offline
firmware extraction for code/assets, and firmware-understood GETs for actual
live state. Device readers that consume events (notably UART) can affect the UI.

Raw 32bpp OSD captures use BGRA on all measured layers, including fb1; 16bpp
captures use RGB565. tvshot converts these into ordinary RGB PNGs. Do not swap
corrected PNGs again. Pre-fix captures have red/blue swapped; retain provenance.
`--format RGBA` is an explicit 32bpp override, not the TV default. OAI1 assets and
the display blit API intentionally use RGBA inputs, converted by the display
backend. Other vendor/API buffers need their own format evidence.

Offline colour regression (synthetic reads, SSH/WoL forbidden):
`python3 tools/tests/tvshot_decode.py` from the project root.

## Host setup helper

`check-host.sh` reports host build/analysis tools (compiler, binutils, Python,
Node, qemu, mtd-utils, squashfs-tools, SQLite headers), Ghidra, the ARM
toolchain and ubi_reader. It never installs anything and never reads the TV.
Sources and versions: [../docs/reproduce/dependencies.md](../docs/reproduce/dependencies.md).

## QEMU device simulation

- `qemu-fb-real-check.sh` — real ARM `mod_display` over the modeled fb1
  (var/fix/mmap, region/MAU, pan, snapshot).
- `qemu-panel-real-check.sh` — real ARM `mod_plat` + `mod_panel` over a
  permissive logged NDAL device boundary; asserts PWM and GPIO values.
- `qemu-hotplug-real-check.sh` — real ARM `mod_plat` + `mod_source` over the
  descriptor/STBC boundary; asserts pins 274..277 active-low and STBC
  channels 74..77 values 2 then 3.
- `qemu-vin-real-check.sh` — real ARM `mod_source` over a fake VIN device.
- `qemu-ui-layout.sh` — real UI modules over the replacement `sim_display`.

## TV descriptor probe

- `tv-gpio-describe.c` — static ARM build; issues only the read-only
  `DEV_GPIO_GetDescription` ioctl (`0xc088671f`) for MHL/HDMI hotplug ids and
  prints pin/mode/normal/active. It does not claim, write, export or initialize
  GPIO. Build/stage/verify/remove manually; see DEV-GPIO-PROVIDER-2026-11-11.

The framebuffer/panel boundaries are test models, not kernel or scanout
emulation. See [QEMU-TV-SIM](../artifacts/reports/QEMU-TV-SIM-2026-11-10.md).

## Documentation helpers

- `docs/gen-report-map.sh` — regenerate
  `docs/reverse-engineering/report-map.md` from the canonical report index
  (optional output path argument).
- `docs/check-docs.sh` — check relative links, report-map freshness and that
  every `artifacts/reports/*.md` is indexed. Run it after any evidence or doc
  change; a failure blocks the change (AGENTS documentation hygiene).
