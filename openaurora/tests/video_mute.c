/* Pure main-video mute test. No DRM, TV, vendor library or display. */
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/video_mute.h"

struct fake {
    char log[512];
    size_t n;
    int fd;
    int plane_type;
    uint32_t plane;
    int fail_step;  /* 1 dev, 2 path, 3 info, 4 mute */
    int rc;
};

static void rec(struct fake *f, const char *fmt, ...)
{
    if (f->n >= sizeof f->log)
        return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(f->log + f->n, sizeof f->log - f->n, fmt, ap);
    va_end(ap);
    if (n > 0)
        f->n += (size_t)n < sizeof f->log - f->n
                    ? (size_t)n : sizeof f->log - f->n - 1;
}

static int f_dev(void *ud, int index)
{
    struct fake *f = ud;
    rec(f, "dev(%d)|", index);
    return f->fail_step == 1 ? f->rc : f->fd;
}

static int f_path(void *ud, int path)
{
    struct fake *f = ud;
    rec(f, "path(%d)|", path);
    return f->fail_step == 2 ? f->rc : f->plane_type;
}

static int f_info(void *ud, int plane_type,
                  uint32_t info[OA_VIDEO_MUTE_INFO_WORDS])
{
    struct fake *f = ud;
    rec(f, "info(%d)|", plane_type);
    if (f->fail_step == 3)
        return f->rc;
    /* The real HAL writes exactly 12 words (48 bytes), not just info[0]. */
    for (int i = 0; i < OA_VIDEO_MUTE_INFO_WRITTEN_WORDS; i++)
        info[i] = (uint32_t)(0xa500u + i);
    info[0] = f->plane;
    return 0;
}

static int f_mute(void *ud, int fd, unsigned plane, int enable, int on)
{
    struct fake *f = ud;
    char msg[96];
    int n = snprintf(msg, sizeof msg, "mute(fd=%d,plane=%u,en=%d,on=%d)|",
                     fd, plane, enable, on);
    if (n > 0 && f->n + (size_t)n + 1 < sizeof f->log) {
        memcpy(f->log + f->n, msg, (size_t)n);
        f->n += (size_t)n;
        f->log[f->n] = '\0';
    }
    return f->fail_step == 4 ? f->rc : 0;
}

static oa_video_mute_io io_for(struct fake *f)
{
    oa_video_mute_io io = { f, f_dev, f_path, f_info, f_mute };
    return io;
}

static void test_hide_show_order(void)
{
    struct fake f;
    oa_video_mute m;
    oa_video_mute_state st;
    oa_video_mute_io io;

    memset(&f, 0, sizeof f);
    f.fd = 17;
    f.plane_type = 3;
    f.plane = 42;
    io = io_for(&f);
    oa_video_mute_init(&m, &io, OA_VIDEO_MUTE_PATH_MAIN);
    assert(oa_video_mute_snapshot(&m, &st) == 0 && st.hidden == -1);

    assert(oa_video_mute_set(&m, 1) == 0);
    assert(strcmp(f.log,
        "dev(0)|path(0)|info(3)|mute(fd=17,plane=42,en=1,on=1)|") == 0);
    assert(oa_video_mute_snapshot(&m, &st) == 0);
    assert(st.hidden == 1 && st.calls == 1 && st.last_rc == 0);
    assert(st.last_fd == 17 && st.last_plane == 42 && st.last_on == 1);

    f.n = 0; f.log[0] = '\0';
    assert(oa_video_mute_set(&m, 0) == 0);
    assert(strcmp(f.log,
        "dev(0)|path(0)|info(3)|mute(fd=17,plane=42,en=1,on=0)|") == 0);
    assert(oa_video_mute_snapshot(&m, &st) == 0);
    assert(st.hidden == 0 && st.calls == 2 && st.last_on == 0);
}

static void test_failures(void)
{
    struct fake f;
    oa_video_mute m;
    oa_video_mute_state st;
    oa_video_mute_io io;

    for (int step = 1; step <= 4; step++) {
        memset(&f, 0, sizeof f);
        f.fd = 17; f.plane_type = 3; f.plane = 42;
        f.fail_step = step; f.rc = -EIO;
        io = io_for(&f);
        oa_video_mute_init(&m, &io, OA_VIDEO_MUTE_PATH_MAIN);
        assert(oa_video_mute_set(&m, 1) == -EIO);
        assert(oa_video_mute_snapshot(&m, &st) == 0);
        assert(st.calls == 1 && st.last_rc == -EIO && st.hidden == -1);
    }

    /* Reject incomplete io and null args without any HAL call. */
    memset(&f, 0, sizeof f);
    io = io_for(&f);
    io.drm_set_mute = NULL;
    oa_video_mute_init(&m, &io, 0);
    assert(oa_video_mute_set(&m, 1) == -EINVAL);
    assert(f.n == 0);
    assert(oa_video_mute_set(NULL, 1) == -EINVAL);
    assert(oa_video_mute_snapshot(NULL, &st) == -EINVAL);
    assert(oa_video_mute_snapshot(&m, NULL) == -EINVAL);
}

static void test_custom_path(void)
{
    struct fake f;
    oa_video_mute m;
    oa_video_mute_io io;
    memset(&f, 0, sizeof f);
    f.fd = 9; f.plane_type = 1; f.plane = 7;
    io = io_for(&f);
    oa_video_mute_init(&m, &io, 2);
    assert(oa_video_mute_set(&m, 1) == 0);
    assert(strstr(f.log, "path(2)|") != NULL);

    /* The record table has 9 slots; the getter does not validate the index. */
    f.plane_type = OA_VIDEO_MUTE_PLANE_SLOTS;
    f.n = 0; f.log[0] = '\0';
    assert(oa_video_mute_set(&m, 1) == -ERANGE);
    assert(strstr(f.log, "info(") == NULL);
}

int main(void)
{
    test_hide_show_order();
    test_failures();
    test_custom_path();
    puts("ok video mute: recovered DRM chain order/args, hide/show state, "
         "HAL failures and rejection");
    return 0;
}
