# Direct speaker gain and the readback trap — 2026-11-01

Status: firmware contract recovered; lower encoder/session dispatch implemented
and tested with injected I/O. No real gain/table/mute/amp/reset/routing write.
The broker, audio.mixer service and CLI remain query-only. UI volume integration
and audible/native ownership acceptance remain open.

## Firmware authority

Read-only/no-analysis Ghidra extracts use target NAND ntkndpalsa.ko
(MD5 8758a13d12799217ce702849df7d54da) and ntkdriver_nondovi.ko
(fded7005651e37b8a1b5e8e89f73f4ff). Addresses below are Ghidra project
addresses, not runtime kernel addresses. Instrumentation distorts some
decompiled arguments; ARM register flow is authoritative. All raw logs are in
../analysis/cec-ownership-2026-11-01/.

The ALSA table returned by tdp_shell_load_pcmd (33194, table5fac8) registers
SetOPUGain: record5fbb8 references name50da0 and handler31e0c. See
speaker-setter-command-table/records/names.txt and speaker-setter-registration.txt.
This establishes the spelling rather than inferring it from a function name.

_cmd_set_opu_gain_p (31e0c) bounds each channel operand to0..0x8000 before
dispatch to Mod_Cell_SetOPUGain(dev,left,right). The maintained API accepts
known devices0..5. Composite selectors>=0x100 reach a separate capture path
with additional table indexing; they are unsupported. See
speaker-setter-alsa-handlers.txt; the basic handler was already present in
audio-gain-kernel.txt.

Mod_Cell_SetOPUGain (1d22a0) receives r0=device, r1=left, r2=right. Instructions
1d238c/1d2390 store channels at cell+34/+38 before subsequent state/mute/mixer
processing. Mod_Cell_GetOPUGain (1cc970) reads those caches at1cca58/1cca60.
See speaker-setter-driver-path.txt and speaker-setter-readback.txt. The operand
bound is not a physical loudness calibration.

## UI volume and rollback limits

UI volume uses ALSA **speaker out gain**, HAL item42(hex), through
Mod_Cell_SetVol (1cd9b0). It selects/clamps the AQ table and stores the UI
level; _mod_cell_setVol_p (1ca2e4) applies lookup, balance and ratio processing
with additional source/state overrides. Direct raw gain bypasses that processing.
See speaker-setter-driver.txt, speaker-setter-driver-path.txt and
AUDIO-VOLUME/AUDIO-GAIN-TRANSPORT. Neither raw SetOPUGain nor decoder opcode33
substitutes for UI speaker volume.

Earlier audio-netlink-query-contract.txt establishes that _cmd_get_opu_gain_p
(31644) converts each cached channel value **exactly100** to0x8000 in its reply.
Connecting this to the direct setter/cache proves raw100 and raw32768 are
indistinguishable through GetOPUGain. **This query cannot supply an exact
rollback snapshot for a raw write.** Do not attempt write/restore experiments
based on it. Our codec preserves wire values and invents no inverse mapping.

As AUDIO-NETLINK establishes, full send means dispatch only: no command-success,
DSP or acoustic ACK exists. ALSA's separate getter also reports requested state,
not audible output.

## Implementation and verification

common/audio_netlink.c adds oa_audio_netlink_set_opu_gain: exact8208-byte frame,
SetOPUGain prefix immediately followed by14 little-endian parameter words, zero
tail, device0..5 and channels0..0x8000. Rejected input leaves output unchanged.
common/audio_client.c adds owned-session dispatch with no wait, fabricated ACK
or EINTR replay. Fork children cannot use the parent's PID binding. No async
owner request, broker opcode, module service or CLI setter was added.

Three wire and six session groups cover golden bytes, unaligned fields,
canaries, operand/capacity rejection, errors/short sends, no replay and inherited
session rejection. Full host make check passes with C11 -Wall -Wextra -Werror
-pedantic. Focused ASan/UBSan includes async owner/broker/mixer regressions.
ARM/qemu executes the updated units. Logs: speaker-setter-full-host.txt,
speaker-setter-sanitizer.txt, speaker-setter-arm-build.txt,
speaker-setter-arm-qemu.txt and speaker-setter-tv-tests-build.txt.

Actual TV execution passed all nine groups **using fake I/O callbacks only**;
no native netlink request or device access occurred. Upload, separate size/hash
verification, execution and cleanup are recorded in
live-speaker-wire-before/verify/runtime/after.txt. Exact ARM units are retained
in speaker-wire-tested-tv.tar:

| Test | Size | MD5 |
|---|---:|---|
| audio_netlink | 37668 | a90d0a2c4635af1210a9ecb1467d9bf2 |
| audio_client | 67756 | bcd8ad1eff1f78aaa0dbb97543e7dc2c |

/tmp/oa-speaker-wire-20261101 was removed. Boot
71f7eb63-727f-4b9a-a9c7-2b04dc3f9197, stock Aurora1785, installed
core/hook/hosts identities, listeners and privacy protections stayed unchanged.
No helper remained running. The production16-module ABI3 bundle was not rebuilt
with this unused lower primitive; stage.md5 retains the MIXER-MODULE identities.
Only ARM unit binaries were rebuilt under build-tv-v3/tests. No persistent
deployment or recovery artifact changed.

## Next integration gate

Implement the firmware-understood UI ALSA control path, preserving AQ selection,
balance, ratio, source/profile handling and ownership. The read-only mixer probe
documents stock ALSA_CONFIG_PATH=/vendor/etc/alsa.conf. Broker table IPC must
copy data into its own address space and retain it through dispatch, never pass
a foreign process pointer. Separate dispatch, requested state and audible
acceptance. Amp/routing/native lifecycle and coordinated standby remain in TODO.
