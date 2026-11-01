# PWM ABI recheck — 2026-10-31, offline

> **Status: Current PWM request/return instruction evidence. A nonzero result does not prove an ioctl or brightness change.** See [report index](README.md) and
> [current corrections](../../FINDINGS.md); old plans do not authorize TV changes.

Source: this TV's `libNDAL3.so`, MD5 `c842bad804302ec822076c3d9d3ce31d`.
Decompiled each PWM function and checked its ARM instructions, including normal
return paths. No TV access or hardware calls. Raw evidence is in
`artifacts/analysis/osd-2026-10-31/oa-pwm-current-contract.txt` and
`oa-pwm-disassembly.txt`.

The previous six-/four-word layout correction was valid, but the conclusion
that these functions return void was not. Ghidra's saved prototypes omit return
values that the instructions explicitly produce. That analysis metadata must
not override the binary.

| function | address | instruction evidence |
|---|---:|---|
| Init | `0x4e7c0c38` | supplies integer 1 on init path; 0 on already-initialised path |
| Open | `0x4e7c0d40` | reads words 0..5; common epilogue at `0x4e7c0da8` moves result r8 to r0 |
| Set_ByChannel | `0x4e7c0fc0` | reads words 0..3; common epilogue at `0x4e7c1056` moves result r6 to r0 |
| Close | `0x4e7c0f70` | no channel argument; closes shared fd; no deliberate scalar return contract |
| Terminate | `0x4e7c0cd4` | puts integer 1 in r0 at `0x4e7c0d12` before common return |

Open request: `{channel, frequency, duty, shift, mode, extra}`, 24 bytes.
Set request: `{frequency, duty, shift, mode}`, 16 bytes. Open accepts channels
0..13; Set accepts 0..12 and requires mode 0..8. Open maps modes 0..6 directly,
and maps other values to mode 2. These are distinct checks, not one common
validation rule.

A nonzero result is not reliable evidence of a panel write: both Open and Set
have a lock-result branch returning 1 without reaching their ioctl. Observed
brightness or driver state is still needed for hardware acceptance. No new
success interpretation or retry policy has been added to mod_panel.

The maintained `include/openaurora/vendor_pwm.h` now supplies one set of layouts,
size/offset assertions and scalar-return declarations. The panel uses it; the
fetch script copies it to `vendor/lib/vendor-hal.h` instead of regenerating the
obsolete five-/three-word version. Host stubs match the declarations. Existing
request policy is unchanged and return values remain explicitly ignored.

Host and ARM modules compile with `-Wall -Wextra -Werror -pedantic`; the copied
header compares byte-identical with the maintained source; shell syntax check
passes for the updated fetch script. This does not verify panel brightness,
ownership, routing or suspend/resume on the TV.
