/* Module-level CEC transport override.
 *
 * Production returns NULL, so mod_cec uses /dev/mem + the raw ntkcec device.
 * A host test shared object installs a fake authority-memory and device I/O
 * table from a constructor; this is the only way to drive the real module
 * acquire/send/receive/release path without a TV.
 */
#ifndef OPENAURORA_CEC_MODULE_TRANSPORT_H
#define OPENAURORA_CEC_MODULE_TRANSPORT_H

#include "openaurora/cec_authority_mem.h"
#include "openaurora/cec_device.h"

typedef struct {
    const oa_cec_authority_mem_io *authority_mem;
    const oa_cec_device_io *device;
} oa_cec_module_transport;

const oa_cec_module_transport *oa_cec_module_transport_get(void);
void oa_cec_module_transport_set(const oa_cec_module_transport *transport);

#endif
