#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include "host.h"

static int image_w, image_h, image_held, image_freed;
/* Large enough for the old missing-axis-guard case too: its regression fails
 * by return value without deliberately reading outside a fixture allocation. */
static unsigned char image_pixels[8193u * 4u];
static int image_decode(const char *path, int *w, int *h, unsigned char **rgba)
{
    assert(path && image_held);
    *w = image_w; *h = image_h; *rgba = image_pixels;
    return 0;
}
static void image_release(void *rgba)
{
    assert(image_held && rgba == image_pixels);
    image_freed++;
}
static const oa_image_decode_iface image_iface = {
    image_decode, image_release, image_decode
};
static const void *image_lookup(void *priv, const char *name, uint32_t version)
{
    (void)priv;
    assert(!strcmp(name, OA_SVC_IMAGE_DECODE) && version == 2 && !image_held);
    image_held = 1;
    return &image_iface;
}
static void image_unhold(void *priv, const char *name)
{
    (void)priv;
    assert(image_held && !strcmp(name, OA_SVC_IMAGE_DECODE));
    image_held = 0;
}
static void image_bounds(oa_host *h)
{
    oa_host_api saved = h->api;
    h->api.lookup = image_lookup;
    h->api.release = image_unhold;
    const int sizes[][2] = {
        {8193, 1}, {1, 8193}, {8192, 2049}, {INT_MAX, INT_MAX}, {0, 1}, {1, 1}
    };
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        image_w = sizes[i][0]; image_h = sizes[i][1]; image_freed = 0;
        char line[] = "image-decode fixture", reply[128], expected[128];
        int rc = oa_control_command(h, line, reply, sizeof reply);
        if (i < 5) {
            assert(rc == -1);
            snprintf(expected, sizeof expected, "failed errno %d\n",
                     i < 4 ? EOVERFLOW : EINVAL);
            assert(!strcmp(reply, expected));
        } else {
            assert(!rc && !strncmp(reply, "image 1x1 fnv ", 14));
        }
        assert(image_freed == 1 && !image_held);
    }
    h->api = saved;
    puts("  ok   image CLI bounds provider axes/bytes before hashing and releases under hold");
}

static void query(oa_host *h, const char *text, size_t cap, const char *expected)
{
    unsigned char *guarded = malloc(cap + 2);
    char command[128];
    assert(guarded);
    memset(guarded, 0xa5, cap + 2);
    snprintf(command, sizeof command, "%s", text);
    assert(oa_control_command(h, command, (char *)guarded + 1, cap) == 0);
    assert(guarded[0] == 0xa5 && guarded[cap + 1] == 0xa5);
    if (cap) {
        assert(memchr(guarded + 1, 0, cap));
        if (expected)
            assert(!strcmp((char *)guarded + 1, expected));
    }
    free(guarded);
}

static void finish_server(void *v)
{
    oa_host *h = v;
    /* These table entries exercise serialization only, not real DSO teardown. */
    memset(h->mods, 0, sizeof h->mods);
    oa_host_quit(h);
}

static void socket_replies(oa_host *h)
{
    char dir[] = "/tmp/oa-control-XXXXXX", path[108];
    assert(mkdtemp(dir));
    snprintf(path, sizeof path, "%s/control.sock", dir);
    char commands[][32] = { "list\n", "status module_00\n" };
    char expected[2][16384];
    for (unsigned i = 0; i < 2; i++) {
        char text[32];
        strcpy(text, commands[i]);
        assert(oa_control_command(h, text, expected[i], sizeof expected[i]) == 0);
        assert(strlen(expected[i]) > 2048);
    }
    assert(strstr(expected[0], "module_31"));
    assert(strstr(expected[1], "47:v4294967295"));
    assert(oa_control_start(h, path) == 0);
    assert(h->api.timer_add(h, 1000, 0, finish_server, h));
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        oa_host_run(h);
        _exit(0);
    }
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    assert(fd >= 0);
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strcpy(addr.sun_path, path);
    assert(connect(fd, (struct sockaddr *)&addr, sizeof addr) == 0);
    oa_control_stop(h); /* child retains listener and the connected client */
    for (unsigned i = 0; i < 2; i++) {
        size_t len = strlen(expected[i]), received = 0;
        char actual[16384];
        assert(write(fd, commands[i], strlen(commands[i])) == (ssize_t)strlen(commands[i]));
        while (received < len) {
            ssize_t n = read(fd, actual + received, len - received);
            assert(n > 0);
            received += (size_t)n;
        }
        assert(!memcmp(actual, expected[i], len));
    }
    close(fd);
    int status;
    assert(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    assert(access(path, F_OK) != 0 && rmdir(dir) == 0);
    puts("  ok   full list/status replies reach a Unix socket client intact");
}

int main(void)
{
    alarm(10);
    oa_host *h = calloc(1, sizeof *h);
    assert(h);
    assert(oa_host_init(h, ".", NULL) == 0);
    image_bounds(h);
    static const char long_name[] = "dependency_or_service_with_a_long_distinct_name";
    oa_module desc = { .version = "test" };
    for (int i = 0; i < OA_REQUIRES_MAX; i++) desc.requires[i] = long_name;
    for (int i = 0; i < MAX_MODULES; i++) {
        oa_mod *m = &h->mods[i];
        snprintf(m->name, sizeof m->name, "module_%02d", i);
        memset(m->path, 'p', sizeof m->path - 1);
        m->state = OA_ST_STARTED;
        m->id = (uint32_t)i + 1;
        m->desc = &desc;
    }
    for (int i = 0; i < MAX_SVCS; i++) {
        h->svcs[i].used = 1;
        h->svcs[i].owner = 1;
        h->svcs[i].version = UINT32_MAX;
        snprintf(h->svcs[i].name, sizeof h->svcs[i].name, "%.36s%02u", long_name, (unsigned)i);
    }
    /* The old socket dispatcher supplied 2048 bytes; the complete status is
     * larger. Also exercise callers supplying tiny or empty response buffers. */
    const size_t sizes[] = { 2048, 1, 0, 2, 16, 127, 4096 };
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        query(h, "status module_00", sizes[i], NULL);
        query(h, "list", sizes[i], NULL);
        query(h, "help", sizes[i], NULL);
    }
    query(h, "status module_01", 4096, NULL);
    query(h, "   ", 16, "");
    socket_replies(h);
    memset(h->mods, 0, sizeof h->mods);
    query(h, "list", 4096, "(no modules loaded)\n");
    close(h->selfpipe[0]); close(h->selfpipe[1]);
    pthread_mutex_destroy(&h->post_mu);
    free(h);
    puts("  ok   control replies stay bounded for full tables and tiny buffers");
    return 0;
}
