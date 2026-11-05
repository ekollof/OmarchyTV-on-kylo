#ifndef OPENAURORA_PVR_H
#define OPENAURORA_PVR_H

#include <stddef.h>
#include <stdint.h>

#include "openaurora/broadcast.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PVR backend boundary shaped after the recovered vendor interfaces:
 *
 *   DvrController::startRecording(std::vector<DvrStream> const&,
 *       EASIReminder const&, DateTime start, DateTime end, uint, uint,
 *       DvrRecordingType, bool, uint)                       @0x00af6a45
 *   DvrController::recordingQuery(int index, DvrRecordingInfo&,
 *       ICEDVR_RecordParams&, DvrRecordingStats&)            @0x00af3a25
 *   DvrController::stopRecording(int recordingId)             @0x00af66e5
 *   DvrController::removeRecording(int)                       @0x00af4e69
 *   NebulaPvrRecordingScheduler getRecordings/getStartTime/getDuration/
 *       getRepeatDays/getStartPadding/getEndPadding
 *
 * Identity is an integer recording id; enumeration is index-based. The stub
 * below is a project file store, not the vendor SQLite metadata store: it
 * persists the request/job fields atomically and has no active recording,
 * timeshift, playback, storage-manager or conflict engine.
 */

#define OA_PVR_MAX_JOBS 256
#define OA_PVR_PATH_MAX 512

struct oa_pvr_backend {
    const char *name;
    int (*open)(const char *path);
    void (*close)(void);
    int (*start)(const struct oa_pvr_request *request, uint32_t *recording_id);
    int (*stop)(uint32_t recording_id);
    size_t (*count)(void);
    int (*query)(size_t index, struct oa_pvr_job *out);
};

const struct oa_pvr_backend *oa_pvr_stub_backend(void);

#ifdef __cplusplus
}
#endif

#endif
