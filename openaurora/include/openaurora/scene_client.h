/* Single-thread/process nonblocking parent of the separate graphics helper.
 * No EGL dependency. Spawn/setup is explicit; steady-state I/O and shutdown
 * never wait for GPU work or child exit. Integrate fd watch + reap timer in host.
 */
#ifndef OPENAURORA_SCENE_CLIENT_H
#define OPENAURORA_SCENE_CLIENT_H
#include "openaurora/scene_broker.h"
typedef struct oa_scene_client oa_scene_client;
/* Absolute paths; stdout/stderr redirected before exec. Success means spawned,
 * not initialized: submit OPEN and collect its completion. Target libc may
 * report spawn success before exec failure. *out must be NULL. */
int oa_scene_client_spawn(oa_scene_client **out, const char *helper, const char *renderer);
/* request.ticket is ignored; generated tickets are instance-scoped. One pending
 * request or uncollected result. Caller retains image fd; close it after success
 * or reuse after EAGAIN. No input mapping/copy/seal creation on this path.
 * Submitted OPEN/FRAME success is never a committed-frame/readiness receipt. */
int oa_scene_client_submit(oa_scene_client *client, const oa_scene_request *request, int image, uint64_t *ticket);
/* At most two nonblocking receives and one WNOHANG reap. No callbacks. Can run
 * while a result is uncollected; valid buffered completion survives child exit.
 * Returns0 on a healthy channel, -1/errno when broken. Still collects/reaps on
 * later calls. A broken channel completes pending request with an error value. */
int oa_scene_client_pump(oa_scene_client *client);
/* 1 copied result (consumed),0 pending,-1 invalid/stale ticket. */
int oa_scene_client_take(oa_scene_client *client, uint64_t ticket, oa_scene_reply *reply);
int oa_scene_client_fd(const oa_scene_client *client);
/* Close IPC once; retain child until WNOHANG reaped, never send a signal. Any
 * pending request becomes ECANCELED (not a claim GPU work was canceled).
 * Returns0 draining,1 reaped/absent,-1 local error. Repeated calls safe. */
int oa_scene_client_shutdown(oa_scene_client *client);
/* Raw wait status after reap; EAGAIN while live. A reaped abnormal child does
 * NOT certify completed EGL cleanup; callers must treat it as a hardware fault. */
int oa_scene_client_status(const oa_scene_client *client, int *status);
/* Nonblocking; EBUSY until shutdown/reap. NULL idempotent. */
int oa_scene_client_destroy(oa_scene_client **client);
#endif
