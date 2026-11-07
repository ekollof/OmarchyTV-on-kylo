/* Temporary CEC stale-owner close probe.
 *
 * Aurora never calls its exported PLAT_CEC_Close and ICE_CECClose is a stub,
 * so SIGTERM leaves the kernel DRV_CEC_CALLBACK thread + exclusive semaphore
 * alive. This probe loads libNDAL_EX, opens the platform video device used by
 * the NDAL globals (NDAL_VideoExt_OpenDevice(0,10)), then calls the linkable
 * close primitive NDAL_VideoExt_CEC_Init(NULL). It is a one-shot diagnostic,
 * not a production helper; run only with stock stopped.
 */
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    (void)dlopen("libasound.so.2", RTLD_NOW | RTLD_GLOBAL);
    if (!dlopen("libv4l2.so.0", RTLD_NOW | RTLD_GLOBAL)) {
        fprintf(stderr, "libv4l2: %s\n", dlerror());
        return 1;
    }
    void *lib = dlopen("libNDAL_EX.so", RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        fprintf(stderr, "libNDAL_EX: %s\n", dlerror());
        return 1;
    }
    int (*open_device)(int, int) = NULL;
    int (*cec_init)(void *) = NULL;
    void *p = dlsym(lib, "NDAL_VideoExt_OpenDevice");
    if (p) memcpy(&open_device, &p, sizeof p);
    p = dlsym(lib, "NDAL_VideoExt_CEC_Init");
    if (p) memcpy(&cec_init, &p, sizeof p);
    if (!open_device || !cec_init) {
        fprintf(stderr, "NDAL symbols missing\n");
        return 1;
    }
    int open_rc = open_device(0, 10);
    int close_rc = cec_init(NULL);
    printf("open_device(0,10)=%d cec_init(NULL)=%d\n", open_rc, close_rc);
    return 0;
}
