# Reloadable amplifier profile provider — 2026-11-04

`audio_amp_profile` publishes `audio.amp_profile` v1 over the recovered
hwprofile decoder. Generic load/start/stop/unload/reload/suspend/resume and the
new read-only `amp-profile` query work through the existing CLI/Unix socket.
This advances the module integration of AUDIO-AMP-PROFILE; no hardware or TV
access occurs. Owner offline-only restriction remains in force.

The module requires `amp_profile.hwprofile`, snapshots a regular file of68
bytes..16MiB, detects short/growing reads and closes the file before publication.
FIFO/directory/special/oversized inputs reject. There is no vendor library,
GPIO, MCU, I2C, gain setter or guessed register baseline. The example uses
the extracted local profile and is not installed/autoloaded on the TV.

`copy(out,power)` returns scalar data and32 copied name bytes into caller
storage. The struct points at that caller buffer, never module/file storage.
Hold the provider through the call. Invalid/unavailable calls preserve both
outputs. Stop clears availability before removal callbacks; normal holds block
stop/unload/reload. Publication failure does not unpublish a peer provider.

Ordinary suspend retains the immutable data. Suspended replacement defers
reading/publication until resume and can retry after a failed activation.
Reload rereads file contents, not the core configuration file or Aurora's
global getter caches. Failed active replacement currently removes the provider;
explicit load/start recovers. No old-binary rollback is promised.

## Verification

Evidence: `artifacts/analysis/amp-profile-module-2026-11-04/`.

- Four real-DSO groups pass strict host, leak-enabled ASan/UBSan and fresh
  ARM/qemu: query/reload/caller-copy lifetime; held-provider and removal-call
  protection; publication collision plus absent/FIFO/directory/oversized/short/
  full-sized corrupt input cleanup and unchanged fd count; suspend/deferred
  reload/resume retry and failed replacement/recovery. Final module-0/1/2.txt
  contain the strengthened error cases; host.txt/check-1.txt are earlier runs.
- Actual local Unix IPC passes on host and ARM/qemu: query/argument validation,
  immutable file snapshot, reload, suspend, deferred replacement, resume,
  missing-file replacement failure, load/start recovery, process exit/socket
  cleanup. corpus-0/1.txt also show the saved target hwprofile passing through
  the real module, returning12Watt/27/5/5/128 (decimal). Corpus use is conditional
  in the portable test; it was present and executed in these recorded runs.
- Full strict host `make -j4 BUILD=build-amp-provider check` passes361 ok lines
  (final-2.txt). This includes the new module and Unix IPC tests. Subsequent
  stronger module fd/corrupt-file cases pass focused host/sanitizer/ARM runs.
- ARM provider393766fdeb16f6e059c81c577b7502120315b712a8e67e78b061d8d38b320621
  SHA256, only GLIBC_2.4 requirements (corpus-2.txt). This is an offline target
  build/load result, not on-TV loader/resource/hardware acceptance.

Reproduce host with `make -C openaurora BUILD=build-amp-provider
check-amp-profile-module check-amp-profile-ipc`. ARM uses the maintained compiler,
TARGET=tv BUILD=build-tv-v3/amp-provider; build the core, provider, ticker and
module test, run the module test under qemu-arm-static -L <sysroot>, then
`node openaurora/tests/amp_profile_ipc.cjs openaurora/build-tv-v3/amp-provider
<sysroot>`. No child loader namespace or device emulation is needed here.

Firmware arithmetic/input decoding evidence remains AUDIO-AMP-NATIVE and
AUDIO-AMP-PROFILE. Provider lifecycle is project policy, not a claim that
Aurora implements this service ABI. Still open: audio consumer integration,
retained baseline ownership, coordinated worker/backend and GPIO/MCU/I2C
lifetime, native initialization/standby/resume and audible acceptance. The
broad audio/amplifier lifecycle TODO remains unchecked.
