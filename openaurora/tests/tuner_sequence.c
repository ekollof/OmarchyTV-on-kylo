#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/tuner_sequence.h"

enum { C_DEMOD, C_TUNER, C_AV, C_DVIF, C_SET, C_LNB, C_MAX };

struct fake {
    int calls[C_MAX];
    int n;
    int demod_rc;
    int tuner_rc;
    int av_rc;
    int dvif_handle;
    int set_rc;
    int demod_index;
    int set_handle;
    uint32_t set_type;
    int set_calls;
    int lnb_rc;
};

static int c_demod(void *ud, int index)
{
    struct fake *f = ud;
    f->calls[f->n++] = C_DEMOD;
    f->demod_index = index;
    return f->demod_rc;
}

static int c_tuner(void *ud)
{
    struct fake *f = ud;
    f->calls[f->n++] = C_TUNER;
    return f->tuner_rc;
}

static int c_av(void *ud)
{
    struct fake *f = ud;
    f->calls[f->n++] = C_AV;
    return f->av_rc;
}

static int c_dvif(void *ud)
{
    struct fake *f = ud;
    f->calls[f->n++] = C_DVIF;
    return f->dvif_handle;
}

static int c_set(void *ud, int handle, uint32_t type)
{
    struct fake *f = ud;
    f->calls[f->n++] = C_SET;
    f->set_calls++;
    f->set_handle = handle;
    f->set_type = type;
    return f->set_rc;
}

static int c_lnb(void *ud)
{
    struct fake *f = ud;
    f->calls[f->n++] = C_LNB;
    return f->lnb_rc;
}

static const oa_tuner_sequence_ops ops = {
    c_demod, c_tuner, c_av, c_dvif, c_set, c_lnb
};

static void reset(struct fake *f)
{
    memset(f, 0, sizeof(*f));
    f->av_rc = 1;
    f->dvif_handle = 7;
    f->lnb_rc = 1;
}

static void test_mxl661(void)
{
    struct fake f;
    reset(&f);
    oa_tuner_sequence_result r;
    assert(oa_tuner_sequence_run(&ops, &f, 0xd, &r) == 0);
    assert(f.n == 6);
    assert(f.calls[0] == C_DEMOD && f.calls[1] == C_TUNER &&
           f.calls[2] == C_AV && f.calls[3] == C_DVIF &&
           f.calls[4] == C_SET && f.calls[5] == C_LNB);
    assert(f.demod_index == 0);
    assert(f.set_calls == 1 && f.set_handle == 7 && f.set_type == 0xc);
    assert(r.demod_rc == 0 && r.tuner_rc == 0 && r.av2018_rc == 1);
    assert(r.dvif_handle == 7 && r.set_type_rc == 0 && r.lnb_called == 1);
    assert(r.lnb_rc == 1);
    assert(r.tuner_type == 0xc);
}

static void test_si2151_type(void)
{
    struct fake f;
    reset(&f);
    oa_tuner_sequence_result r;
    assert(oa_tuner_sequence_run(&ops, &f, 0x8, &r) == 0);
    assert(f.n == 6 && f.set_type == 8 && r.tuner_type == 8);
}

static void test_external_type_skips_tuner(void)
{
    struct fake f;
    reset(&f);
    oa_tuner_sequence_result r;
    assert(oa_tuner_sequence_run(&ops, &f, 0xf, &r) == 0);
    assert(f.n == 2 && f.calls[0] == C_DEMOD && f.calls[1] == C_LNB);
    assert(r.dvif_handle == 0 && r.set_type_rc == 0 && r.tuner_type == 0);
    assert(r.lnb_called == 1 && r.lnb_rc == 1);
}

static void test_failures_continue(void)
{
    struct fake f;
    oa_tuner_sequence_result r;

    reset(&f);
    f.demod_rc = -1;
    assert(oa_tuner_sequence_run(&ops, &f, 0xd, &r) == -1);
    assert(f.n == 6 && r.demod_rc == -1 && f.calls[4] == C_SET);
    /* all later steps still ran */

    reset(&f);
    f.tuner_rc = -1;
    assert(oa_tuner_sequence_run(&ops, &f, 0xd, &r) == -1);
    assert(f.n == 6 && r.tuner_rc == -1 && f.calls[5] == C_LNB);

    reset(&f);
    f.dvif_handle = -1;
    assert(oa_tuner_sequence_run(&ops, &f, 0xd, &r) == -1);
    assert(f.n == 6 && f.set_handle == -1 && f.calls[4] == C_SET);

    reset(&f);
    f.set_rc = -5;
    assert(oa_tuner_sequence_run(&ops, &f, 0xd, &r) == -1);
    assert(f.n == 6 && r.set_type_rc == -5 && f.calls[5] == C_LNB);

    /* AV2018's return is recorded but ignored, like the vendor. */
    reset(&f);
    f.av_rc = 0;
    assert(oa_tuner_sequence_run(&ops, &f, 0xd, &r) == 0);
    assert(r.av2018_rc == 0 && f.calls[5] == C_LNB);

    /* LNB absence/failure is recorded but does not fail the sequence. */
    reset(&f);
    f.lnb_rc = 0;
    assert(oa_tuner_sequence_run(&ops, &f, 0xd, &r) == 0);
    assert(r.lnb_rc == 0 && f.calls[5] == C_LNB);
    reset(&f);
    f.lnb_rc = -1;
    assert(oa_tuner_sequence_run(&ops, &f, 0xd, &r) == 0);
    assert(r.lnb_rc == -1 && f.calls[5] == C_LNB);
}

static void test_no_lnb_and_bad_args(void)
{
    struct fake f;
    reset(&f);
    oa_tuner_sequence_ops no_lnb = ops;
    no_lnb.lnb_init = NULL;
    oa_tuner_sequence_result r;
    assert(oa_tuner_sequence_run(&no_lnb, &f, 0xd, &r) == 0);
    assert(f.n == 5 && r.lnb_called == 0);

    assert(oa_tuner_sequence_run(NULL, &f, 0xd, &r) == -1);
    assert(oa_tuner_sequence_run(&ops, &f, 0xd, NULL) == -1);
}

int main(void)
{
    test_mxl661();
    test_si2151_type();
    test_external_type_skips_tuner();
    test_failures_continue();
    test_no_lnb_and_bad_args();
    puts("ok tuner sequence: demod->tuner->AV2018->dvif->SetTunerType->LNB "
         "order, 0xd->0xc/8->8, type-0xf skip and failure continuation");
    return 0;
}
