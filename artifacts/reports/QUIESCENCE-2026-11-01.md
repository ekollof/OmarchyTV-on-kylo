# Busy-module cleanup gate — 2026-11-01

Follow-up: WORK-REVIEW-2026-11-01 permits retrying reload while cleanup is
pending. It still cannot replace a busy instance or bypass release with force.
The old blanket reload rejection below describes the initial implementation.

Audio's owned netlink session and raw CEC's failed-close retention require code
and context to remain valid until release finishes. The old void stop/destroy
callbacks could not refuse unload. ABI 3 adds optional int quiesce(void *ctx),
called on the loop thread before destructive cleanup, also after failed init/
start and for never-started instances. Gates must tolerate partial/NULL contexts,
be nonblocking and idempotent. Zero promises cleanup can finish synchronously.

Nonzero retains DSO/context/registrations/posts/holds, reports cleanup_pending
in status, and rejects activation, suspend/resume and reload. Stop/unload can
retry; --force cannot bypass the gate. A veto can already have begun quiescence,
so it does not promise continued service. Failed init remains named/discoverable
for cleanup but never activates. Rejected reload candidates remain rejected.
Partial starts eventually receive stop; never-started instances do not.

The magic/version bump prevents older hosts from accepting a descriptor while
ignoring this gate. Existing modules omit it and retain synchronous behavior.
Installed ABI 1 and historical ABI 2 artifacts are preserved. Current matching
development build is openaurora/build-tv-v3. No hardware worker integration is
claimed by this change.

Six real-DSO regression groups cover running/loaded/suspended owners, force,
post completion, failed init/partial start, deferred activation failure, retry,
status visibility and cleanup. They pass strict host, ASan/UBSan, ARM/qemu and
actual TV. The full strict host suite passes; sanitizer safety/reload/phases/
power/control suites also pass. ABI 1/2 and short guard-page fixtures reject.
Default CFLAGS retain -Werror -pedantic.

Actual TV: foreground upload to /tmp/oa-quiesce-20261101, separate size/hash
verification, core plus all 13 production modules --check, then device-free
regression executable. Both commands returned zero. No production module was
started or hardware ownership taken. Core max required GLIBC2.17.
Staging removed; boot ebb0aaaa-72a2-4410-9f8d-95b9249f8a90 and live Aurora PID1737
unchanged; SSH22/telnet2323, hook/hosts/core hashes and Samba REJECT remain intact.

Selected MD5 identities (full matching list in build-tv-v3/stage.md5):

| artifact | MD5 |
|---|---|
| core | 8b06fd51c9ff662f13815ef6406dc184 |
| audio | d89c03929e435db2d5394d86f8b5ea92 |
| audio_curve | 73ba8ff7ff250ffd52738c75b148ecf4 |
| quiesce test | 90088588bd6539060a2266c4ac4dfe02 |
| quiesce DSO fixture | 091d9399b7ee239473896e1f0deab321 |

Raw logs: ../analysis/cec-ownership-2026-11-01/quiesce-* and live-quiesce-*.
Errors logged by veto/failure fixtures are expected assertions of rejection.

The host-loop gap described here is superseded by
[SHUTDOWN-2026-11-01](SHUTDOWN-2026-11-01.md): shutdown now dispatches completion
work and retries stop/unload before it can dlclose. That later report does not
add a hardware worker or change this gate's contract. No full native CEC/audio
acceptance is implied.
