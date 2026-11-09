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

Priority (owner, 2026-11-08): platform/hardware support and stock-UI parity come
first, so the replacement is dependable. The native Omarchy shell is a skin on
top and must not be used as evidence of platform or stock-UI parity.

## Read first and document authority

Read [TODO.md](TODO.md), [MEMORY.md](MEMORY.md), [FINDINGS.md](FINDINGS.md) and
[HANDOFF.md](HANDOFF.md). Follow the [report index](artifacts/reports/README.md)
to topic evidence. Older reports and archived logs are dated records, not a
queue of experiments to repeat. The [documentation index](docs/README.md) is the
navigation layer for the compiled reverse-engineering set; the report index
remains the chronological authority.

- These instructions carry the standing owner constraints.
- MEMORY owns last verified TV/deployment/build state; it is not a live poll.
- TODO owns pending work and acceptance gates.
- FINDINGS owns the correction map; topic reports retain supporting evidence.
- HANDOFF supplies entry points. Do not append competing “latest state” sections.
- Keep implementation, offline tests, on-TV reads and independent hardware
  acceptance distinct. A checkmark only closes the stated, evidenced milestone.
- A TODO.md item can only be checked off when it is properly implemented,
  tested, and, for parity work, compared against the firmware. An unverified
  helper, harness or dated observation does not close the item; record the
  evidence and the remaining limits in the entry.
- Update canonical entries when evidence changes; mark superseded claims at
  their source. Do not silently generalize a failed experiment into an API rule.

### Documentation hygiene

`docs/reverse-engineering/` is maintained navigation over the evidence, not a
snapshot. Keep it current in the same change that changes the evidence; a
docs-only pass is not acceptance evidence.

**On every evidence change** (new/corrected report, binary, hash, address,
contract, gate or resolved question):

- Update the canonical dated report and [FINDINGS.md](FINDINGS.md) first.
- Update the matching section of `docs/reverse-engineering/subsystems.md`;
  update `binaries.md` for a new/changed binary, path, MD5 or Ghidra project;
  `tooling.md` for a new search/extraction step; `glossary.md` for a new term;
  `open-questions.md` when a question is answered or opened.
- Add/rename/remove the entry in the
  [report index](artifacts/reports/README.md) and regenerate the topic map:
  `sh tools/docs/gen-report-map.sh`.
- Update MEMORY only for verified state, TODO only for work/gates and HANDOFF
  only for entry points. Do not create a competing “latest state” in docs.
- Run `sh tools/docs/check-docs.sh`; broken relative links, a stale report map
  or an unindexed report fail the change.

**Report status.** New reports, and reports touched for a correction, carry a
`Status:` line (current / superseded by X / archived), a `Last verified:` date,
`Supersedes:`/`Superseded-by:` when applicable, and the evidence class
(firmware / on-TV / host / qemu). Do not leave a corrected report marked
current.

**Archiving.** `artifacts/archive/` is the only archive; never delete evidence.
Archive a report or plan when it is superseded, its claim is corrected, the
approach is abandoned, or it no longer describes the current contract:

- keep the dated filename under `artifacts/archive/`, add a top
  `SUPERSEDED BY: <current doc>` line, and record the reason/replacement in
  `artifacts/archive/README.md`;
- remove it from the active report index and topic map, or mark it historical;
- never cite an archived item as current evidence. Reference it only as
  archaeology, with the replacement named;
- raw `artifacts/analysis/` evidence stays for provenance; if its report is
  archived, link that report as historical;
- do not resurrect a superseded contract without re-verifying the firmware and
  recording the new evidence.

Do this at every milestone/commit that changes evidence, when a report
supersedes another, when a hash/address changes, and before a handoff.

## TV access: standing owner authorization

**Current owner override, 2026-11-05: "You can test on the TV if you need to"
and "You can also use fb1." Temporary display tests are reauthorized, superseding
the earlier fb1 exclusion and offline-only restriction. Preserve/restore stock
operation, display state and access. This does not authorize persistent
replacement, boot-hook changes, stopping stock Aurora or watchdog takeover.**

**Owner update, 2026-11-06: "you have full access to the TV." Temporary
controlled display ownership is authorized, including stopping stock Aurora and
a recovery reboot for a test. Preserve and verify stock restoration, remove
staging, and never leave the watchdog disabled or a helper running. Persistent
replacement and boot-hook changes remain separate gates.**

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

**The shipped vendor firmware is the source of truth for hardware and vendor
behavior.** The TV/hardware is the black box used to confirm an already
understood contract, never to discover one by guessing. Do not infer an ioctl,
register, call signature, return convention, bit mask or task order from a
decompiler alone, a function name, a nearby symbol, or a successful return
code. Before implementing or probing, recover the fact from the vendor binary,
driver, kernel or library and check the actual ARM instructions.

Every hardware/API question must go through Ghidra (read-only) first:

- record the exact source path, MD5 and address in the report;
- decompile the exact function and inspect the instructions around it;
- follow the callers/callees and data references (`FindCallers.java`);
- verify arguments, return values, stack/struct-return conventions, Thumb/ARM
  state and signedness from instructions, not the decompiler prototype;
- cross-check symbols with `nm -D`/`readelf -s`; use the real exported names.
  If a helper is genuinely unnamed, say so and name it only by its recovered
  behavior rather than presenting an invented symbol;
- compare the implementation and its tests against that recovered contract,
  including failure branches and ignored returns.

Saved Ghidra prototypes are analysis metadata, often wrong about
arguments/returns. On-set probing confirms understood behavior; it must not
discover an API by guessing. Success return codes alone do not prove hardware
effects.

Extract offline with `artifacts/scripts/extract_from_dump.sh vendor:/path`,
`vendor2:/path` or `root:/path`; the source is
`artifacts/dumps/tv_nand.bin`. All device profiles, including /conf, are in the
dump. The helper does not support apdat0: carve 0xA00000 bytes at NAND offset
0xE340000 with dd, then use ubireader if needed. Existing profile copies are
indexed in [artifacts/README.md](artifacts/README.md).

Ghidra: `/opt/ghidra/support/analyzeHeadless`, Java scripts only.
Use `-readOnly -noanalysis` on existing projects; do not open the same project
concurrently. Durable scripts:

- `openaurora/tools/re/DumpFirmware.java` — exact symbols or `@hex`, optional
  `--asm`, data dumps and function-name search. This is the default.
- `openaurora/tools/re/FindCallers.java` — a symbol's call/data references; use
  it for "who calls X" instead of a whole-binary grep.
- `openaurora/tools/re/DecompileRaw.java` — for an exact Thumb entry that
  Ghidra did not define as a function (for example a task pointer). Run it only
  on a **disposable copy** of the project; it creates the function there and
  prints the decompilation. Never modify the canonical project for this.

The objdump cache `artifacts/workbench/aurora.dis`
(`objdump -d --no-show-raw-insn`, ~189 MB) serves instruction-level greps
without re-disassembling. Other scripts are in
`artifacts/workbench/ghidra_scripts`.

Before calling a decompiled result "the firmware contract", check the exact
instructions at least once; when Ghidra and objdump disagree, the instructions
win. Keep raw decompiles/disassembly under `artifacts/analysis` and cite the
addresses. A fake-device test proves the modeled order/arithmetic; it does not
prove the hardware effect.

| project directory under artifacts/workbench | project / program |
|---|---|
| ghidra_proj | Aurora / aurora.elf |
| ghidra_ndal3 | ndal3 / libNDAL3.so |
| ghidra_ndal_ex | ndal_ex / libNDAL_EX.so |
| ghidra_extin | Extin / ntkndpextin.ko |
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
The OmarchyTV reference implementation is <https://gitlab.com/piermark/omarchytv>
(Android/webOS/Tizen). The checked-in `launcher/omarchy/` snapshot is dated; use the
GitLab tree (pin the commit in the report) for current UI/UX. The production
Omarchy UI is native `ui_engine` drawing: do not embed or launch the browser app.
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
AMP-GPIO-QUERY-2026-11-04 establishes direct GET670d/670f on already-open
GPIO114. Do not substitute Hal_Gpio_GetDirection: it first issues670a, which
marks/records the global pin open. Ordinary-pin Hal_Gpio_Read writes sysfs
export first. These wrappers are not view-only. The maintained query validates
char132:0, never claims/exports/configures and passed on TV; direction is cached
(kernel0 output/1 input), level may be the output latch. This is not independent
voltage, ownership or exact rollback evidence. Do not initialize GPIO, issue
global6701/6702, or fall back to pin claim after a failed GET under stock Aurora.
No MCU/STBC getter or reset/mute write was tested by that query.
AMP-STBC-2026-11-04 recovers the separate MCU GPIO codec. The NDAL getter
takes pin plus TWO uint32 output pointers (three arguments); its transport
boolean ignores the kernel's separate device-result byte. Raw outputs are
reply bits23..16/15..8, not a proved direction/level pair. The new helper requires
an explicit transport callback and validates device status; no native fallback
or MCU operation is tested. OpenDevice(0,10) maps video24 and installs a
process-global fd, not a per-module lease. AMP-VIDEO-OWNER ties video24 to
extin's simple comp_open/comp_release using section-specific .rodata relocations;
Ghidra's missing data refs do not mean unused callbacks. The mutating
comp_release_func belongs to input selection, not fd close. Full libv4l2 wrapper
behavior and coherent/cold authority remain unresolved. Preserve the cold-call
warning and do not infer historical reset causes from the recovered signature alone.
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

AMP-INIT-2026-11-04 records the injected AD82587 Init sequencer and29952
native Init/Reset/Mute comparisons. Its return1 means the vendor write loop
completed,0 means a write failed; negative EINVAL rejects bad arguments. GPIO
failures are ignored by native Init and caches update only on nonzero GPIO
returns. Adapt errno-returning transports to that boolean contract; never call
this blocking sequencer on the host loop or equate its return with hardware
ownership/audible acceptance. No native backend or paired shutdown is supplied.

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
what remains unproven), so a broken experiment can be rolled back. The existing
origin remote is git@github.com:ekollof/OmarchyTV-on-kylo.git. The owner explicitly
authorized pushing progress on 2026-11-03. Use that remote; do not add another.
The firmware dump, vendor libraries, toolchain,
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

## Review and acceptance lessons

- Review the fix, not only its report. Reproduce the failing case, then run the
  corrected path at the reviewed commit/build. Stale counts or logs from before
  the fix are not evidence. Keep focused sanitizer runs and the full suite
  distinct; neither substitutes for the other.
- A regression must distinguish the defect. If a zero-valued input hides the
  bug, add a nonzero case; never rewrite expectations just to make the model
  match. Decompiler output and resource coordinates can invert a subtraction or
  hide a repeated term; verify the actual instructions and a contrasting case
  (for example, `SVGGrid` repeats the left inset in the right edge, which a
  zero-inset portal cannot detect).
- When a layout path both consumes and mutates state, record which values are
  captured before the mutation and which are read after it. Do not derive a
  column step and the insets it later overwrites from the same post-update
  struct; model first-paint and settled/redraw states separately where they
  differ.
- Resource coordinates are not runtime coordinates. Before deriving insets from
  SVG/rectangle data, verify the renderer's transform-before-storage path. A
  negative or absurd offset is a prompt to inspect transform/container
  semantics, not to accept the arithmetic or declare the screen dead.
- Control-plane consumers must validate provider-reported axes and byte counts
  against the service contract before any multiplication, allocation, hash walk
  or copy. Test oversized, zero, negative and `INT_MAX` values, and release the
  provider buffer on every error path. This protects against replacement
  providers, not just the shipped one.
- A differential that only compares the vendor to a host library is incomplete.
  Execute the real module/service on the same corpus under the target ABI
  (ARM/qemu or TV), including rejection, empty-output and release/reload
  invariants. External oracles are supplementary.
- An explicitly requested test input that is missing is a failure, not a skip.
  Keep explicit inputs separate from optional auto-discovered corpora; private
  assets stay ignored and hash-verified.
- A CPU-only on-TV run is not display/HAL acceptance. Record which device paths
  were opened or avoided, verify staging removal and before/after identity, and
  do not infer fb1/region/MAU/scanout ownership from a successful file test.
  fb1 authorization does not turn a CPU test into display acceptance.

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

## Offline ARM crash diagnosis

A qemu reference crash is a local harness/guest failure, not evidence of a TV
fault or a reason to probe devices. Keep the exact executable path/hash, command,
last completed group and first fault PC/registers before changing anything.
Use a fresh isolated `BUILD=build-tv-v3/<review-name>`; the maintained
`artifacts/analysis/tab-navigation-2026-11-03/verify.cjs` accepts an optional
binary path and prints its SHA256. Do not mix a root build with tab-policy or
keep rerunning stale binaries while guessing at calling conventions.
Its current default is build-tv-v3/amp-init/tests/tab_policy_firmware
and it requires45 groups, including6000 AD82587 profile images/38 write failures,
2312 profile payload and29952 Init control comparisons. Build that target first or pass an explicitly
rebuilt path. Older binaries cannot certify newly added groups. AUDIO-AMP-PROFILE
and AUDIO-AMP-NATIVE/AMP-INIT record the commands/scopes.

Use GDB for a reproducible first fault: inspect PC, CPSR/instruction state,
registers, nearby instructions and backtrace through qemu's gdbstub, then check
the shipped ARM call site. `/usr/bin/gdb` accepts ARM and reads our ARM ELF here.
The toolchain's arm-none-linux-gnueabihf-gdb exists but currently cannot launch
because libncursesw.so.5 is missing; use the working host GDB rather than adding
ABI-incompatible ncurses symlinks. Do not claim a historical crash cause when
its core/binary is unavailable and the current build does not reproduce it.

`BLX` to an even PLT slot enters ARM state; a Thumb trampoline there is wrong.
Check hidden struct-return pointers and actual VFP argument/result registers
from instructions; saved decompiler signatures are not ABI proof. The animation
call at10bbfd4 returns double in d0; start at10bc002 takes float in s0. Preserve
argument registers (including r3 for four-argument calls). Reference stub
installers validate their own registered mapping: never use an adjacent mapped
page as accidental spill space. Direct fragment copies still need explicit
extent checks; a green harness is not a general memory-safety proof.

Keep borrowed animation bytes separate from owned UString handles. Source
length/substrings use UTF-8 character walks, not strlen/pointer+1; empty-name
length-1 wraps and must remain bounded. NULL names construct empty strings in
firmware. Parse duration before destroying substring/name, preserve comparison
operand order and signed container-count semantics, and expose these operations
in tests instead of hiding them inside shared stubs. A controlled string fixture
does not validate the full native UString implementation or visible animation.
See ARM-HARNESS-REVIEW-2026-11-03 for evidence and reproduction.

UI-REVIEW-2026-11-04 corrects two further fixture traps: the image supplier
writes32 bytes of UIImageResource, not the16 bytes its old fake token used.
Establish complete object capacity/alignment from actual writes and test every
byte. Arithmetic stubs must not invent helper semantics: every zero divisor
in the shipped signed divider reaches unconditional raise(8). Nonzero-range
reference tests now execute the real divider; zero-range replacement EDOM is
an explicit error-policy deviation. Do not report that as firmware fault parity.

WORK-REVIEW-2026-11-04: SVGWindow+6c (including scrollbar skin caches) is
the four-state UFA/FA/UFI/FI index, not orientation. Use all four cache/name
slots. Test changing getter results: renderBar calculates from the first
window query and places from the second. Keep add/sub/mul wrap explicit and
execute the real divider in reference tests. Image+0xb0 is read with ldrb;
nonzero adjacent bytes must not change its truth. Exclude advertising data
from SQL projection and object conversion, not merely network delivery.

## Useful local tools

qemu-arm-static, cross compilers, squashfs-tools, mtd-utils and ubireader are
installed. Paths and source hashes are in the artifact map and architecture.
`--check` verifies loader/ABI metadata without starting modules; it is not a
hardware health test. qemu cannot supply TV devices, and --check's child loader
needs a guest-path namespace rather than qemu -L alone.
