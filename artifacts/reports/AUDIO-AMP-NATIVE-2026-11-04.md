# AD82587 register-image comparison against native Init — 2026-11-04

The new `tests/audio_amp_native.h`, included in the maintained ARM firmware
harness, executes the complete shipped AMP_AD82587_Init body and compares its
retained38-byte image and write bytes against `oa_ad82587_configure`.
**6,000 profile/image cases and all38 write-failure positions pass.** The42
preceding groups pass as well (43 total). No TV access or hardware operation.

## Actual code versus controlled callees

The fixture copies598 bytes at874ef0 from the local verified Aurora ELF,
CRC32 `1fba7b2d`. The instruction body is unchanged. It maps the referenced
guard word, writable image and GPIO-pointer storage at their recovered
addresses. The image starts with one of four inputs: the local shipped image
(CRC32 `b942ff40`), zero, ff, or an ordinal byte pattern. No vendor bytes are
embedded in source. Needed literal/string pages are loaded from the ELF and
CRC checked. Mappings and stub writes use the maintained extent checks.

Profile getters return explicit test values; the five DRC-key calls verify
actual firmware string identities/order. GPIO descriptions return NULL,
reset/mute/delay/board/subwoofer callees are controlled callbacks. No vendor
library is linked or initialized. The native register-write call must have the
expected object, ordinal register, length1 and exact data pointer. That fourth
argument is preserved by a16-byte trampoline. strcmp's even PLT slot uses an
ARM trampoline, not Thumb. The stack guard remains checked against a mapped
fixture word. Present-GPIO branches are not exercised.

The image matrix covers four power names (including case mismatch), four
baselines, unsigned DRC0/499/500/123456/ffffffff, release/state overrides
-1/-2/0/123456/INT_MIN and limiter -1/-2/123, with varying byte-source fields.
All38 bytes, the38 emitted bytes, neighboring guards, getter count, successful
return and subwoofer-callback count are checked. Values are not reduced to
nonzero success or one selected register.

Each write-failure position then verifies false return, exact emitted prefix,
no subwoofer calls and the fully patched retained image. Consecutive cases
reuse that image with alternating state override/-1. This proves against the
native body that image state is updated before the write loop and survives
failure. It does not establish hardware rollback semantics or pin behavior.

## Reproduction and evidence

```
make -C openaurora TARGET=tv BUILD=build-tv-v3/audio-amp-native \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/audio-amp-native/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs \
  openaurora/build-tv-v3/audio-amp-native/tests/tab_policy_firmware
```

Strict C11/Wall/Wextra/Werror/pedantic build. Evidence:
`artifacts/analysis/audio-amp-native-2026-11-04/build.txt` and `reference.txt`.
The log identifies the binary by SHA256; it contains all43 group completions.
The verifier now requires the new group and defaults to this isolated build,
so an older42-group executable cannot silently omit the comparison.

This supersedes AUDIO-AMP-IMAGE's static-only/native-differential limit for
the register-image calculation. No production helper change was necessary.
Actual profile lookup, original retained-image ownership, present-GPIO/reset/
MCU/I2C behavior, worker lifetime, startup/standby/resume and audible acceptance
remain open. The full amplifier/audio TODO stays unchecked. Offline-only owner
restriction remains in force; this comparison does not authorize device tests.
