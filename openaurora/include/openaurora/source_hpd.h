#ifndef OPENAURORA_SOURCE_HPD_H
#define OPENAURORA_SOURCE_HPD_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Pure HDMI HPD provider model from the shipped ICE loop.
 *
 * Recovered from aurora.elf 64c252340d3af6c5d66072611b7bad7a:
 *   ice_externalCheckCbStatus                       @ 0x007f58ec
 *   _ice_CheckHdmiPlugStatus                        @ 0x007f56c8
 *   PLAT_VIU_GET_HDMI_IDENT                         @ 0x00901a38
 *   extInterfaceCallback                            @ 0x00a26e44
 *   ExternalAVSourceManager::publishExternalSourceState @ 0x00a26514
 *   InputManager::externalSourceStatusChanged       @ 0x00fb02f8
 * and libNDAL3.so Hal_Gpio_Read                    @ 0x4e7be4cc
 * and ntkdriver GPIO_Ioctl                         @ 0x0003be18.
 *
 * The stock thread sleeps 1500 ms, then polls four HPD inputs every 30 ms.
 * _ice_CheckHdmiPlugStatus calls PLAT_VIU_GET_HDMI_IDENT(0..3).
 * PLAT_VIU_GET_HDMI_IDENT maps argument 0 to the 0xcb fallback (pin 203) and
 * arguments 1..3 to the table at 0x0139f448 (200, 201, 202).
 * _ice_ext_GetHDMICallbackPosition maps the same 0..3 to the HDMI1..4
 * source-list positions 4..7 (table at 0x0175de8c), so the physical order is
 * HDMI1=203, HDMI2=200, HDMI3=201, HDMI4=202. Hal_Gpio_Read has a separate
 * 200..204 bulk branch: it sends kernel getter 0x6712 and reads five bytes,
 * without sysfs export, pin claim or GPIO configuration. The five bytes map to
 * pins 203, 202, 200, 201, 204 at indices 0..4, so HDMI1..4 map to indices
 * 0, 2, 3, 1. The live sample with the Deck on HDMI1 was 01 01 00 01 00;
 * HDMI1=byte0 is live-confirmed, HDMI2..4 follow the same loop/table order.
 *
 * extInterfaceCallback maps the ICE callback status to ExternalSourceStatus:
 *   0 -> 3 (connected)        on a 0->1 HPD transition;
 *   1 -> 4 (disconnected)     then 2 -> 2 (attributes changed) on 1->0.
 * InputManager::externalSourceStatusChanged consumes 3/4 for HDMI (type 7) and
 * calls NovoCECController::onHDMIDisconnected on 4. The source-loss policy
 * consumes that disconnected event.
 *
 * This is a model, not a device access layer. The caller owns the GPIO fd and
 * the poll thread. The V4L2 event 0xc000001 is a scaler update, not this
 * provider (see source_hdmi_event.h).
 */
enum oa_source_hpd_status {
    OA_SOURCE_HPD_NONE = 0,
    OA_SOURCE_HPD_ATTRIBUTES_CHANGED = 2,
    OA_SOURCE_HPD_CONNECTED = 3,
    OA_SOURCE_HPD_DISCONNECTED = 4
};

#define OA_SOURCE_HPD_PORTS 4u
#define OA_SOURCE_HPD_BULK_BYTES 5u
#define OA_SOURCE_HPD_INITIAL_DELAY_MS 1500u
#define OA_SOURCE_HPD_POLL_MS 30u

struct oa_source_hpd_event {
    unsigned port; /* 1..4 */
    enum oa_source_hpd_status status;
};

struct oa_source_hpd_state {
    uint8_t level[OA_SOURCE_HPD_PORTS]; /* last raw sample, 0/1 */
};

/* Port 1..4 -> GPIO 200..203; 0 for an invalid port. */
unsigned oa_source_hpd_pin(unsigned port);

/* Port 1..4 -> 0x6712 payload index 2,3,1,0; 0xff for an invalid port. */
unsigned oa_source_hpd_bulk_index(unsigned port);

/* Initialises all last levels to 0, the stock BSS default. */
void oa_source_hpd_init(struct oa_source_hpd_state *state);

/* Applies one raw 0/1 sample for a port. On a change, writes the stock event
 * sequence to events (at most max) and returns the count:
 *   0 -> 1: CONNECTED;
 *   1 -> 0: DISCONNECTED then ATTRIBUTES_CHANGED.
 * No change returns 0. Invalid arguments return -EINVAL; a too-small output
 * buffer returns -ENOSPC. State is unchanged on failure. */
int oa_source_hpd_update(struct oa_source_hpd_state *state, unsigned port,
                         uint8_t level, struct oa_source_hpd_event *events,
                         size_t max);

/* Injectable bulk getter. read_bulk fills the five 0x6712 bytes. The adapter
 * maps one port and normalises any nonzero byte to 1, matching Hal_Gpio_Read.
 * Returns 0 or -errno; level is unchanged on failure. */
struct oa_source_hpd_io {
    void *context;
    int (*read_bulk)(void *context, uint8_t bytes[OA_SOURCE_HPD_BULK_BYTES]);
};

int oa_source_hpd_sample(const struct oa_source_hpd_io *io, unsigned port,
                         uint8_t *level);

#ifdef __cplusplus
}
#endif

#endif
