/* Pure HDMI RX status decoder and poll-seam test. No /dev, TV, V4L2 or HAL. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/source_vin.h"

struct fake {
    uint32_t words[OA_SOURCE_VIN_WORDS];
    int reads;
};

static int fake_read(void *context, int fd,
                     uint32_t words[OA_SOURCE_VIN_WORDS])
{
    struct fake *f = context;
    assert(fd == 105);
    f->reads++;
    memcpy(words, f->words, sizeof f->words);
    return 0;
}

int main(void)
{
    const uint32_t ready[OA_SOURCE_VIN_WORDS] = { 1, 1, 1, 0, 0 };
    const uint32_t sleep0[OA_SOURCE_VIN_WORDS] = { 0, 1, 1, 0, 0xffffffffu };
    const uint32_t sleep5[OA_SOURCE_VIN_WORDS] = { 5, 0, 0, 0, 0xffffffffu };

    assert(oa_source_vin_ready(ready) == 1);
    assert(oa_source_vin_ready(sleep0) == 0);
    assert(oa_source_vin_ready(sleep5) == 0);
    assert(oa_source_vin_ready(NULL) == -EINVAL);

    struct fake f;
    memset(&f, 0, sizeof f);
    memcpy(f.words, ready, sizeof f.words);
    struct oa_source_vin_io io;
    memset(&io, 0, sizeof io);
    io.context = &f;
    io.read = fake_read;

    uint32_t words[OA_SOURCE_VIN_WORDS];
    memset(words, 0xa5, sizeof words);
    assert(oa_source_vin_poll(&io, 105, words) == 1);
    assert(f.reads == 1);
    assert(memcmp(words, ready, sizeof words) == 0);

    memcpy(f.words, sleep5, sizeof f.words);
    assert(oa_source_vin_poll(&io, 105, words) == 0);
    assert(f.reads == 2);
    assert(words[0] == 5 && words[4] == 0xffffffffu);

    assert(oa_source_vin_poll(NULL, 105, words) == -EINVAL);
    assert(oa_source_vin_poll(&io, -1, words) == -EINVAL);
    assert(oa_source_vin_poll(&io, 105, NULL) == -EINVAL);
    io.read = NULL;
    assert(oa_source_vin_poll(&io, 105, words) == -EINVAL);

    /* PLAT_VIU_NdVideoSetting @0x00901c28: path 0..6 -> slot, path > 6 ->
     * slot 0; ChangeInput @0x00902d60 stores the selected source type there.
     * The following GetVinParam/TDLayer copy is skipped on this firmware
     * (all table pointers zero), so no 13-word state is emulated. */
    struct oa_source_vin_paths paths;
    memset(&paths, 0xa5, sizeof paths);
    oa_source_vin_paths_init(&paths);
    for (int i = 0; i < (int)OA_SOURCE_VIN_PATHS; i++)
        assert(oa_source_vin_path_get(&paths, i) == 0);
    assert(oa_source_vin_path_set(&paths, 0, 17) == 0);
    assert(oa_source_vin_path_set(&paths, 6, 19) == 0);
    assert(oa_source_vin_path_get(&paths, 0) == 17 &&
           oa_source_vin_path_get(&paths, 6) == 19);
    assert(oa_source_vin_path_get(&paths, 1) == 0);   /* slots independent */
    assert(oa_source_vin_path_set(&paths, 7, 5) == 0);
    assert(oa_source_vin_path_get(&paths, 0) == 5);   /* clamp to slot 0 */
    assert(oa_source_vin_path_get(&paths, -1) == 5);
    assert(oa_source_vin_path_set(&paths, 0, -1) == -EINVAL);
    assert(oa_source_vin_path_get(NULL, 0) == -EINVAL);
    assert(oa_source_vin_path_set(NULL, 0, 1) == -EINVAL);

    puts("ok source-vin: w0=1 ready, w0=0/5 lost, live sleep samples, "
         "poll seam, null/fd rejection, 7-slot NdVideo path table");
    return 0;
}
