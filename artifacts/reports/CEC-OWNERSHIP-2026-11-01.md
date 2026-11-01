# Native CEC ownership and raw-device backend — 2026-11-01

Status: firmware registration/lifetime investigation plus a tested **blocking
raw-device backend**. It is not yet a running CEC module or complete authority/
standby policy. No TV query, device access, deployment or recovery change occurred.
This follows [CEC-TRANSPORT-CONTRACT](CEC-TRANSPORT-CONTRACT-2026-10-31.md).
Subsequent authorized TV checks and specification regressions are recorded in
[CEC-LIVE](CEC-LIVE-2026-11-01.md). The no-TV statements here describe this
earlier offline stage; the follow-up resolves node registration and shared-owner
queries without claiming independent raw initialization or complete authority.

## Registration and ownership evidence

PreInit.ko from this target's root filesystem has MD5
1e50bee8b7888315c7a070f8e69947f6. New analysed project:
artifacts/workbench/ghidra_preinit, project PreInit / program PreInit.ko.
The Aurora/NDAL/kernel identities remain those in CEC-TRANSPORT-CONTRACT.
Instructions and source-specific offsets are in ../analysis/cec-ownership-2026-11-01.

The vu_* symbols imported by ntkdriver_nondovi are **PreInit's kernel device
emulation layer**, not libc open/ioctl/close. In this import:

- vu_open (0x4b1b8) resolves emulated devices, builds fake file/inode objects and
  calls the device's open operation. vu_close (0x4ad94) invokes release and clears
  its emulated handle; it does not repair the CEC caller's stored global fd.
- vu_ioctl (0x4afb0) extracts its vararg, resolves the emulated device, calls its
  ioctl operation and normalizes nonzero failure to -1. It preserves a null
  argument for the size-bearing CEC close request. The earlier close/reopen
  hazard therefore survives this wrapper; it was not a libc-fd inference.
- vk_alloc_chrdev_region / vk_register_chrdev_region call the real Linux
  registration functions as well as maintaining the emulation map.
- vk_cdev_init (0x4aa40) calls cdev_init with the supplied operations, and
  vk_cdev_add (0x4aa7c) calls cdev_add before adding emulation metadata.

The CEC .rel.rodata table at 0x7164 has module owner and direct function entries:
ioctl +0x24 → KER_CEC_IOCtl, open +0x30 → KER_CEC_Open, release +0x38 →
KER_CEC_Release. The same exclusive semaphore applies to a native file open.
Thus a native raw-device candidate is supported by registration evidence;
node existence, actual user-pointer I/O and independent hardware operation have
not been checked on the TV. The handler directly dereferences pointer-bearing
arguments; use the recovered target ABI, not a host-size/Linux CEC structure.

## Implemented backend and failure semantics

`openaurora/common/cec_device.c` / public cec_device.h implement explicit
open/initialize, send, receive and close. Nothing opens automatically. Real
syscalls are selected only on ARM32; other host architectures need injected I/O.
The caller supplies the device path and must already own authority/wake policy.

Initialization sends the recovered **28-byte** raw open block:
{300, 3, 0xedd5c5ad, 0x8b64573f, 0x8b642a12, 0, 0}. These are the built-in
DRV_CEC_Open parameters (timeout, controller setup, logical address 0, hardware
id 0), not the different 36-byte VPL input. Descriptor 0 is valid. The backend
sets O_CLOEXEC and owns its real fd rather than using the broken VPL sentinel.

Transmit marshals a complete 1..16-byte TV frame, including header-only polling,
then calls raw ioctl 0xc0104304. Foreign initiators reject before I/O. Receive
uses 0x80104303 with 16 backing bytes; empty/error output is unchanged. It does
not use the VPL opcode filter, so the spec-valid broadcast System Audio Mode
frame can reach higher-level policy. Framing acceptance is not opcode conformance.

Close passes a **pointer to hardware-id byte 0**, not a null argument, to
0x80014302. If that ioctl fails, fd/state stay owned for explicit retry. An owner
must not discard this object or unload while that cleanup remains pending.
After successful hardware close, final OS close consumes the descriptor even if
it reports an error; the integer is never blindly retried after possible reuse.
Failed initialization closes the acquired fd without shutting down a context
that failed to initialize; primary errno survives cleanup errors.

No userspace retry replays an interrupted transmit. The kernel already retries
and waits for hardware completion/permission. All operations belong outside
the host dispatch loop. Workers/authority leases and failed-close retention
across module stop/unload are still to be integrated; the current void stop hook
cannot be treated as proof that physical cleanup succeeded.

Three injected-I/O regression groups pass strict host C11 -Wall -Wextra -Werror
-pedantic, ASan/UBSan and ARM/qemu. They cover fd 0/reopen, exact request blocks,
frame/poll transmission, failures/unwind/retained ownership, raw broadcast receive,
empty/error atomicity and no replay after EINTR. Full host suite passes. These
fixtures prove code behavior, not native node access, electrical ACK, physical
reopen, cancellation latency or audible output. Production core/modules unchanged.

## Authority, audio and remaining work

VPL_CEC_Open checks bit 1 in the mirror at physical 0xfc040200 and requests an
STBC 0xf011 write preserving the low byte with bit 1 set. Raw open does not
reproduce that authority step. DRV_STBC_SwitchCEC separately sends IPC opcode 9a;
it must not be substituted for the mirror/register operation without caller
analysis. No guessed register write or MCU packet was executed or added here.

Additional audio evidence: DEV_AMP_Init (0x84f4ac) sets I2S MCLK and creates a
device task; it is not complete amplifier bring-up. PLAT_AOUT_Init (0x88744c)
initializes three outputs and nine 0x3c-byte speaker slots. SpeakerSet
(0x8886f0) emits routing controls for particular speaker types; success alone
does not prove an output was configured. Current output-id/amp routing remains open.

The pcbconfig inventory includes FIPC_get_audio_type, but its CLI is not a
view-only getter: main can create tag files, invoke reset handling and other
actions. Its FIPC helper uses a six-byte BA/subcommand-1 exchange; board-type
mapping and PcbConfig_get_audio_type's complete path remain unverified. Do not
run pcbconfig -a as a harmless query. No live query was needed for this work.

Next integrate stable authority/device ownership and owned blocking-I/O workers
with module reload/error cleanup, then CEC policy/System Audio/ARC and local-vs-
remote volume state. Recover MCU standby handoff and board amplifier routing
before physical takeover/wake/audio acceptance.
