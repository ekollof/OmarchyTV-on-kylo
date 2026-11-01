# CEC transport, queues and reload constraints — 2026-10-31

Status: current offline instructions and tested packet marshalling. No TV
query, device open/ioctl, bus transmission, audio change or deployment occurred.
No working native CEC transport/module or wire conformance is claimed yet.
Follow-up: [CEC-OWNERSHIP](CEC-OWNERSHIP-2026-11-01.md) adds a tested blocking
raw-device backend and resolves PreInit/real registration. Authority/worker/module
and hardware integration remain open; use that report for current next steps.
This follows [CEC-AUDIO-CONTRACT](CEC-AUDIO-CONTRACT-2026-10-31.md).

## Authoritative sources

Aurora MD5 64c252340d3af6c5d66072611b7bad7a;
libNDAL_EX e6a465861d1869b8b138b3f299c89f87;
ntkdriver_nondovi.ko fded7005651e37b8a1b5e8e89f73f4ff.
Files and analysed projects persist under artifacts/workbench. Raw instruction
logs: ../analysis/cec-transport-2026-10-31. Kernel addresses below are Ghidra
program addresses; ELF .text offsets differ by 0x10228 for this import. Other
sections have different mappings; use their own section/symbol file offsets.

## Message transport is not an MCU UART protocol

The normal path is:

```text
Aurora PLAT_CEC_* → libNDAL_EX private V4L2 controls (class 0x0f1a0000)
 → EX_PLAT_CEC_ext_ctrls → VPL_CEC_* → DRV_CEC_*
 → kernel-internal ntkcec character-device operations → hardware CEC controller
```

MCU CEC-enable/wake/authority policy remains relevant, but it does not imply that
normal frames use UART. No Linux /dev/cec UAPI is assumed. CEC transport must not
read ttyS2 alongside stock Aurora; that would consume remote input.

NDAL Init uses id 0x0f1a0f01, status byte plus 36-byte configuration, SET_EXT_CTRLS
0xc0185648. Non-null configuration invokes VPL_CEC_Open; null invokes Close.
EX_PLAT ignores the supplied configuration contents for Open. VPL_CEC_Open
(0x616ce4) sets CEC authority toward ARM through STBC register 0xf011 bit 1 and
constructs its own defaults/callback. It then enters DRV_CEC_Open (0xdb0f8),
which tries /dev/nvt/ntkcec, /dev/ntkcec, then creates /tmp/ntkcec if necessary.
KER_CEC_Open (0xd916c) uses an exclusive semaphore. These opens are hardware
ownership operations, not read-only diagnostics.

Aurora's monitor at 0x88ba30 checks NDAL queue count every 200 ms and invokes its
registered callback when nonzero. It is missing as a Ghidra function; exact Thumb
objdump instructions are retained instead of guessing a decompile. The ICE
handler and CECInterface add queues, policy and a separate send thread; their
callbacks must not be copied as raw pointers into unloadable modules.

## Distinct packet layouts and status results

Both structures are **16 bytes on ARM32**, with pointer at +4 and hardware id at
+12, but the intervening bytes have different meanings:

| offset | ntkcec raw packet | VPL/NDAL Process_Command packet |
|---|---|---|
| 0 | completion/receive status | receive status |
| 1 | reserved | input validation-bypass / output header-only flag |
| 4 | pointer to entire frame | pointer to operands |
| 8 | whole frame length, 1..16 | operand count, 0..14 |
| 9 | reserved | initiator |
| 10 | reserved | destination |
| 11 | reserved | opcode |
| 12 | hardware id, only 0 supported | hardware id, only 0 supported |

KER_CEC_IOCtl (0xd94e0) handles raw open 0x401c4301 (28 bytes), receive
0x80104303 and transmit 0xc0104304 (16-byte pointer-bearing packets). Raw receive
returns immediately with length zero and a null buffer pointer when empty.
It clamps a queued whole-frame length to 16. It is a consuming operation.
Transmit waits for hardware completion, can sleep while acquiring permission
and retries. It cannot run synchronously in the host's dispatch loop.

NDAL Process_Command (0x4db59970) uses id 0x0f1a0f04 and three controls: result
byte, 16-byte VPL packet, mode word (0 send / 1 receive). On receive EX_PLAT
(0x6188fc) copies **16 operand-buffer bytes**, irrespective of the reported
operand count, including failure paths. A 14-byte operand allocation is unsafe.
The new helpers always require at least 16 backing bytes for either packet type.

`ndcommon_v4l2_ioctl` (0x4db66c70) returns whether v4l2_ioctl was not -1.
NDAL Process_Command returns that boolean without combining the separate driver
result byte. Init/polling do combine the two. Thus Process_Command success alone
does not establish transmit success or receive acceptance. Our VPL decoder
requires both results and never changes output after a rejection.

Queue count id 0x0f1a0f08 uses GET_EXT_CTRLS 0xc0185647. Kernel
_cmd_received_notification increments a global count; VPL receive decrements it
**before** attempting a dequeue. Never request VPL receive with count zero or
race another reader. The observed decrement/callback paths are not a synchronized
queue abstraction; stale/racing counts and restart reconciliation remain work.

## Driver filter is not specification validation

DRV_CEC_Get_Validated_CEC_Command (0xdc1c8) uses gstCEC_CMD_Talbe. Flags are
0x80 reject, 0x10 directed, 0x20 broadcast, 0x40 fixed operand count in low nibble.
The 256-byte table was extracted using .rodata file offset 0x6823b0 plus symbol
offset 0x743c. Examples:

- Give OSD Name / Set OSD Name (46/47) have flag 0x20, incorrectly broadcast-only.
- Set System Audio Mode (72) has 0x51, rejecting the specification's allowed
  broadcast form. ARC C0..C5 have 0x50 (direct, zero operands).

However these bytes alone do **not** prove the stock TV drops those valid frames:
EX_PLAT fills the packet before reporting the filter result, NDAL loses the result
byte, and _ice_cec_Handler (0x8067bc) logs a failed result but still processes a
non-poll packet. The higher policy validator has separate addressing rules.
This is a concrete reason the old per-layer conformance conclusions were too strong.

A strict VPL-result decoder preserves that layer's rejection; it is not an
end-to-end spec-compliant receive backend. Do not implement native naming/System
Audio by blindly accepting that filter. A raw transport plus our applicable
specification validation needs explicit ownership/authority recovery first.
Setting byte 1 as a blanket bypass is not an established solution: the same byte
also marks header-only polling on output. No on-TV experiment is authorized here.

## Close/open is not a reload recipe

DRV_CEC_Close.part.3 (0xdafd8) stops its kernel thread, calls close ioctl
0x80014302 with a null argument, closes its internal fd, then stores **0** at
gFileDesc (0x8ff478). KER_CEC_IOCtl rejects a null close argument before disabling
hardware. KER_CEC_Release only releases the exclusive semaphore, not hardware.
DRV_CEC_Open's early test treats any fd other than **-1** as already open and
returns success. The initial .data value is -1, verified at ELF offset 0x8ef2a8.
After this close path, reopen therefore returns without opening a new fd. These
are instruction-level findings, not a reproduced on-TV restart test.

Do not patch signed firmware or driver globals to repair this. A reloadable
design must explicitly choose a stable owner for initialized hardware or recover
a complete independently owned raw-device lifecycle. Simply calling NDAL Init
on start and Init(NULL) on stop would give a false success after reload.

Kernel suspend (0xda1c0) disables/frees the IRQ; resume (0xda044) reinstalls it,
resets RX indices, restores default hardware parameters and clears the logical
address to 0. It does not supply complete MCU wake/authority policy. Userspace
reopen, kernel PM restore and callback/queue restoration are distinct operations.

## Implemented and next steps

vendor_cec.h records both packet layouts with ARM size/offset assertions.
common/cec_packet.c supplies bounded marshalling, distinguishes polls from
opcode-zero frames, checks hardware id/pointer/length/status and requires 16-byte
storage. No borrowed pointers are serialized. Four regression groups pass on
strict host, ASan/UBSan and ARM/qemu; full host suite passes. Host fake layouts
use native pointer widths; ARM builds verify the actual 32-bit layout.

Next recover the complete raw-device initialization/authority and lifetime
contract, including fops/user-pointer handling and semaphore/error unwind.
Assign a stable ownership boundary before transport/module implementation;
workers must isolate blocking transmit and join/cancel safely during reload.
Then implement System Audio/ARC policy and remote-volume state separately from
local audio control. Independent hardware/wake/reload/wire acceptance remains open.
