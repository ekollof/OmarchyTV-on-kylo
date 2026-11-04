# Reset GPIO query and unsafe HAL getter boundary — 2026-11-04

Fresh decompilation/ARM inspection preceded `common/amp_gpio_query.c` and
amp-gpio-probe. A native GET-only GPIO114 query passed on TV. No pin claim,
configuration, level write, export, HAL/STBC initialization or framebuffer
operation occurred. Mutating GPIO/MCU control and ownership remain open.

## Firmware findings

Evidence: `artifacts/analysis/amp-gpio-2026-11-04/`. Source MD5s: NDAL3
`c842bad804302ec822076c3d9d3ce31d`, PreInit
`1e50bee8b7888315c7a070f8e69947f6`, Aurora
`64c252340d3af6c5d66072611b7bad7a`. Existing Ghidra projects were read-only/
noanalysis; instructions, not saved prototypes, establish arguments/effects.

- hal-gpio.txt: Hal_Gpio_GetDirection4e7be990 issues670a before670f. Kernel
  GPIO_Ioctl3be18 case0a marks the global pin context opened and records state.
  This HAL wrapper is not a view-only diagnostic.
- Ordinary-pin Hal_Gpio_Read4e7be4cc (including114) writes the pin number to
  /sys/class/gpio/export before reading value. It is not a pure getter. The
  separate200..204 HDMI bulk branch must not be generalized to other pins.
- Hal_Gpio_Config4e7be880 maps DEV direction0/1 to kernel payload1/0, with
  670a then670e. Hal_Gpio_Write4e7be754 uses670a,670e(direction0),670c(level).
  Native writes must account for that claim/direction sequence and authority.
- kernel-gpio.txt case0d copies8 bytes in/out and calls ntgpio_GetLevel3903c
  (kernel-pin.txt). It checks the pin is already open/in GPIO mode, then reads
  MMIO under its semaphore: input register or output latch according to cached
  direction. It changes only the byte at payload+4.
- Case0f checks the same existing-context requirements and returns the cached
  direction byte at+4, copying8 bytes back. It does not claim/change the pin.
  Kernel0=output/1=input differs from DEV0=input/1=output and the profile enum.
  Direction is not an independent direction-register/physical-voltage read.
- kernel-registration.txt: KER_GPIO_DrvInit registers ntgpio char132:0 and
  installs g_fops at652b8. Its complete120-byte symbol (kernel-fops.txt) has
  only owner/GPIO_Ioctl nonzero; open/release callbacks are zero. The final8
  dumped bytes are adjacent data, not part of that table. Ordinary fd open/
  close therefore does not invoke global GPIO initialization/termination.
- Ioctls6701/6702 invoke global ntgpio open/close, unlike fd open/close. They
  are not used here. Missing pin context never triggers670a/export fallback.
- aurora-gpio.txt and ndal-stbc-get/set.txt record the >=200 STBC/V4L2 boundary.
  MCU mute208 is outside the query API. No STBC call, including its getter,
  was run; the existing cold-reset warning/unresolved native authority apply.

## Implementation and verification

`oa_amp_reset_query` borrows a caller-owned fd and sends only670d then670f,
fixed pin114, explicit8-byte payload. Native ARM32 mode validates char132:0;
other architectures require an injected callback. It publishes output only
after both requests validate pin/boolean/status. Errors return negative errno,
without replay or hidden setters. Two GETs are not an atomic snapshot,
exclusive lease, complete amp state or rollback image.

The CLI --read-reset opens O_RDONLY|O_CLOEXEC and closes its own fd; --describe
does no I/O. No setter, arbitrary pin, export, HAL/MCU init or global close.
The future blocking owner must serialize and retain the fd through return.

Two groups pass strict host, leak-enabled ASan/UBSan and fresh ARM/qemu: four
level/direction combinations, exact pin/opcodes/payload, nonzero adjacent reply
bytes, invalid inputs, either-query EINTR, malformed pin/boolean/status and
atomic error output. The first fixture failed because loop counters remained2
before error tests (fixture-before.txt); resetting that fixture state resolved it before the passing
runs. No TV fault inferred. Full strict host:392 ok, exit0. Kernel comparison
is static instruction inspection; injected tests do not execute kernel bodies.

## Native result and limits

Probe21,812 bytes, SHA256
`19517981e612e04eef2c0fc2502cc7963ed7eab739ac1f3e9ac40405769f669d`,
GLIBC_2.4 only. Separate staging size/hash verification preceded execution:

    GPIO114 level=1 cached-direction=0 (output)

Staging /tmp/oa-amp-gpio-20261104 was removed; /proc/*/exe found no helper.
Boot ID71f7eb63-727f-4b9a-a9c7-2b04dc3f9197, vendor PIDs1967/1785 and no-reset
protection were unchanged/present. Evidence: tv-before/upload/verified/query/
after.txt. Every connection used tools/tvssh.sh. No fb1, audio setting,
reset/mute/level write, global GPIO init/close or persistent deployment.
This cached-direction/output-latch observation is not physical voltage,
audio acceptance or an exact rollback snapshot.

Next: coherent pin/device/MCU authority and paired lifetime before mutators,
worker/module integration, native reset/mute/write effects, standby/resume and
audible acceptance. Do not fix a missing-context error by claiming pins under
the stock owner. No broad amplifier TODO is closed.
