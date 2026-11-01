/* Private bootstrap/renderer DSO boundary, independent of the host module ABI.
 * Load only in the separate graphics process, after inherited-fd cleanup.
 * Keep the DSO loaded for the entire process: EGL close does not establish that
 * every vendor library background thread has stopped executing library code. */
#ifndef OPENAURORA_SCENE_RENDERER_H
#define OPENAURORA_SCENE_RENDERER_H
#include <stdint.h>
#include "openaurora/scene_broker.h"
#define OA_SCENE_RENDERER_ABI 1u
typedef struct {
    uint32_t abi, size;
    oa_scene_backend backend;
} oa_scene_renderer;
/* Exported data symbol, so dlsym needs no object/function pointer conversion. */
extern const oa_scene_renderer oa_scene_renderer_v1;
#endif
