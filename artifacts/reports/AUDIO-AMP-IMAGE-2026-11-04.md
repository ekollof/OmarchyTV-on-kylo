# AD82587 profile image and lifecycle contract — 2026-11-04

Follow-up [AUDIO-AMP-NATIVE](AUDIO-AMP-NATIVE-2026-11-04.md) closes the native
instruction comparison limit for the register image:6000 cases plus38 write
failures match the complete shipped Init body with controlled hardware callees
and absent GPIO descriptions. The static-only scope below describes the earlier
milestone; actual hardware ownership/bring-up remains unimplemented/unaccepted.

Recovered the selected AD82587_D's init/open/close/mute/device wrappers from
Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`, with ARM instructions. Added
`common/audio_amp.c` to convert explicit profile values into the38-byte register
image. This is a pure data prerequisite, not native amplifier ownership/init.
No TV access, GPIO, MCU, I2C, audio write, reset or deployment occurred.

## Register image

AMP_AD82587_Init@874ef0 mutates the retained image at17640e8 before writing
registers0..25 hex in order. The helper takes the current image plus explicit
profile inputs. No firmware baseline bytes are embedded or redistributed.
Initial state must come from the local firmware; subsequent calls must retain
the previous image, including after a failed hardware write. In particular,
the OR of state-control bits is not a fresh-default assignment on resume.

| Registers (hex) | Recovered conversion |
|---|---|
| 03/04/05/17 | low byte of AmpPrescale/Ch1Volume/Ch2Volume/VolumeFineTune |
| 07 | retained value OR30, then replace with SpeakerStateControl4 low byte unless exactly-1 |
| 08 | DRCLimiterAttackRate low byte; exactly-1 selects6a |
| 13..15 | AMPPowerClip low24 bits, most-significant byte first; exact power names6Watt or8Watt_12V select080000 |
| 20/21 | 00/10 |
| 10..12 | unsigned SpeakerDRC below500 selects041eb0; otherwise low24 bits, most-significant byte first |
| 22..24 | same DRC triplet; only when unsigned SpeakerDRC>=500 does DRCReleaseThreshold override it (unless exactly-1) |

All other bytes remain the caller-supplied current image. The helper supports
in-place updates and rejects invalid pointers atomically. These values are not
UI volume, decoder gain, speaker DSP curve entries or GPIO levels.

## Lifecycle corrections needed before native integration

- Init sets mute-resource direction, calls Mute(0), waits5ms, drives reset's
  configured level for20ms and inverse for10ms, then performs Reset(1),10ms,
  Reset(0), Mute(0),1000ms. Those waits are milliseconds: TD_OS_TaskDelay@91bee4
  calls usleep(argument*1000), except the special-1 sleep path.
- It patches the image and writes38 single-byte registers. First false
  _DEV_AMP_WriteIIC result stops the loop, waits10ms and calls Mute(1), then
  returns0. Success also runs the profile-gated subwoofer helpers, waits10ms,
  calls Mute(1) and returns1. A subwoofer function's AD82586C name does not
  change the selected main amplifier. Lower transfer completion and actual
  effects still need explicit validation; ignored GPIO returns are not success.
- Open@875598 and Close@874c48 return object!=NULL and perform no hardware
  acquisition/release. The saved void prototypes hide that return. They are
  insufficient as paired native ownership hooks.
- DEV_AMP_Init@84f4ac requests HAL_AUDIO_SET_IISMCLK (opcode3a, first word100
  hex) and starts task84ebf1. Its bool result means task creation succeeded.
  DEV_AMP_ResumeDevice@84f6c4 sets byte192fa55 and starts task84e1bd; the
  failure branch does not clear that byte. The task calls the descriptor's
  init callback, ignores its result, handles headphone/profile mute selection
  and clears the byte. Thus wrapper success/flag clear is not amp acceptance.
- DEV_AMP_Term@84f55c optionally calls descriptor+54 and returns1; it does
  not itself join tasks or free the descriptor. PowerDown@84f904 calls+68
  when present. The AD82587 constructor starts with calloc and does not install
  either slot. Do not invent a real shutdown from these wrapper names or
  assume no later callback patching without tracing that separately.

## Tests, evidence and limits

Evidence directory: `artifacts/analysis/audio-lifecycle-2026-11-04/`.
amplifier-methods.txt and tasks-term.txt contain read-only Ghidra decompilation
and ARM instructions; device-lifetime.txt includes references. Some requested
symbols/task entries are not defined functions in that saved Ghidra project,
so those batches stop at their explicit errors. This is metadata absence,
not absent firmware code. resume-task-arm.txt and init-task-arm.txt recover
those known entry ranges from the maintained objdump cache; misleading nearby
symbol labels do not name the tasks. Existing AUDIO-BOARD and the earlier
ad82587-contract.txt supply GPIO/resource and constructor evidence.

`make check-audio-amp` tests byte/24-bit narrowing, case-sensitive power
exceptions, unsigned499/500/ffffffff DRC boundaries, signed override sentinels,
unchanged bytes, guards, in-place/repeated-init state and atomic rejection.
Strict host, leak-enabled ASan/UBSan and fresh ARM/qemu pass (test-0/1/2.txt).
ARM unit SHA256:
`f00d039a2d3ff482993efae207910c3a40c2039fbc9645afd16f3e6aa070dceb`.
Full host `make -j4 BUILD=build-audio-amp check` passes (349 ok lines).

The implementation was compared to the recovered instructions; the test
vectors do not execute the vendor Init body. A native instruction differential,
real profile acquisition/baseline ownership, owning worker and GPIO/MCU/I2C
backends, init failure recovery, standby/resume and audible acceptance remain
open. No broad audio/amp lifecycle TODO is closed by this helper.
