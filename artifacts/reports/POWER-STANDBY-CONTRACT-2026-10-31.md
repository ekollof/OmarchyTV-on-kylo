# Native standby contract — firmware recheck, 2026-10-31

Status: **current static evidence and offline host-cycle tests**, not hardware
acceptance or a complete native standby implementation. No TV access, device
calls, deployment or recovery changes.

Source: this TV's extracted vendor:/com.nvt.idtv/aurora.elf, MD5
64c252340d3af6c5d66072611b7bad7a. The saved Ghidra program reports the same hash.
DumpFirmware.java ran read-only/noanalysis with decompilation and ARM instructions.
Raw output: ../analysis/power-2026-10-31. Saved function prototypes are not trusted.

## Actual ordering and ownership

| function | address | role |
|---|---|---|
| ICE_HardwareSuspendToRam | 0x7531b4 | public wrapper, forwards two arguments |
| ice_hardware_SuspendToRam | 0x752f70 | outer preparation, RAM write, outer restoration |
| idtvSuspend | 0x752824 | device/network/app preparation before write |
| idtvResume | 0x752b88 | device/network/app restoration after write |
| PLAT_SYS_MCPU_MonitorTask_Reset | 0x8c5244 | changes internal monitor task state under mutex |
| PLAT_SYS_SetMcuWakeUpTime | 0x8c35dc | packs fields for NDAL STBC call |

The outer function parks CI, disables IR, conditionally disables remote reception
for the boot-logo path, and shuts down NVRAM. It converts/programs wake time,
syncs, opens/caches the power-state fd, sets monitor state, resets CI readiness,
then calls idtvSuspend **before** writing three bytes to the power fd. Immediately
after write it records video resume time and calls idtvResume. Finally it restores
monitor state, initializes NVRAM, cleans UART receive state, enables IR and sends
middleware key 0x21f. The write result is not checked on this path. Even open
failure goes through outer restoration, so reaching that restoration is not
proof that RAM sleep occurred.

Instruction anchors: idtvSuspend call 0x75308c; write setup/call
0x753090–0x753096; video timestamp and idtvResume 0x75309a/0x75309e.
The boot-logo remote enable counterpart is in the resume stage, not simply an
unconditional inverse immediately following write.

idtvSuspend also:

- Coordinates an app/network command on a conditional path and checks app state.
- Selects MCU Wi-Fi power policy from wake preference, dongle type and connection;
  the non-wake path can suspend and unload Wi-Fi components.
- Backs up picture settings, programs MCU WoL/WoW, ejects Bluetooth, selects PHY
  policy and conditionally enters Wi-Fi standby.
- Handles boot-logo/backlight state, powers down the amplifier and LNB, suspends
  demod/tuner, closes VBI, runs conditional commands and clears an OSD plane.
- Coordinates remote devices, IR simulation, UART send synchronization, Bluetooth
  shutdown and pending LED timing. A remote-device condition can keep it waiting.

idtvResume resumes UART send, updates wake reason/key, triggers tuner recovery,
resets Wi-Fi and processes conditional app/boot-logo paths. It restores PCR state,
HDMI hotplug/backlight/OSD state, amplifier, source-specific VBI/teletext, HDCP and
MCU read/write database state, remote devices and further app wake decisions.
These are real responsibilities, not empty middleware notification hooks.
Their internal state dependencies are not reusable independently merely because
dynamic symbols exist in Aurora. All non-telemetry/ad feature scope stays intact.

## Wake/network policy: preserve the distinctions

ICE_HardwareWakeOnNetworkSettings at 0x74e618 forwards its **second** argument to
_HW_EnableWoL (0x74e3d8) and its **first** argument to _HW_EnableWoW (0x74e538).
Those helpers update cached policy, not the MCU immediately. Defined globals are
_gb8EnWoL at 0x17964e4 and _gb8EnWoW at 0x17964e8. Getters at 0x74e4b8 and
0x74e654 read those policy values. Do not collapse the two arguments into one
undifferentiated wake flag.

Literal pools independently confirm the global association: 0x752b08 points to
_gb8EnWoW and 0x752b80 to _gb8EnWoL; the setters' corresponding pointers are at
0x74e5b8 and 0x74e458. Raw objdump bytes are in oa-power-policy-literals.txt.

idtvSuspend programs PLAT_SYS_SetMcuWoLOnOff/WoWOnOff from that policy and calls
PLAT_SYS_SetEth_PHY_Power_STR with the predicate **WoL == 0**. The PHY wrapper
(0x8c6ca8) opens AF_INET/SOCK_DGRAM, builds an eth0 ifreq whose data points at
four words {1, bool(argument), 0, 0}, and issues private ioctl 0x89f0. The target
driver follow-up below resolves this opcode's next-suspend policy and polarity;
it is not a generic immediate PHY power API.

The WoL/WoW platform wrappers call NDAL_VideoExt_STBC_*; the WoL wrapper at
0x8c7440 forwards its incoming r0 and returns the saved scalar result at
0x8c7458, despite Ghidra's void prototype. Wake-time packing likewise returns
the saved NDAL scalar at 0x8c363e. Do not import guessed prototypes from this
decompile, or invoke the cold STBC path: prior hardware resets remain relevant.

## Target Ethernet driver follow-up

vendor:/modules/ntkethmac.ko was extracted from the NAND-backed offline helper;
the extracted and imported copies both have MD5 2efe1ee138ee29c2d490f6e4a667a38d.
New Ghidra project /tmp/opencode/ghidra_eth, Eth / ntkethmac.ko. Its ELF .text
symbols map at +0x10028 in this import, not the offsets used for other drivers.

| symbol | ELF .text offset | Ghidra address |
|---|---|---|
| synopGMAC_linux_do_ioctl | 0x726c | 0x17294 |
| nvt_private_ioctl | 0x6ec0 | 0x16ee8 |
| synopGMAC_suspend | 0x6354 | 0x1637c |
| synopGMAC_resume | 0x5c90 | 0x15cb8 |

The ioctl dispatcher checks the device/private state and IFF_UP, then routes
0x89f0 to nvt_private_ioctl. The private handler reads ifreq.data at ARM offset
0x10 and copies 16 bytes from userspace. Opcode 1 accepts value 0 or 1 and stores
phy_pwd_down; invalid values return failure. It does not immediately power down
the link. Instruction anchors: copy length 0x16f3c, value compare 0x17034–0x17044;
the two value paths store the flag and log next-suspend policy.

During driver suspend, the running-interface path detaches the device, disables
IRQ/work/timers, stops DMA and reclaims descriptors. It enables PMT/magic-packet
detection and MAC power-down, then checks phy_pwd_down: zero keeps PHY power;
nonzero calls nvt_internal_PHY_power_down. Consequently Aurora's WoL==0 predicate
means a WoL-enabled standby programs {1,0,0,0}, preserving PHY power while the
driver arranges MAC wake detection.

Resume restores clocks, invokes SysPlat_Init_Ethernet_Phy, reinstalls MAC address
and DMA queues/configuration, refills RX, disables MAC power-down, reattaches the
device and restarts IRQ/timer handling. This is the kernel device-PM responsibility,
not an instruction to duplicate its register writes in userspace. Its return or
a successful private policy ioctl does not prove wake/network acceptance.

Other private opcodes are different: 2 immediately powers down PHY, 3 reads a
register, 4/5 write registers and 6 runs substantial MAC/DMA/clock shutdown. Do
not sweep them or treat all private ioctls as harmless queries. No private ioctl
was issued on the set for this investigation. __gnu_mcount_nc causes misleading
decompiled argument/prototype recovery here; entry register/offset/branch
instructions and symbol relocations were checked instead.

## Two misleading helper names

PLAT_SYS_MCPU_MonitorTask_Reset does not issue a watchdog-disable packet. With
its mutex available it sets an internal word at 0x197d828 to 0xff for a nonzero
argument and masks it with 0xf for zero. It logs if the mutex is absent. This is
distinct from the previously verified MCU watchdog packet/tool in AGENTS.

DEV_Uart_SuspendSend (0x85f404) increments a counter and waits on a semaphore;
DEV_Uart_ResumeSend (0x85f474) increments another counter and signals it. These
helpers synchronize the vendor sender; their names do not imply an MCU suspend
packet to replay. They are also distinct from the outer DEV_Uart_ResumeClean.

## Implementation consequence and remaining work

The earlier mod_power slept inside its ordinary suspend callback, bypassed
quiescence from the service path, left host state suspended after return and
announced ON before other modules restored. Those software integration defects
are now corrected; device-specific preparation/restoration still remains open.

The host now has an internal oa_module_sleep_cycle engine: a final entry callback
after preparation, transition ledger, provider-first restoration after wake or
entry/preparation failure, separate phase results and mutation/nesting guards.
It preserves modules already suspended, handles pending shutdown cancellation
and refuses active modules without paired hooks before any preparation callback.
Nine added real-DSO/fake-entry groups pass host, ASan/UBSan and ARM/qemu; details
are in MODULE-SAFETY and raw ../analysis/sleep-cycle-2026-10-31.

The host now publishes host.sleep v1 through the existing service registry;
mod_power holds/requires it, preserving public module ABI. Its service and CLI
standby command run the notified cycle. Ordinary power hooks do not write RAM
state or emit ON. Final entry performs sync/open/write(mem)/close once; failed
or short writes do not increment the accepted-request counter. A publication
failure retains an existing peer service and releases the coordinator hold.

STANDBY is emitted before preparation; ON follows restoration of prior state.
Failed restoration emits RESUMING instead. Notifications run while the cycle
still pins lifecycle mutations; recursive standby is refused. Preflight rejection
emits no events. Manual suspend/resume run hooks only, never the RAM entry; manual
repair after a failed cycle does not yet emit a new ON notification.

Seven new integration groups in tests/power_cycle.c link actual mod_power with
wrappers for power-state open/write/close and sync, plus real dependency DSOs.
They assert all modules are suspended at entry, exercise both service/CLI and
manual hooks, preparation/open/short-write/write/restore failures, notification
order and recursion, disabled/incomplete-hook refusal, preexisting suspended
state and failed-publication/missing-coordinator cleanup. Full strict host suite,
ASan/UBSan regressions and ARM/qemu power/phase tests pass. No power-state syscall
or sync reached the host or TV. Logs: ../analysis/power-service-2026-10-31.

Modules without hooks still need explicit quiescence/ownership decisions,
especially workers and HAL owners, rather than guessed no-op hooks. Local
power.is_available reflects the enable/target guard plus coordinator availability,
not full-system hardware readiness. Config parsing is global/append-only across
host init; multi-case integration fixtures override config_get to isolate the
enable guard. They do not claim to validate parser reinitialization semantics.

Next: establish the native wake-setting source/MCU ownership without relying on
live Aurora; map device prep/restore to modules, implement remaining hooks and
complete frontend recovery-state signaling. Independent RAM/wake/
network/hardware acceptance remains a future authorized test. This research does
not authorize standby on the in-use set or full system migration.
