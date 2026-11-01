# Reloadable audio.curve provider — 2026-11-01

Status: production data module implemented, six real-DSO lifecycle groups pass
on host, sanitizer, ARM/qemu and the TV. No audio hardware is initialized or
changed. Native gain/table application, profile decoding/default fallback,
routing/amp lifetime and audible acceptance remain unfinished.

The firmware decoder and all101 live curve comparisons are in AUDIO-VOLUME.
New modules/audio_curve/audio_curve.c uses that same common decoder and publishes
**audio.curve v1** through services.h. Callers hold it with lookup/release. The
interface copies the whole table into caller-owned data or returns one clamped
entry; it never hands out pointers to module-owned table storage.

Configuration is explicit:

    audio_curve.aq = /path/to/AQ.bin
    audio_curve.profile = 0

Selection1 is aggressive; every other integer selects normal, matching firmware.
Missing/noninteger/overflowing selection rejects. This is an explicit selection,
not automatic hardware-profile detection. The AQ file must be a readable regular
file of exactly1668 bytes. Nonblocking open plus fstat rejects FIFOs/devices/
directories before reading; reads are bounded and decoded before publication.
No guessed curve or automatic fallback is published on failure.

Start reads a fresh file snapshot; published data stays immutable until stop.
Stop removes only this module's publication. A collision preserves the peer.
Normal stop/unload/reload is refused while a consumer holds the service; consumers
must release it, then reacquire the replacement interface. Caller copies remain
valid across replacement. --force retains the host's existing unsafe-consumer
semantics; it is not a way to preserve a cached interface pointer.

Paired suspend/resume retains the immutable snapshot without device work. Reload
while suspended defers first start/file access/publication until resume; failed
activation remains retryable. Reload rereads the AQ file using current host
configuration values. The host does **not** reread its configuration file on
module reload and has no old-binary rollback: a rejected replacement can leave
the module absent. These host limitations were not changed or concealed.

From openaurora/, the checked-in examples/audio-curve.conf demonstrates a local
data-only process (normal selection explicitly chosen):

    make
    build/openaurora --modules build/modules --config examples/audio-curve.conf --sock /tmp/oa-curve.sock

The existing client, in another terminal:

    python3 tests/client.py /tmp/oa-curve.sock 'reload audio_curve'

There is no automatic audio_curve startup in the TV boot configuration. This
module does not yet drive mod_audio or call HAL. File/profile/default policy and
gain/routing/lifecycle integration remain next work, not implied acceptance.

## Validation and build identities

tests/audio_curve_module.c and make check-audio-curve-module exercise:

- Actual DSO/CLI start/reload, changed AQ contents and immutable caller copies.
- Registry consumer holds blocking stop/unload/reload until release.
- Publication collision preserving the peer and retrying the same instance.
- Missing/malformed profile, missing/short/long file, directory and FIFO failure
  without a stale service, followed by aggressive-curve recovery.
- Standby snapshot retention; suspended replacement deferral and resume retry.
- Failed replacement cleanup and explicit subsequent load (no false rollback).

All six groups pass strict host C11 -Wall -Wextra -Werror -pedantic, ASan/UBSan,
ARM/qemu and real TV. Full make check also passes. Initial sanitizer build
incorrectly passed executable-only -no-pie to a shared-module link; it failed
before testing. DSOs were then linked with sanitizer flags without -no-pie and
the non-PIE test executable ran successfully. Failure/pass logs are both retained.

The ARM test ran from /tmp/oa-curve-module-20261101 with only the curve and ticker
DSOs. It links the real host lifecycle/registry/control objects and exercises
CLI command parsing directly; this was not an additional Unix-wire transport test
or complete native-system standby. There was no vendor HAL/MCU/audio-device call.

| tested ARM file | size | MD5 |
|---|---:|---|
| tests/audio_curve_module | 453460 | fe093c638f46271108a52b51ac515a06 |
| modules/mod_audio_curve.so | 22284 | a644c66e92bd6d53ef1032a990f8c4cc |
| modules/mod_ticker.so | 21612 | 9f1fdadf6f9ca721ed0a97f2d5da98eb |

Upload and size/hash verification were separate successful connections before
execution. Exit0; temporary fixtures, three staged files and staging directories
removed, no test process left. Boot ebb0aaaa-72a2-4410-9f8d-95b9249f8a90 and
stock PID1737 unchanged. Ethernet/MAC, SSH/telnet, hosts/Samba blocks and
hook/keeper identities verified afterward. Installed ABI1/recovery untouched.

New matching offline ABI2 build has13 modules. Core MD5
54a626f4c3a00b2e90490bc2626fe055, max GLIBC2.17. Header additions rebuilt the
existing artifacts too; use MEMORY for current hashes. This is not a new
all13-module on-TV --check claim: the historical11-module preflight and current
curve/ticker DSO runtime test have different scopes.

Evidence: ../analysis/cec-ownership-2026-11-01/curve-module-* and
live-curve-module-*. Existing AUDIO-VOLUME source identities remain authoritative.
