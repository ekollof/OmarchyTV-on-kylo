#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "openaurora/lnb_rt5047.h"

struct fake {
    int present;
    int get_rc;
    int set_rc;
    int get_calls;
    int set_calls;
    uint32_t pin;
    uint32_t level;
    uint32_t seen_pin;
    uint32_t seen_level;
};

static int get_lnb(void *ud, uint32_t *pin, uint32_t *level)
{
    struct fake *f = ud;
    f->get_calls++;
    if (f->get_rc < 0)
        return f->get_rc;
    if (!f->present)
        return 0;
    *pin = f->pin;
    *level = f->level;
    return 1;
}

static int set_level(void *ud, uint32_t pin, uint32_t level)
{
    struct fake *f = ud;
    f->set_calls++;
    f->seen_pin = pin;
    f->seen_level = level;
    return f->set_rc;
}

int main(void)
{
    const oa_rt5047_ops ops = { get_lnb, set_level };
    struct fake f = { .present = 1, .pin = 42, .level = 1 };
    assert(oa_rt5047_init(&ops, &f) == 1);
    assert(f.get_calls == 1 && f.set_calls == 1);
    assert(f.seen_pin == 42 && f.seen_level == 1);

    f = (struct fake){ .present = 0 };
    assert(oa_rt5047_init(&ops, &f) == 0);
    assert(f.get_calls == 1 && f.set_calls == 0);

    f = (struct fake){ .present = 1, .get_rc = -5 };
    assert(oa_rt5047_init(&ops, &f) == -1);
    assert(f.set_calls == 0);

    f = (struct fake){ .present = 1, .pin = 7, .level = 0, .set_rc = -3 };
    assert(oa_rt5047_init(&ops, &f) == -1);
    assert(f.set_calls == 1 && f.seen_pin == 7 && f.seen_level == 0);

    assert(oa_rt5047_init(NULL, &f) == -1);
    const oa_rt5047_ops partial = { get_lnb, NULL };
    assert(oa_rt5047_init(&partial, &f) == -1);
    puts("ok rt5047: LNB_EN present/absent/error flow and level write");
    return 0;
}
