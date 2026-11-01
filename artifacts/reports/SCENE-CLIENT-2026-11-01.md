# Nonblocking graphics parent client — 2026-11-01

Status: maintained lower parent client implemented and tested on host and TV,
including410 actual GPU frame requests through the diagnostic adapter. Host
service/module integration, readiness, coalescing and UI lifetime remain open.
This extends SCENE-HELPER; it does not make the production display asynchronous.

## Contract and ownership

`common/scene_client.c` / `include/openaurora/scene_client.h` own a separate
helper process and seqpacket connection. No EGL library is linked into the
parent. Spawn/setup is explicit; successful spawn is not initialization. The
caller submits OPEN and waits for its successful completion before submitting
frames. Instance-scoped tickets prevent an old session's results matching a new
one within a loaded library. A DSO reload can reset the static instance counter;
GRAPHICS-MODULE maps public tickets through the host owner id to cover reload.
State is single-threaded and must be serialized by the integrating host.
Inherited clients reject use after fork with EPERM.

One accepted request or uncollected result occupies the bounded slot. Submit
only uses MSG_DONTWAIT; it does not create/copy/map image data. The caller owns
the immutable image descriptor and can close it after successful send. EAGAIN
leaves the request unaccepted and the output ticket unchanged. EINTR is not
replayed: the channel breaks. Upload revisions must increase, cached frames name
the last successfully completed revision, and frames require completed OPEN.
Once CLOSE was accepted, only CLOSE retries are allowed until shutdown. A failed
CLOSE reply can be collected before another CLOSE; other operation errors break
the connection after preserving their reply.

Pump performs at most two nonblocking receives and one WNOHANG waitpid. The
second receive handles a final reply buffered during child exit. Pending work
becomes a copied error result if the channel fails; an already valid result is
preserved. Extra descriptors, malformed replies, operation/revision/ticket
mismatches and unsolicited messages break the connection. A timer must keep
pumping/reaping even after a watch detects EOF.

Shutdown closes IPC once and returns immediately while the helper drains.
Pending work becomes local ECANCELED, which explicitly does not claim GPU work
was canceled. No signal/kill is sent. Destroy returns EBUSY until shutdown and
child reap complete. Raw child wait status remains available: a reaped abnormal
child is a hardware fault, not proof of completed EGL cleanup. External reaping
(ECHILD) is an ownership error, not silently accepted as a successful close.

`tools/display/scene-probe-client.c` now adapts this maintained client for the
existing full-backup diagnostic. Only this diagnostic adapter waits/polls for
completion and drain. Do not copy those waits into a module or ABI3 gate.

## Validation

Five process/lifecycle groups pass strict C11/-Wall/-Wextra/-Werror/-pedantic,
ASan/UBSan and actual TV:

1. Pending/uncollected result backpressure, image upload/cache, stale revisions,
   instance tickets, foreign-fork rejection and successful CLOSE.
2. FIFO-gated executing finish and blocked close: repeated shutdown/pump/destroy
   cannot wait for or discard the live child. A local canceled result remains
   collectable; releasing the gate allows drain/reap. Fixtures access no hardware.
3. Buffered completion followed by exit, wrong tickets, unexpected descriptor
   replies and an intentionally SIGKILLed test peer with visible raw wait status.
   Only our own test peer is signaled, never stock Aurora or a GPU owner.
4. Injected send EAGAIN leaves the request retryable; EINTR makes exactly one
   send attempt, breaks the channel and is not replayed.
5. Invalid executable with X_OK: accepts immediate ENOEXEC or asynchronous EOF
   followed by child127, supporting the target libc semantics from MIXER-MODULE.

Complete host `make check`:214 checks pass. No new ARM/qemu subprocess claim;
these tests spawn ARM children on actual TV instead. Previous lower IPC qemu
coverage remains valid. Native test90108bytes, MD54068c3b34d51bb38e0b340fdeb3a2762,
maxGLIBC2.15. Updated fixture14768bytes/245513b064ba9347aa7aea244d135eb7.

The actual native diagnostic uses this client for410 requests: two identity
frames and51 frames for each of eight effects. BGRA corner checks match, each
effect takes1004.80..1016.43ms at the diagnostic's50Hz pacing. Child exit status0;
full16588800-byte backing, pan0, MAU0 and region restored with snapshot_match1.
This verifies real GPU submit/completion/drain through the maintained parent,
without a new independent visual observation or production UI acceptance.

Native artifacts in `openaurora/build-tv-v3/scene-client`:

| artifact | bytes | MD5 | maximum glibc |
|---|---:|---|---|
| scene-window-probe |91068|df6428c2f968b617121474b15fc933c7|2.17|
| tests/scene_client |90108|4068c3b34d51bb38e0b340fdeb3a2762|2.15|
| oa-scene-broker |57304|596643ed63c76231e6a7e31fd22d7efd|2.4|
| scene-renderer.so |68064|f4196fa2413cbf60efdfb136d8b56e4b|2.4|

Foreground upload and separate size/hash verification preceded execution.
Staging and the fsynced backup at `/tmp/oa-scene-helper-20261101` were removed
after independent restoration verification; no test process or FIFO directory
remains. Stock boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197, live Aurora1785 and
zombie1967 unchanged. Full framebuffer MD5before/after41735daa69dc137743b59efe12db4a19,
pan0,0; hook/hosts hashes, SSH22/telnet2323, Samba REJECT and safety markers
remain. No persistent deployment. Matching16-module production stage and older
helper diagnostics preserved. Evidence: `artifacts/analysis/scene-client-2026-11-01/`.

## Next integration

Follow-up: GRAPHICS-MODULE-2026-11-01 now implements/tests the provider's owned
watch/timer/quiescence and actual GPU service frames. Consumer coalescing,
first-frame readiness and ui_fb lifecycle below remain pending.

Expose an asynchronous provider with owned fd watch and reap timer. Reserve
completion machinery before spawning. During ABI3 quiescence, close IPC and
return busy until the client reports reap; keep timer/context/code alive. Connect
consumers with service holds and copied completion values. Coalesce transforms
while busy, retain immutable pixel revisions, and retry a final frame after its
animation timer disarms. First-frame readiness must follow a successful FRAME
completion, never submit success. Add explicit hardware-fault behavior on abnormal
child exit. Then test module reload/suspend/resume and actual UI presentation.
CPU display remains production; no display.renderer configuration option exists.
