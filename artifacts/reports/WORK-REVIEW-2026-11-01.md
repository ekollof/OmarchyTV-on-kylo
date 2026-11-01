# Review of the shutdown, worker and animation work

The worktree has no Git history. This review checked the current handoff,
reports, source and tests rather than inventing a commit diff. The initial full
strict host suite passed; additional lifetime tests exposed gaps it did not cover.

Fixed:

- CEC/audio quiescence read worker-mutated device/session fields without shared
  synchronization. Hardware structs are now worker-only; session state is
  published under the owner mutex. Quiesce reads only protected completion
  state and never waits for I/O or the formerly timed 500ms idle handoff.
- A failed pthread_join previously consumed the started flag. Joinability is
  now retained until successful join; self-join reports EDEADLK explicitly.
- A second CEC acquisition could read/change authority before rejecting an
  already-open device. It now rejects before authority access.
- An outstanding quiescence veto prevented retrying reload. Reload may retry
  release; it cannot replace a busy instance or override the gate with force.
  Start/resume/suspend still reject pending cleanup. Tests and the socket
  client smoke test now exercise explicit retries instead of depending on a
  half-second wait in a lifecycle callback.
- Audio's service bind_session called the synchronous owner RPC from the host
  loop. It now returns ENOTSUP for all arguments until async completion exists.
  The lower injected owner API is retained for non-loop callers. No live audio
  binding support is removed: that was never implemented or accepted.
- ui_anim lost its cadence timer after suspend. Resume now recreates it and
  reports timer-allocation failure so resume can retry. Service-triggered timer
  registration previously inherited the consumer's owner; it now uses a
  ui_anim-owned post. Consumer cleanup cannot silently remove that timer.
- Animation completion/finalise cleared a slot after calling its observer,
  deleting a new animation started by that observer. Slots are retired before
  notification; event data is copied and the active count includes callback
  replacements. Extreme float-to-int coordinate casts are bounded without
  changing the recovered float/truncation arithmetic for normal UI geometry.
- check-cec-owner omitted its ticker DSO dependency and failed in a fresh build
  directory. The dependency and shared lifecycle-test header dependencies were
  added. The CEC fixture's concurrently observed counters now use atomics.

Evidence: ../analysis/cec-ownership-2026-11-01/review-*.

- review-baseline-host: full strict suite before fixes.
- review-fixed-host: full strict make check after fixes, including new
  check-owner-races and check-ui-anim-review.
- review-sanitizer: focused owner/animation/audio-service/policy/quiescence/HAL
  groups with ASan, UBSan and float-cast-overflow instrumentation, exit0.
- review-arm-build / review-arm-qemu: ARM hard-float owner-race, animation DSO,
  CEC owner and nine shutdown/quiescence groups against target rootfs, exit0.
  These execute explicitly with qemu-arm-static -L; no device emulation claimed.

No firmware ABI or CEC wire policy was guessed or changed. These are native
host lifetime corrections, not vendor conformance findings. Default CFLAGS
retain C11, -Wall -Wextra -Werror -pedantic. No subagents were used.

The only TV operation was a view-only status/hash/listener/firewall query through
tvssh.sh. It found boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197, uptime6min,
live vendor Aurora1785 and zombie1967, superseding the preceding absence report.
The boot changed before the query; cause is unestablished. Installed ABI1 core,
hook and hosts hashes match MEMORY; SSH22/telnet2323, Samba REJECT and safety
markers remain. No helper/binary was uploaded, no process restarted and no
CEC, amplifier, framebuffer or settings operation was performed.

The reviewed source is ahead of preserved build-tv-v3/stage.md5. Isolated test
builds live in build-review-san and build-review-arm; installed/recovery files
and historical builds are unchanged. This is not a new TV --check or deploy.

Remaining acceptance: asynchronous CEC/audio service commands, coordinated live
socket/device ownership and full hardware parity. Queued animation timer failure
currently logs rather than notifying its consumer. Busy host shutdown gates can
wait indefinitely for a future worker post; queue emptiness is not dead-worker
detection. These limits are tracked in TODO, not marked accepted.
