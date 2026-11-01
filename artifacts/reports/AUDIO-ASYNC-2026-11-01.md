# Asynchronous audio-owner requests

The existing owner RPCs execute I/O on a worker but block their caller. The
loop-thread bind_session service therefore remains disabled. This continuation
adds nonblocking lower-owner *_async open/gain/close calls and take() results
without changing that service's advertised capability or selecting native I/O.

Each owner has one fixed request/result slot. Submission returns a nonzero
instance-scoped ticket; EBUSY rejects a second request until the result is taken.
Synchronous RPCs also reject while that async slot is occupied. No caller output
pointer or user completion callback survives submission. Result values include
ticket, operation, status, captured worker errno and successful gain data.
take() copies/consumes once; a not-ready call leaves caller output unchanged.
Counter exhaustion rejects instead of wrapping. Tickets cannot cross reload.

Open copies the injected I/O table. Its stable session copy survives collecting
the open result and replacing the request slot with later gain/close jobs.
A rejected second open does not overwrite the active table. Callback context
and code are still borrowed until close or quiesce+join; module integration
must account for that lifetime. NULL I/O never selects a system socket.

Quiesce refuses new submissions, drains an accepted request, closes the resulting
session on the worker and then permits join. An untaken value result remains
available until destruction, which discards it without invoking caller code.
OS-close errors consume the audio fd as before. Completion is query/transport
status, never a DSP-operation or audible-output acknowledgement.

Three groups in tests/audio_async.c pass strict host, ASan/UBSan, ARM/qemu and
actual TV. They cover a deterministically blocked open, nonblocking submissions,
copied callback-table lifetime, fd0, backpressure, synchronous/async exclusion,
collected-once value results, gain data, failed second open, OS-close errno,
gain after close, and quiesce during accepted blocked work. Full host make check
passes. Existing owner/module/policy tests pass under sanitizers as well.

The CEC worker's receive returns1 for a decoded message. Its synchronous wrapper
previously treated that as an error for errno propagation; it now propagates
worker errno only for negative returns. A fake raw receive deliberately leaves
stale worker errno and verifies positive success leaves caller errno alone.
That regression passes host/sanitizer/ARM-qemu; it sends/receives nothing on TV.

Raw evidence: ../analysis/cec-ownership-2026-11-01/audio-async-* and
live-audio-async-*. Native test75264bytes, MD571a53754d8c48fe97cfb65580c0f8d2c;
the exact tested binary is preserved there as audio-async-tested-tv.elf.
Foreground upload, separate size/hash verification, then execution returned0.
Staging /tmp/oa-audio-async-20261101 was removed. Every socket/device callback
in this executable is injected; no real netlink, CEC, amplifier or display access.
Boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197, live Aurora1785, installed core/hook/
hosts hashes, SSH/telnet and Samba block remained intact. No deploy/restart.

Next: CEC async requests, then module service adapters with explicit holds,
cancellation and polling/notification. Native socket/process ownership, real
gain/table/profile writes and hardware parity remain unaccepted. Preserved
build-tv-v3 and recovery artifacts were not rebuilt or replaced.
