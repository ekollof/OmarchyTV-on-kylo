#ifndef SCENE_GPU_TRACE_H
#define SCENE_GPU_TRACE_H
#include "openaurora/services.h"
#define SCENE_GPU_TRACE "test.scene_gpu"
typedef struct {
    int (*settle)(int error);
    int (*stats)(oa_display_result *last, unsigned *uploads, unsigned *frames);
    int (*pixel)(unsigned x, unsigned y, unsigned char rgba[4]);
} scene_gpu_trace;
#endif
