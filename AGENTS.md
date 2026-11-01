# AGENTS.md — tvhack

## Target and objective

Only target `192.168.178.87` (`TELEFUNKEN-TV.fritz.box`, `TELEFUNKEN-TV.local`),
MAC `70:54:b4:fe:70:c6`. Do not touch other LAN hosts.
Telefunken D65U546A / 2199818 / SN 8368599 900143; Vestel_MB230 / MB230_VESTEL;
firmware 0.51.52.3.

Build a bug-for-bug compatible Aurora replacement, modular and reloadable through
IPC/socket/CLI, then a native OmarchyTV UI. Preserve all other TV feature scope.
**Do not port telemetry, advertising, ACR, analytics/tracking, profiling,
advertising IDs, sponsored placements or ad delivery, even as optional modules.**
Local diagnostics and ordinary TV/network functions remain in scope.

## Read first and document authority

Read [TODO.md](TODO.md), [MEMORY.md](MEMORY.md), [FINDINGS.md](FINDINGS.md) and
[HANDOFF.md](HANDOFF.md). Follow the [report index](artifacts/reports/README.md)
to topic evidence. Older reports and archived logs are dated records, not a
queue of experiments to repeat.

- These instructions carry the standing owner constraints.
- MEMORY owns last verified TV/deployment/build state; it is not a live poll.
- TODO owns pending work and acceptance gates.
- FINDINGS owns the correction map; topic reports retain supporting evidence.
- HANDOFF supplies entry points. Do not append competing “latest state” sections.
- Keep implementation, offline tests, on-TV reads and independent hardware
  acceptance distinct. A checkmark only closes the stated, evidenced milestone.
- Update canonical entries when evidence changes; mark superseded claims at
  their source. Do not silently generalize a failed experiment into an API rule.

## TV access: standing owner authorization

**Using the TV to interrogate it is always allowed when the operation is
view-only. No additional confirmation is needed**, including while it is in use.
This includes files, processes, logs, existing state and firmware-understood
GET-only device queries. Establish device-query semantics from firmware first.

While the TV is in use, do not draw, send keys, take hardware ownership, change
settings/network, deploy persistently, restart services, suspend or reboot until
the owner authorizes that work. Temporary /tmp staging for read-only runtime
checks and queries was authorized: separately verify size/hash before execution,
remove staging afterwards and verify no helper is left running.

**Idle test window authorized by owner, 2026-11-01:** the TV is not actively
used and a CEC-compatible Steam Deck is on HDMI1. Temporary project hardware/
CEC/audio tests are allowed now, following firmware/spec evidence and with stock
operation/access restored afterward. This does not enable persistent replacement
or authorize firmware/boot-hook changes. If the owner reports use again, return
to view-only operations. Existing authorization for view-only queries is permanent.

Use `tools/tvssh.sh` (owner preference, including its WoL handling if needed).
It tests reachability and only wakes if necessary. This host's plain `ssh` has
OnConnect hooks; the helper uses `/usr/bin/ssh -F /dev/null`. Offline firmware
reads are preferable to waking the set just to fetch a library or asset.
A UART reader such as `irkey` consumes input bytes and can disrupt the stock UI;
it does not qualify as a harmless view-only observation.

## Firmware first

Before a hardware change, ioctl, probe or reboot, answer the API question from
the shipped binary/driver/kernel and check ARM instructions. Saved Ghidra
prototypes are analysis metadata, often wrong about arguments/returns.
On-set probing confirms understood behavior; it must not discover an API by
guessing. Success return codes alone do not prove hardware effects.

Extract offline with `artifacts/scripts/extract_from_dump.sh vendor:/path`,
`vendor2:/path` or `root:/path`; the source is
`artifacts/dumps/tv_nand.bin`. All device profiles, including /conf, are in the
dump. The helper does not support apdat0: carve 0xA00000 bytes at NAND offset
0xE340000 with dd, then use ubireader if needed. Existing profile copies are
indexed in [artifacts/README.md](artifacts/README.md).

Ghidra: `/opt/ghidra/support/analyzeHeadless`, Java scripts only.
Use `-readOnly -noanalysis` on existing projects; do not open the same project
concurrently. Durable script: `openaurora/tools/re/DumpFirmware.java` accepts
exact symbols or @hex and optional --asm. `openaurora/tools/re/FindCallers.java`
lists a symbol's references (call and data) from Ghidra's reference manager, so
"who calls X" does not need a whole-binary grep; the objdump cache
`artifacts/workbench/aurora.dis` (`objdump -d --no-show-raw-insn`, ~189 MB)
serves instruction-level greps without re-disassembling. Other scripts are in
`artifacts/workbench/ghidra_scripts`.

| project directory under artifacts/workbench | project / program |
|---|---|
| ghidra_proj | Aurora / aurora.elf |
| ghidra_ndal3 | ndal3 / libNDAL3.so |
| ghidra_ntk | ntk / ntkndpdrm.ko |
| ghidra_nondovi | nondovi / ntkdriver_nondovi.ko |
| ghidra_tdga | Tdga / libdirectfb_tdga.so |
| ghidra_eth | Eth / ntkethmac.ko |
| ghidra_preinit | PreInit / PreInit.ko |
| ghidra_hdmiswitch | HdmiSwitch / ntkndphdmiswitch.ko |
| ghidra_alsa | Alsa / ntkndpalsa.ko |
| ghidra_mali | Mali / libmali.so (partial initial analysis; EGL-SCENE records limits) |

Required extracts, analysed projects, toolchains and research evidence have been
moved from ephemeral /tmp/opencode to **artifacts/workbench/**. Use those durable
paths; old /tmp aliases are temporary compatibility only. Verify source hashes
against topic reports. Preserve new evidence under artifacts/analysis, not /tmp.
Aurora is stripped in the ordinary ELF sense but retains rich dynamic names:
use `nm -D`, not plain nm. Kernel .text address offsets must not be applied to
.init.text. External relocation errors are not resolved call targets.

Parity assets come from /vendor2/dynamic_ui, /buffer logos/downloaded tile icons,
settingsDB LauncherItem, slim_i18n and source-name getters. Use the profile-derived
IR map plus ICE/vk XMLs; NEC, RC, ICE and DIAL are distinct code spaces.
Theme geometry and aligned localized string records remain partly undecoded.
Offline `openaurora/tools/re/svg-animations.c` walks the BinSVG pack and
`svg-names.c` resolves all 702 resource IDs/names from ELF svg_name_list, verifying
CRC32 against the pack index. UI-ANIMATIONS records the recovered state bindings;
do not guess names/hashes or conflate category styles with generic launcher tiles.

Vendor UI parity includes animations, transitions, focus/selection feedback,
scrolling and other UI/UX effects, not just static screens. Recover timing,
easing, geometry and interruption behavior from firmware before implementing;
single-frame similarity does not establish animation or interaction parity.

CEC must follow the applicable CEC specification (owner instruction). Read
CEC-SPEC and CEC-AUDIO-CONTRACT before implementation; the former contains dated
claims corrected by the latter. Use CEC 1.3a for base System Audio Control and
1.4 for ARC; do not infer ARC from 1.3a or claim CEC 2.0 support. Keep vendor
interoperability workarounds explicit and do not copy spec violations as parity.
Owner explicitly reaffirmed on 2026-11-01: **where Aurora deviates from the
official CEC specification, fix the behavior in openaurora. Specification
compliance takes precedence over bug-for-bug compatibility for those cases.**
Record the clause/revision, observed vendor behavior and regression evidence;
do not treat an unverified quirk as a proven conformance defect.
Read CEC-TRANSPORT-CONTRACT before native CEC I/O: normal traffic is private
V4L2/kernel CEC, with distinct raw/VPL packets, a mandatory 16-byte receive
buffer, lost/filter status and defective VPL close/reopen paths. These findings
do not authorize bus activity or a UART reader while the TV is in use.
CEC-OWNERSHIP-2026-11-01 records the blocking raw backend and PreInit/cdev
registration. It still requires authority/device lifetime and worker integration.
CEC-LIVE-2026-11-01 records shared-owner queries, the Steam Deck at LA8/PA1000
and spec-based identity validation. TX acceptance/cache are not captured replies
or independent backend acceptance. PID 1737/1899 were the earlier session's
live UI/zombie pair. The review observed a new boot and PID1785; MEMORY owns
the latest sample. Never blindly use the first pidof aurora.elf result.
Raw CEC receive consumes messages; it is not a harmless view-only GET query.
Before audio bring-up read AUDIO-ROUTING-2026-11-01: the verified live main amp
is **ESMT AD82587_D**, not AD82586C. Firmware contains both drivers; subwoofer
helper names do not select the main amp. Decoder volume, speaker output gain and
amp register volume are separate controls/scales. Native GPIO/I2C control and
paired lifecycle remain unimplemented; a cached output handle is not native ownership.
AUDIO-BOARD-2026-11-01 recovers reset GPIO114, MCU mute GPIO208/STBC8 and
I2C bus3/address34; its fixed register03..05 read passed on real hardware.
Board metadata/read access is established; reset/mute/writes, complete init and
standby are not. Inspect /proc/PID/fd targets with readlink or ls, **never cat**:
reading a descriptor can consume input/device data and is not a view-only listing.
AUDIO-VOLUME-2026-11-01 traces UI level through speaker gain and AQ DSP tables;
common/audio_curve.c is tested against all101 live entries and on the TV. It is
not yet integrated with the audio module. Do not confuse decoder opcode33,
speaker gain/table entries and amplifier register03 or use SetMainVolume as a
hardware setter: that vendor wrapper only range-checks its argument.
AUDIO-CURVE-MODULE-2026-11-01 adds audio.curve v1, a reloadable immutable-data
provider with explicit AQ path/profile. Lifecycle/hold/failure tests pass on TV;
this is not hardware gain, complete AQ fallback/profile handling or audio parity.
AUDIO-GAIN-TRANSPORT-2026-11-01 corrects status assumptions: message-backed
Hal_Audio_Ctrl0 can hide a failed send, and speaker ALSA put/get uses a request
cache with ignored lower errors. No such return/readback proves audible output.
A read-only mixer query passed with the stock ALSA_CONFIG_PATH=/vendor/etc/alsa.conf;
the loader-only --check does not validate that runtime resource. Do not run full
audio/platform init under the stock owner just to query this getter.
AUDIO-NETLINK-2026-11-01 resolves synchronous callback/table-copy lifetime and
records the tested codec/native read-only GetOPUGain exchange. Keep tables alive
through the syscall; no command-success ACK exists. Native execution still needs
one coordinated process/socket owner and a blocking worker. Do not bind a
competing same-process HAL socket or replay EINTR. _cmd_capability_p is a setter
and GermBeacon a loader; neither is a harmless query.
That report's session follow-up documents common/audio_client.c. Reuse the
maintained session instead of copying the initial probe's private socket code.
Five offline groups and native read-only query pass; table/selection dispatch is
only injection-tested. Host worker/broker and real audio acceptance remain open.
Serialize outside the loop; inherited children cannot reuse parent's pid binding,
and final OS-close errors consume fd (unlike CEC hardware-close failure).

SPEAKER-GAIN-2026-11-01 distinguishes direct SetOPUGain channel operands
0..0x8000 from UI volume through ALSA speaker out gain/Mod_Cell_SetVol and
AQ/balance/ratio processing. The lower direct setter is injection-tested only;
the broker/service still exposes queries only. GetOPUGain remaps cached100 to
0x8000, so it cannot provide an exact rollback snapshot. No live gain write was
performed; do not use that query to restore a guessed raw-gain experiment.

All measured 32bpp OSD framebuffer layers use **BGRA** bytes (B,G,R,A), including
fb1. Raw framebuffer captures must not be decoded as RGBA. Old tvshot captures
before the correction have red/blue swapped; see UI-PARITY. Keep formats explicit
at every boundary: decoded PNGs and our OAI1 assets/display blit inputs are RGBA,
and the display backend converts them to BGRA framebuffer storage. Saved screenshot
PNGs are already converted to RGB; do not swap their channels again. RGB565 and
other vendor/API buffers require their own established format, not a blanket swap.

EGL-WINDOW-2026-11-01 records direct GPU presentation on initialized fb1:
display1, retained4-byte uint16 width/height native window, FRAMEBUFFER_2=/dev/fb1,
and process-local MALI_MAU_SWITCH=0,0,0,0,0 for the verified primary two-buffer
profile. Owner-observed eight-effect50Hz diagnostic passed with full backing/
region/MAU/pan restoration. This is not cold/native startup or a production
display service. Keep blocking EGL on an owning worker/helper; never put its
destroy/swap calls in the loop or ABI3 quiesce gate. No display.renderer option
exists yet. Pbuffer readback is bottom-up premultiplied RGBA; native BGRA storage
was checked independently. Read the report before creating/swapping a window.

## Build and edit rules

Version control is the local git repository at the workspace root. Commit after
each verified milestone with the evidence in the message (tests run, hashes,
what remains unproven), so a broken experiment can be rolled back. There is no
remote and none should be added. The firmware dump, vendor libraries, toolchain,
Ghidra projects, build trees, partition images and third-party manuals stay out
of history (`.gitignore`); the no-redistribution rule still applies to anything
that is committed.

Use the edit/apply_patch tool for source changes. Avoid Python for editing files.
Default CFLAGS must retain `-std=c11 -Wall -Wextra -Werror -pedantic`.
Do not spawn agents unless the owner explicitly requests delegation.

The TV is glibc 2.23, ARM hard-float, interpreter /lib/ld-linux-armhf.so.3.
Musl builds cannot use the vendor glibc libraries. Recheck symbol versions and
runtime lookup; a compiler's libc version is not compatibility proof.

Keep new ABI 3 ARM artifacts in `openaurora/build-tv-v3`; preserve `build-tv`,
the historical ABI 2 `build-tv-v2`, installed ABI 1 files and recovery images.
Do not mix core/module ABIs. ABI 3 adds the nonblocking quiescence gate.
Use the explicit TARGET=tv BUILD=build-tv-v3 command in
[openaurora/docs/ARCHITECTURE.md](openaurora/docs/ARCHITECTURE.md);
`make tv` hardcodes build-tv and `make clean` also removes build-tv.

## Recovery and process safety

- Never patch signed fs0/ap0/ap1, kernel, xboot, U-Boot or SPI. apdat0 (/conf)
  is unsigned, not risk-free: it is the access/boot hook and can still lock us out.
- Preserve the pristine dump and apdat0_orig.ubifs. Recovery identities and the
  tested 584-byte USB script are in [artifacts/README.md](artifacts/README.md).
  Older raw images lack current maintenance/protections. Do not substitute them.
- Never combine an stdin-streamed upload and asynchronous work in one shell
  AND-list. A trailing & previously left the hook empty and required physical
  recovery. Verify installed size/hash in a separate successful connection.
- A crash normally invokes core_helper.sh, detaches partitions and arms a
  one-second MCU reset. /conf/coredumpnoreset disables that reset path and was
  verified; retain it. /conf/enable_coredump controls writing cores to USB.
- Vendor Aurora feeds the MCU watchdog. Historical wdtctl disable allowed it to
  be stopped without reset, but that operation is currently unauthorized.
  Do not retry NDAL_VideoExt_STBC_* cold: earlier calls reset the set.
  Native supervision is implemented, but real cold takeover remains unproven.
- /vendor/run.sh trigger_reboot waits for /tmp/trigger_reboot and deliberately
  kills Aurora/detaches partitions; it is not a recovery supervisor.
- USB recovery uses hold OK at power-on, FAT32, kylo_usb_update.scr <8 KB ending
  with %. Script success cannot be inferred from blinking LEDs; earlier failed
  attempts have no established parser-defect explanation.
- Never flash sibling firmware. Any official firmware request needs the exact
  VESxxx panel identity as well as model/serial. Keep USB recovery intact.
- No long MITM: previous capture was empty. Any separately authorized targeted
  capture is limited to 60 seconds, with IP forwarding and cleanup.
- Stop only your own background probes/servers. Use exact process identities;
  a pkill pattern can match the invoking shell's later arguments.
- Firmware/dumps/libraries and private keys stay local; do not redistribute.
  A hardware crash or lost UI is not automatically a permanently bricked TV.

## Useful local tools

qemu-arm-static, cross compilers, squashfs-tools, mtd-utils and ubireader are
installed. Paths and source hashes are in the artifact map and architecture.
`--check` verifies loader/ABI metadata without starting modules; it is not a
hardware health test. qemu cannot supply TV devices, and --check's child loader
needs a guest-path namespace rather than qemu -L alone.
