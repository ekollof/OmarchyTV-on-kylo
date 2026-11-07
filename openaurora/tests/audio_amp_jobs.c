/* audio_amp result handling: mute/balance are owner jobs; a later job must not
 * overwrite the retained Init result, caches change only on success, and the
 * one-slot backpressure still prevents overtaking a pending Init. */
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "openaurora/module.h"

#include "../modules/audio_amp/audio_amp.c"

static const oa_amp_gpio mute_record = { 208, 1, 0 };

static struct {
    pthread_mutex_t mu;
    pthread_cond_t cv;
    pthread_t main_thread;
    unsigned preludes, levels, writes;
    int block, released, level_fail, fail_write_reg;
} hw = {
    .mu = PTHREAD_MUTEX_INITIALIZER,
    .cv = PTHREAD_COND_INITIALIZER,
    .fail_write_reg = -1
};

static void on_worker(void)
{
    assert(!pthread_equal(pthread_self(), hw.main_thread));
}

static void prelude(void *v)
{
    assert(v == &hw);
    on_worker();
    pthread_mutex_lock(&hw.mu);
    hw.preludes++;
    pthread_cond_broadcast(&hw.cv);
    while (hw.block && !hw.released)
        pthread_cond_wait(&hw.cv, &hw.mu);
    pthread_mutex_unlock(&hw.mu);
}

static const oa_amp_gpio *gpio(void *v, unsigned function)
{
    assert(v == &hw && function < 2);
    return function == 1 ? &mute_record : NULL;
}

static void direction(void *v, uint32_t pin, uint32_t dir)
{
    (void)pin;
    (void)dir;
    assert(v == &hw);
    on_worker();
}

static int level(void *v, uint32_t pin, uint32_t value)
{
    (void)pin;
    (void)value;
    assert(v == &hw);
    on_worker();
    pthread_mutex_lock(&hw.mu);
    hw.levels++;
    pthread_cond_broadcast(&hw.cv);
    pthread_mutex_unlock(&hw.mu);
    return hw.level_fail ? 0 : 1;
}

static void delay_ms(void *v, unsigned ms)
{
    (void)ms;
    assert(v == &hw);
    on_worker();
}

static int write_reg(void *v, unsigned reg, const uint8_t *value)
{
    (void)value;
    assert(v == &hw && value);
    on_worker();
    pthread_mutex_lock(&hw.mu);
    hw.writes++;
    pthread_cond_broadcast(&hw.cv);
    pthread_mutex_unlock(&hw.mu);
    return reg != (unsigned)hw.fail_write_reg;
}

static uint32_t sub_enabled(void *v) { assert(v == &hw); on_worker(); return 0; }
static void sub_enable(void *v, int on) { (void)on; assert(v == &hw); on_worker(); }
static void sub_init(void *v) { assert(v == &hw); on_worker(); }

static void fake_log(void *priv, int level_, const char *fmt, ...)
{
    (void)priv;
    (void)level_;
    (void)fmt;
}
static int fake_publish(void *priv, const char *name, uint32_t version,
                        const void *iface)
{
    (void)priv;
    (void)name;
    (void)version;
    (void)iface;
    return 0;
}
static int fake_unpublish(void *priv, const char *name)
{
    (void)priv;
    (void)name;
    return 0;
}
static const char *fake_config(void *priv, const char *key, const char *def)
{
    (void)priv;
    (void)key;
    return def;
}

static const oa_host_api g_api = {
    .size = sizeof g_api,
    .abi = OA_ABI_VERSION,
    .log = fake_log,
    .publish = fake_publish,
    .unpublish = fake_unpublish,
    .config_get = fake_config,
};

static void wait_prelude(void)
{
    struct timespec until;
    assert(!clock_gettime(CLOCK_REALTIME, &until));
    until.tv_sec += 5;
    pthread_mutex_lock(&hw.mu);
    while (hw.preludes < 1)
        assert(!pthread_cond_timedwait(&hw.cv, &hw.mu, &until));
    pthread_mutex_unlock(&hw.mu);
}

static void release_worker(void)
{
    pthread_mutex_lock(&hw.mu);
    hw.released = 1;
    pthread_cond_broadcast(&hw.cv);
    pthread_mutex_unlock(&hw.mu);
}

static oa_audio_amp_status wait_idle(void)
{
    oa_audio_amp_status st;
    for (int i = 0; i < 5000; i++) {
        assert(!iface_state(&st));
        if (!st.busy)
            return st;
        const struct timespec pause = { .tv_nsec = 1000000 };
        (void)nanosleep(&pause, NULL);
    }
    assert(0 && "amp job did not complete");
    return st;
}

static struct ctx *make_ctx(void)
{
    void *opaque = NULL;
    assert(m_init(&g_api, &opaque) == 0);
    struct ctx *c = opaque;
    c->published = 1;
    c->enabled = 1;
    c->init_io = (oa_amp_init_io){ &hw, prelude, gpio, direction, level,
        delay_ms, write_reg, sub_enabled, sub_enable, sub_init };
    snprintf(c->power, sizeof c->power, "6Watt");
    c->profile = (oa_ad82587_profile){ .speaker_power = c->power,
        .prescale = 0x11, .state_control4 = 0x42,
        .limiter_attack = -1, .release_threshold = -1 };

    oa_amp_init_state initial;
    memset(&initial, 0, sizeof initial);
    initial.reset_cached = 7;
    initial.mute_cached = 9;
    for (unsigned i = 0; i < OA_AD82587_REGISTERS; i++)
        initial.image[i] = (uint8_t)(0x40 + i);
    assert(!oa_amp_owner_create(&c->owner, &initial, &c->init_io));
    return c;
}

static void test_failed_init_retry(void)
{
    struct ctx *c = make_ctx();
    hw.block = 0;
    hw.level_fail = 0;
    hw.fail_write_reg = 3;
    assert(iface_init() == 0);
    oa_audio_amp_status st = wait_idle();
    assert(st.have_result == 1 && st.native_result == 0);

    hw.fail_write_reg = -1;
    assert(iface_init() == 0); /* a failed Init can be retried */
    st = wait_idle();
    assert(st.have_result == 1 && st.native_result == 1);
    assert(iface_init() == -EBUSY); /* success stays final for this generation */

    int result = 0;
    finish_owner(c->owner, &result);
    assert(result == 0);
    c->owner = NULL;
    m_destroy(c);
    puts("ok audio_amp lifecycle: failed Init retries, successful Init stays final");
}

static void test_suspend_resume(void)
{
    const oa_module *mod = oa_module_entry();
    assert(mod->suspend && mod->resume);

    struct ctx *c = make_ctx();
    assert(mod->suspend(c) == 0);
    assert(!c->published && c->owner == NULL && c->i2c == NULL);
    m_destroy(c);

    void *opaque = NULL;
    assert(m_init(&g_api, &opaque) == 0);
    c = opaque;
    c->published = 1;
    c->enabled = 0;
    assert(mod->suspend(c) == 0 && !c->published);
    assert(mod->resume(c) == 0 && c->published);
    m_destroy(c);
    puts("ok audio_amp lifecycle: suspend releases the owner and resume republishes");
}

int main(void)
{
    hw.main_thread = pthread_self();
    struct ctx *c = make_ctx();

    /* Pending Init blocks both controls; after completion the Init result is
     * retained and the Init's trailing Mute(1) is reflected. */
    hw.block = 1;
    assert(iface_init() == 0);
    wait_prelude();
    assert(iface_set_mute(1) == -EBUSY);
    assert(iface_set_balance(50) == -EBUSY);
    release_worker();
    oa_audio_amp_status st = wait_idle();
    assert(st.have_result == 1 && st.native_result == 1);
    assert(st.mute_known == 1 && st.mute == 1 && st.balance_known == 0);
    uint64_t init_ticket = st.ticket;
    puts("ok audio_amp jobs: pending Init blocks mute/balance; Init state is retained");

    /* A later mute updates only the mute cache, not the Init result. */
    hw.block = 0;
    assert(iface_set_mute(0) == 0);
    assert(iface_set_balance(50) == -EBUSY); /* one ordered slot */
    st = wait_idle();
    assert(st.mute_known == 1 && st.mute == 0);
    assert(st.have_result == 1 && st.ticket == init_ticket &&
           st.native_result == 1);
    assert(st.balance_known == 0);
    puts("ok audio_amp jobs: mute result does not overwrite the Init result");

    /* Balance updates only the balance cache. */
    assert(iface_set_balance(25) == 0);
    st = wait_idle();
    assert(st.balance_known == 1 && st.balance == 25);
    assert(st.mute == 0 && st.have_result == 1 && st.ticket == init_ticket);
    puts("ok audio_amp jobs: balance result keeps mute and Init state");

    /* Failed jobs leave the cached values unchanged. */
    hw.level_fail = 1;
    assert(iface_set_mute(1) == 0);
    st = wait_idle();
    assert(st.mute == 0 && st.mute_known == 1);
    hw.level_fail = 0;
    hw.fail_write_reg = 5;
    assert(iface_set_balance(50) == 0);
    st = wait_idle();
    assert(st.balance == 25 && st.balance_known == 1);
    hw.fail_write_reg = -1;
    assert(iface_set_mute(2) == -EINVAL);
    assert(iface_set_balance(101) == -EINVAL);
    puts("ok audio_amp jobs: failed mute/balance report no cache success");

    int result = 0;
    finish_owner(c->owner, &result);
    assert(result == 0);
    c->owner = NULL;
    m_destroy(c);

    test_failed_init_retry();
    test_suspend_resume();
    puts("all audio_amp job tests passed");
    return 0;
}
