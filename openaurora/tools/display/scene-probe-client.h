/* Blocking diagnostic client only. Never call from the host loop/quiesce. */
#ifndef SCENE_PROBE_CLIENT_H
#define SCENE_PROBE_CLIENT_H
#include <stddef.h>
#include <stdint.h>
typedef struct scene_probe scene_probe;
int probe_open(scene_probe **scene, unsigned width, unsigned height, const char *helper, const char *renderer);
int probe_render(scene_probe *scene, const uint8_t *rgba, size_t size, int type, float progress);
int probe_close(scene_probe **scene);
void probe_error(const scene_probe *scene, char *out, size_t size);
#endif
