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
