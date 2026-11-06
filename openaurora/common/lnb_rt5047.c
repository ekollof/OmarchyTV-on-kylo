#include "openaurora/lnb_rt5047.h"

int oa_rt5047_init(const oa_rt5047_ops *ops, void *ud)
{
    if (!ops || !ops->get_lnb_en || !ops->set_level)
        return -1;
    uint32_t pin = 0;
    uint32_t level = 0;
    int present = ops->get_lnb_en(ud, &pin, &level);
    if (present < 0)
        return -1;
    if (present == 0)
        return 0;
    if (ops->set_level(ud, pin, level) != 0)
        return -1;
    return 1;
}
