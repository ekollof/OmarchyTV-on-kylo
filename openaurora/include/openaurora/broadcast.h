#ifndef OPENAURORA_BROADCAST_H
#define OPENAURORA_BROADCAST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Project-internal broadcast service contracts. These are NOT claimed to match
 * a vendor object ABI; they establish the boundaries that a future vendor-
 * backed tuner, EIT/EPG and PVR module must satisfy. The only implementation
 * currently shipped is the host fixture in openaurora/tests.
 */

enum oa_tuner_network {
    OA_TUNER_NET_UNKNOWN = 0,
    OA_TUNER_NET_DVB_T = 1,
    OA_TUNER_NET_DVB_C = 2,
    OA_TUNER_NET_DVB_S = 3,
    OA_TUNER_NET_ANALOG = 4,
    OA_TUNER_NET_IP = 5
};

enum oa_tuner_scan_state {
    OA_TUNER_IDLE = 0,
    OA_TUNER_SCANNING = 1,
    OA_TUNER_DONE = 2,
    OA_TUNER_CANCELED = 3,
    OA_TUNER_FAILED = 4
};

struct oa_tuner_status {
    uint8_t network;        /* enum oa_tuner_network */
    uint8_t scan_state;     /* enum oa_tuner_scan_state */
    uint8_t progress;       /* 0..100 */
    uint8_t reserved;
    uint32_t services_found;
    int error;              /* 0 or positive errno-ish status */
    uint32_t selected_handle;
};

/* Loop-thread service; scan_start/cancel do not block. */
typedef struct {
    int (*status)(struct oa_tuner_status *out);
    int (*scan_start)(uint8_t network, int clear_existing);
    int (*scan_cancel)(void);
    int (*select)(uint32_t service_handle);
    uint32_t (*selected)(void);
} oa_tuner_iface;

struct oa_epg_event {
    uint32_t service_handle;
    uint32_t event_id;
    int64_t start_ms;
    int64_t end_ms;
    char name[96];
    char description[160];
};

/* Returns 0 and the full match count. Copies at most cap events. */
typedef struct {
    int (*query)(uint32_t service_handle, int64_t from_ms, int64_t to_ms,
        struct oa_epg_event *events, size_t cap, size_t *count);
} oa_epg_iface;

enum oa_pvr_state {
    OA_PVR_SCHEDULED = 0,
    OA_PVR_RECORDING = 1,
    OA_PVR_COMPLETED = 2,
    OA_PVR_CANCELED = 3,
    OA_PVR_FAILED = 4
};

struct oa_pvr_job {
    uint32_t id;
    uint32_t service_handle;
    int64_t start_ms;
    int64_t end_ms;
    uint8_t state;          /* enum oa_pvr_state */
    uint8_t reserved[3];
    char name[64];
};

typedef struct {
    int (*list)(struct oa_pvr_job *jobs, size_t cap, size_t *count);
    int (*add)(uint32_t service_handle, int64_t start_ms, int64_t end_ms,
        const char *name, uint32_t *id);
    int (*cancel)(uint32_t id);
} oa_pvr_iface;

#ifdef __cplusplus
}
#endif

#endif
