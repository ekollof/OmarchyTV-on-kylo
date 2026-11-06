#ifndef OPENAURORA_CHANNELS_H
#define OPENAURORA_CHANNELS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Runtime channel-list provider (project boundary, not a vendor ABI).
 *
 * The request/response shape comes from the recovered testtool
 * ACTIVECHANNELLIST XML:
 *
 *   <active_list network='...' count='N' total_size='...'>
 *     <service rsn='...' name='...' handle='...' onid='...' tsid='...'
 *              sid='...' type='...'/>
 *   </active_list>
 *
 * ACTIVECHANNELLIST does not expose the runtime LCN or visibility/
 * HD-simulcast flags, so lcn_known/visibility_known stay 0. The list preserves
 * vendor row order. When no runtime list has been ingested the snapshot is
 * NULL and the UI keeps its stored-order fallback with the explicit
 * provisional marker.
 */

#define OA_CHANNEL_NAME_MAX 96
#define OA_CHANNELS_MAX 512

struct oa_channel_entry {
    uint32_t rsn;
    uint32_t handle;
    uint16_t onid;
    uint16_t tsid;
    uint16_t sid;
    uint8_t type;
    uint8_t reserved;
    char name[OA_CHANNEL_NAME_MAX];
};

struct oa_channel_snapshot {
    const struct oa_channel_entry *entries;
    size_t count;
    uint8_t source;             /* 1 = runtime active list */
    uint8_t lcn_known;          /* 0 for ACTIVECHANNELLIST */
    uint8_t visibility_known;   /* 0 for ACTIVECHANNELLIST */
};

typedef struct {
    const struct oa_channel_snapshot *(*snapshot)(void);
    /* Replaces the runtime list on success. Returns the stored row count. */
    int (*ingest_xml)(const char *xml, size_t len);
    void (*clear)(void);
} oa_channel_iface;

/* Per-instance runtime list. mod_channels owns one; the shared global iface
 * below keeps the direct unit-test/test-fixture path. */
struct oa_channel_list;
struct oa_channel_list *oa_channel_list_create(void);
void oa_channel_list_destroy(struct oa_channel_list *list);
const struct oa_channel_snapshot *oa_channel_list_snapshot(
    const struct oa_channel_list *list);
int oa_channel_list_ingest_xml(struct oa_channel_list *list, const char *xml,
    size_t len);
void oa_channel_list_clear(struct oa_channel_list *list);

/* Shared in-process implementation used by direct tests. */
extern const oa_channel_iface oa_channel_runtime_iface;

#ifdef __cplusplus
}
#endif

#endif
