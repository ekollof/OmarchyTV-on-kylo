#include "openaurora/cec_module_transport.h"

static const oa_cec_module_transport *g_transport;

const oa_cec_module_transport *oa_cec_module_transport_get(void)
{
    return g_transport;
}

void oa_cec_module_transport_set(const oa_cec_module_transport *transport)
{
    g_transport = transport;
}
