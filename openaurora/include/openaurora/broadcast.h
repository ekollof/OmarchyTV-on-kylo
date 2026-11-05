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
 * currently shipped is the host fixture in openaurora/tests plus the
 * firmware-grounded EIT time/schedule code in openaurora/common.
 *
 * Firmware mapping and deltas are documented in
 * artifacts/reports/VENDOR-BROADCAST-ABI-2026-11-05.md. Key facts:
 *   - vendor EIT ranges are uint32 seconds; this contract uses int64 ms and
 *     converts at the service boundary (common/eit_time.c implements the exact
 *     MJD/BCD decode and range algebra);
 *   - vendor EPG identity is (service id, EIT event id) with CRID,
 *     first-showing, running-status/state/version/actual metadata; the fields
 *     below mirror the recovered EITEventKey and ScheduleEvent schema;
 *   - vendor tuner status exposes per-path lock, strength, quality, BER/SNR
 *     floats and an UNC sentinel (0xffffffff means unavailable);
 *   - vendor PVR scheduling is EASIReminder + explicit start/end + stream list,
 *     enumeration is index-based and cancellation is by recording id. The
 *     repeat/padding fields mirror NebulaPvrRecordingScheduler naming, but the
 *     request shape here remains a project abstraction.
 */

#define OA_TUNER_UNC_UNKNOWN UINT32_MAX

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

/* Mirrors DEV_NIM_DTV_GetSignalStrength/Quality/BER/SNR/UNC and
 * DEV_NIM_DTV_CheckDEMODLock. BER/SNR are floats with a 0.0 default; unc is
 * OA_TUNER_UNC_UNKNOWN when the front end cannot report it. */
struct oa_tuner_status {
    uint8_t network;        /* enum oa_tuner_network */
    uint8_t scan_state;     /* enum oa_tuner_scan_state */
    uint8_t progress;       /* 0..100 */
    uint8_t locked;         /* vendor CheckDEMODLock result */
    uint32_t services_found;
    int error;              /* 0 or positive errno-ish status */
    uint32_t selected_handle;
    uint32_t signal_strength;
    uint32_t signal_quality;
    uint32_t unc;
    float ber;
    float snr;
};

/* Loop-thread service; scan_start/cancel do not block. */
typedef struct {
    int (*status)(struct oa_tuner_status *out);
    int (*scan_start)(uint8_t network, int clear_existing);
    int (*scan_cancel)(void);
    int (*select)(uint32_t service_handle);
    uint32_t (*selected)(void);
} oa_tuner_iface;

/* Event identity and metadata mirror EITEventKey (begin/end seconds, event id,
 * version/state/actual, running status) plus the ScheduleEvent columns
 * ProgramCridId/FirstShowing. Times are milliseconds at this boundary. */
struct oa_epg_event {
    uint32_t service_handle;
    uint32_t event_id;
    int64_t start_ms;
    int64_t end_ms;
    uint32_t program_crid_id;
    uint32_t running_status;
    uint8_t first_showing;
    uint8_t state;          /* vendor EventState bits (0..3) */
    uint8_t version;        /* EIT version 0..31 */
    uint8_t actual;         /* present/following actual flag */
    char name[96];
    char description[160];
};

/* Returns 0 and the full match count. Copies at most cap events. */
typedef struct {
    int (*query)(uint32_t service_handle, int64_t from_ms, int64_t to_ms,
        struct oa_epg_event *events, size_t cap, size_t *count);
} oa_epg_iface;

/* Ingest side of the EIT-backed EPG module. feed_section accepts one complete
 * DVB EIT section (header + CRC) and returns the number of stored/replaced
 * events or a negative errno; clear drops all cached events. This is a project
 * boundary for a future tuner/EIT source, not a vendor ABI. */
typedef struct {
    int (*feed_section)(const uint8_t *data, size_t len);
    void (*clear)(void);
    size_t (*count)(void);
} oa_epg_ingest_iface;

enum oa_pvr_state {
    OA_PVR_SCHEDULED = 0,
    OA_PVR_RECORDING = 1,
    OA_PVR_COMPLETED = 2,
    OA_PVR_CANCELED = 3,
    OA_PVR_FAILED = 4
};

struct oa_pvr_request {
    uint32_t service_handle;
    int64_t start_ms;
    int64_t end_ms;
    uint32_t repeat_days;   /* project bitmask; vendor getRepeatDays domain */
    int32_t start_padding;  /* seconds, vendor start padding */
    int32_t end_padding;    /* seconds, vendor end padding */
    char name[64];
};

struct oa_pvr_job {
    uint32_t id;            /* project handle; maps to vendor recording id */
    uint32_t service_handle;
    int64_t start_ms;
    int64_t end_ms;
    uint32_t repeat_days;
    int32_t start_padding;
    int32_t end_padding;
    uint8_t state;          /* enum oa_pvr_state */
    uint8_t reserved[3];
    char name[64];
};

typedef struct {
    int (*list)(struct oa_pvr_job *jobs, size_t cap, size_t *count);
    int (*add)(const struct oa_pvr_request *request, uint32_t *id);
    int (*cancel)(uint32_t id);
} oa_pvr_iface;

#ifdef __cplusplus
}
#endif

#endif
