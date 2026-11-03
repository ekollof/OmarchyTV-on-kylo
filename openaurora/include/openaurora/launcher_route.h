#ifndef OPENAURORA_LAUNCHER_ROUTE_H
#define OPENAURORA_LAUNCHER_ROUTE_H
#include <stddef.h>
#include <stdint.h>
/* NovoLauncherOperationId -> signed dialog result. This is not an IR/ICE key
 * conversion. The complete shipped 62-entry table includes -1 holes. */
int32_t oa_launcher_operation_result(uint32_t operation);
enum oa_launcher_action_kind {
    OA_LAUNCHER_SET_TAB=1, OA_LAUNCHER_SET_ITEM,
    OA_LAUNCHER_OPEN_SUBMENU, OA_LAUNCHER_PUSH_FRONT
};
struct oa_launcher_action { enum oa_launcher_action_kind kind; int32_t value; };
/* In-process data only, not a wire encoding or reload snapshot. PUSH_FRONT
 * means enqueue, not immediate dialog construction/reentrant dispatch. */
struct oa_launcher_plan { size_t count; struct oa_launcher_action actions[4]; };
/* Ordered selection/navigation writes for the recovered unconditional settings
 * branches in OperationFinder, after launcher modal completion. No execution,
 * IPC, provider calls or telemetry. A missing write means retain prior state.
 * Other operations and branches needing extra runtime/portal work return
 * ENOTSUP. EINVAL/ENOTSUP leave output unchanged. UI service integration and
 * live acceptance remain separate; this helper does not implement doModal. */
int oa_launcher_settings_plan(int32_t result,struct oa_launcher_plan *out);
#endif
