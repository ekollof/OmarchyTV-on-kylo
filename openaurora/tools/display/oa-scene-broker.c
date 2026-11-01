/* This bootstrap must NOT link EGL/Mali. Parent redirects stdout/stderr before
 * exec because the subsequently loaded vendor library has constructor logging.
 * stdin is a connected private AF_UNIX/SOCK_SEQPACKET. */
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "openaurora/scene_renderer.h"
static int close_inherited(void)
{
    DIR *dir = opendir("/proc/self/fd");
    if (!dir) return -1;
    int primary = 0;
    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(dir);
        if (!entry) { if (errno) primary = errno; break; }
        char *end; long fd = strtol(entry->d_name, &end, 10);
        if (*end || fd < 3 || fd > INT_MAX || fd == dirfd(dir)) continue;
        if (close((int)fd) && errno != EBADF) { primary = errno; break; }
    }
    if (closedir(dir) && !primary) primary = errno;
    if (primary) { errno = primary; return -1; }
    return 0;
}
int main(int argc, char **argv)
{
    if (argc != 3 || strcmp(argv[1], "--serve") || argv[2][0] != '/') {
        fprintf(stderr, "usage: %s --serve /absolute/renderer.so (seqpacket stdin)\n", argv[0]);
        return 2;
    }
    int type; socklen_t size = sizeof type;
    struct sockaddr_storage peer; socklen_t peer_size = sizeof peer;
    if (getsockopt(STDIN_FILENO, SOL_SOCKET, SO_TYPE, &type, &size) ||
        getpeername(STDIN_FILENO, (struct sockaddr *)&peer, &peer_size) ||
        type != SOCK_SEQPACKET || peer.ss_family != AF_UNIX) return 1;
    if (close_inherited() || setenv("FRAMEBUFFER_2", "/dev/fb1", 1) ||
        setenv("MALI_MAU_SWITCH", "0,0,0,0,0", 1)) return 1;
    void *library = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
    if (!library) { fprintf(stderr, "renderer load: %s\n", dlerror()); return 1; }
    const oa_scene_renderer *renderer = dlsym(library, "oa_scene_renderer_v1");
    if (!renderer || renderer->abi != OA_SCENE_RENDERER_ABI || renderer->size != sizeof *renderer) {
        fputs("renderer ABI mismatch\n", stderr); return 1;
    }
    int result = oa_scene_broker_serve(STDIN_FILENO, &renderer->backend);
    /* Deliberately no dlclose: this process owns vendor library lifetime even
     * after all explicit EGL handles drain. The parent must waitpid this child. */
    if (result) perror("scene broker");
    return result ? 1 : 0;
}
