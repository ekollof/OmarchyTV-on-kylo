/* Real GPU regression/benchmark; no native window, framebuffer, swaps or HAL.
 * Deliberately opt-in, never part of make check. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "openaurora/scene_gles.h"

static double millis(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) return 0;
    return (double)t.tv_sec * 1000 + (double)t.tv_nsec / 1000000;
}
static int report(oa_scene_gles *scene)
{
    char error[160];
    oa_scene_gles_error(scene, error, sizeof error);
    fprintf(stderr, "%s errno=%d\n", error, errno);
    return 1;
}
static int pixel_tests(oa_scene_gles *scene)
{
    unsigned char input[8*16*4], output[sizeof input];
    for (unsigned y = 0; y < 16; y++)
        for (unsigned x = 0; x < 8; x++) {
            size_t at = (y*8+x)*4;
            input[at] = (unsigned char)(x*16); input[at+1] = (unsigned char)(y*16);
            input[at+2] = 80; input[at+3] = 255;
        }
    for (int type = 0; type <= 8; type++) {
        if (oa_scene_gles_render(scene, input, sizeof input, type, 1, output, sizeof output))
            return report(scene);
        for (unsigned y = 0; y < 16; y++)
            if (memcmp(output + (15-y)*8*4, input + y*8*4, 8*4)) {
                fprintf(stderr, "identity/orientation mismatch type=%d row=%u\n", type, y); return 1;
            }
    }
    if (oa_scene_gles_render(scene, input, sizeof input, 1, 0.5f, output, sizeof output))
        return report(scene);
    unsigned upper = output[((15-4)*8+3)*4+1], lower = output[((15-5)*8+5)*4+1];
    printf("keystone filtered green upper=%u lower=%u; CPU golden27/33\n", upper, lower);
    if (upper < 25 || upper > 29 || lower < 31 || lower > 35) return 1;
    if (oa_scene_gles_render(scene, input, sizeof input, 6, 0, output, sizeof output))
        return report(scene);
    for (unsigned i = 0; i < sizeof output; i++) if (output[i]) return 1;
    if (oa_scene_gles_render(scene, input, sizeof input, 6, 0.5f, output, sizeof output))
        return report(scene);
    /* Top-row red0, green0, blue80 at half alpha -> premultiplied blue40. */
    const unsigned char *top = output + 15*8*4;
    if (top[0] || top[1] || top[2] < 39 || top[2] > 41 || top[3] < 127 || top[3] > 128) return 1;
    if (oa_scene_gles_render(scene, input, sizeof input-1, 6, 0.5f, output, sizeof output) != -1 ||
        errno != EINVAL) return 1;
    puts("small GPU identity/orientation, triangle/filter, fade and bounds checks passed");
    return 0;
}
int main(void)
{
    oa_scene_gles *scene = NULL;
    int result = 1;
    unsigned char *input = NULL, *output = NULL;
    if (oa_scene_gles_open(&scene, 8, 16)) { (void)report(scene); goto cleanup; }
    if (pixel_tests(scene)) goto cleanup;
    if (oa_scene_gles_close(&scene)) { (void)report(scene); goto cleanup; }
    const size_t bytes = 1920u*1080u*4u;
    input = malloc(bytes); output = malloc(bytes);
    if (!input || !output) goto cleanup;
    for (unsigned y = 0; y < 1080; y++)
        for (unsigned x = 0; x < 1920; x++) {
            size_t at = ((size_t)y*1920+x)*4;
            input[at] = (unsigned char)((x/240)*32); input[at+1] = (unsigned char)((y/135)*32);
            input[at+2] = 80; input[at+3] = 255;
        }
    if (oa_scene_gles_open(&scene, 1920, 1080)) { (void)report(scene); goto cleanup; }
    for (int type = 0; type <= 8; type++) {
        double began = millis();
        for (int step = 0; step <= 4; step++)
            if (oa_scene_gles_render(scene, input, bytes, type, (float)step/4, output, bytes)) {
                (void)report(scene); goto cleanup;
            }
        printf("GPU type=%d five uploads/renders/readbacks elapsed_ms=%.2f\n", type, millis()-began);
        fflush(stdout);
    }
    result = 0;
cleanup:
    if (oa_scene_gles_close(&scene)) { (void)report(scene); result = 1; }
    free(input); free(output);
    puts(result ? "GPU scene test failed" : "GPU scene/reopen/cleanup passed");
    return result;
}
