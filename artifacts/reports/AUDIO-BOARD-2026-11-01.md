# AD82587 board resources and real amplifier register read — 2026-11-01

Status: board resources recovered from instructions and confirmed live; a narrow
native register-read diagnostic passed on the TV. Complete native audio
initialization, UI-volume conversion, mute/routing and standby remain unfinished.
This follows [AUDIO-ROUTING](AUDIO-ROUTING-2026-11-01.md).

Follow-up [AMP-I2C](AMP-I2C-2026-11-04.md) refactors the maintained probe onto
an owned reusable transport and validates the opened fd instead of a preceding
stat. Fresh firmware write-path review and a new native read pass are recorded
there; the executable hash/size and inline implementation below are historical.
No native write/amp lifecycle acceptance is implied by either read test.

## Device resources

The verified live Aurora still has one AD82587_D instance, index 0, id 1.
DEV_AMP_GetGpioDescription (84f668) reads the instance index at object+4 and
returns 0192fa64 + (index*3 + function)*32 for function 0..2. The saved void
decompile is misleading; the ARM instructions establish this return contract.
Function 0 is reset, 1 mute, 2 the remaining power resource.

DEV_AMP_FACT_GetDefaultResource (874b10) returns a 36-byte entry at
01763ef0 + amplifier-id*36. It can patch the bus field for a PCB revision;
it is not an immutable table getter. The task copies the selected record into
0192fac4 + instance-index*36 and resolves GPIO descriptions. Live defaults and
the selected instance's copied record agree:

| word offset | observed value / use |
|---|---|
| +0 | 0 (retain as raw field) |
| +4 | 114 (72), reset logical GPIO |
| +8 | 196 (c4), mute logical GPIO |
| +12 | 3, I2C bus |
| +16 | 1, subaddress-length enum → one byte |
| +20 | 0, frequency enum → table value 100 |
| +24 | 104 (68), eight-bit slave address → seven-bit 34 |
| +28 | 10, used as post-write delay |
| +32 | 1 (retain as raw field) |

DEV_IIC_Read/Write (85358c/8537ac) establish field ordering and tables at
01358454/0135849c. Do not treat the raw resource record as the HAL's argument
structure or treat vendor address 68 as a seven-bit address.

Live copied GPIO records resolve logical reset 114 → physical **114**, and mute
196 → physical **208**. The latter uses MCU/STBC GPIO **8**, not normal
Hal_Gpio_Write. Direction enums are 2 (output); reset's configured level at +14
is 0. AMP_AD82587_Reset(1) writes that level, and Reset(0) writes its inverse 1.
Mute(1) uses its description's +10 value 1; Mute(0) uses +14 value 0. These are
recovered vendor argument/level semantics, not a general assertion about which
level means audible sound. The third resource record is all zero in this sample.

DEV_GPIO_SetLevel (8516c4) routes physical pins >=200 through
PLAT_SYS_SetStbcGpio, with MCU commands 2 for high / 3 for low. That wrapper calls
NDAL_VideoExt_STBC_SetStbcGpio. Do not initialize/call that path cold based on the
pin number alone: earlier cold STBC experiments reset the set. No GPIO or MCU
operation was executed here. AD82587_Close (874c48) is a trivial return path;
DEV_AMP_ResumeDevice (84f6c4) starts another task. Neither establishes a complete
native paired cleanup/resume contract.

## Actual I2C transfer path

Hal_Iic_Read (NDAL3 4e7bf268) invokes _i2c_RdWr_Transfer (4e7bf024). For a
one-byte subaddress and a three-byte read it builds two ARM32 i2c_msg records,
address 34, first buffer = register address, second = data, both flags READ,
then I2C_RDWR ioctl 707 on the selected /dev/i2c-N fd.

Both READ flags initially look suspicious. PreInit's **nt726xx_i2c1_xfer**
(4f4fc) resolves it: in the two-message case it consumes the first buffer as the
subaddress regardless of its flag, then uses the **second** message's flag to
choose read/write. Do not declare this a failed read from generic Linux rules
alone. This is a target-specific transport convention, not a CEC rule or generic
I2C recipe for other adapters. Kernel global/per-bus semaphores serialize hardware
transactions with stock Aurora. It returns the message count on success;
disabled-bus paths can return zero. The NDAL wrapper only rejects negative
ioctl returns, so it can falsely accept an incomplete transaction.

The device nodes are major89/minors0..5; stock Aurora fds17..22 point to them.
Bus3 is fd20. PreInit's separately named IIC_Ioctl (4bf98) is a device-tree
configuration query, and ntkdriver DDCCI_Ioctl is a different device path. Neither
is the I2C_RDWR register-read handler. Missing /sys/class/i2c-adapter entries in
this set's mounted sysfs did not establish missing bus support.

## Probe and hardware confirmation

New source: openaurora/tools/audio/amp-probe.c, make build/amp-probe or ARM
build-tv-v2/amp-probe. It only supports --describe (no I/O) and --read-volume:
one fixed read of **registers03..05**, matching AMP_AD82587_SetVolume's readback.
GetVolume (87551c) instead reads just register03; neither converts it to the UI
scale. The probe checks ARM32 structure sizes and device major89/minor3, opens and
closes its own fd, requires exactly **two completed messages**, and does no HAL
init, device scan, GPIO/reset, mute or gain write. Register selection is part of
the read transaction. Bytes are reported raw; no unverified UI-scale conversion.

Strict host and ARM builds pass -std=c11 -Wall -Wextra -Werror -pedantic.
ARM/qemu --describe passes. The tested ARM file was **19,420 bytes**, MD5
**f9b53b7de49da18d33d357b952929391**. Upload and size/hash verification occurred
in separate successful connections before execution.

Real TV result, exit0:

    completed=2 register03=1b register04=05 register05=05

This proves this transaction reached the live amplifier and returned all requested
bytes. It does not prove audible output, native bring-up, correct UI volume scale,
amp write/mute behavior or independent standby/resume.

Staging /tmp/oa-amp-20261101 and its binary were removed; no amp-probe process
remains. Boot ID ebb0aaaa-72a2-4410-9f8d-95b9249f8a90, stock PID1737, Ethernet
IP/MAC, SSH/telnet, hosts/Samba blocks and hook/keeper hashes remained at baseline.
No persistent deployment or recovery change occurred.

## Inspection mistake and cleanup

An attempted fd inspection mistakenly used cat /proc/1737/fd/* instead of
readlink and blocked on /tmp/stdin.idtv. This can consume vendor input and is not
a view-only listing. The SSH client was stopped; its surviving remote cat was
identified as PID9821 and explicitly terminated. Absence was verified again in
live-amp-cleanup. Stock UI/boot remained unchanged; no claim is made that a
blocked pipe read is inherently harmless. Future fd inspection must use readlink
or ls, never cat of the fd targets.

Evidence is under ../analysis/cec-ownership-2026-11-01: amp-board-*,
amp-iic-contract, amp-stbc-gpio, iic-hal-abi, iic-transfer, iic-preinit-*,
iic-resource-tables, live-amp-board, live-iic-*, live-amp-probe-verification,
live-amp-register-read and live-amp-cleanup. Existing source hashes apply;
no new Ghidra project was required. Next recover the UI-volume callers/table and
implement explicit amplifier ownership/lifecycle using these board resources.
