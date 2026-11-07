/* app.launch — request a stock/vendor application launch.
 *
 * This is the consumer for OA_TOPIC_ACTION SELECT events. It does not link the
 * vendor app-manager library itself; a configured helper (for example
 * oa-app-launch) performs the platform-specific LIBNAM call in a child process.
 * Keeping the vendor call out of the host loop also keeps a blocking IPC/launch
 * path out of the UI thread.
 */
#ifndef OPENAURORA_APP_LAUNCH_H
#define OPENAURORA_APP_LAUNCH_H

#ifdef __cplusplus
extern "C" {
#endif

#define OA_SVC_APP_LAUNCH "app.launch"

typedef struct {
    /* name is the launcher/application name; args is optional. Returns 0 when
     * the helper was spawned, -1 with errno set otherwise. The request copies
     * both strings and never keeps the caller's pointers. */
    int (*request)(const char *name, const char *args);
} oa_app_launch_iface;

#ifdef __cplusplus
}
#endif

#endif /* OPENAURORA_APP_LAUNCH_H */
