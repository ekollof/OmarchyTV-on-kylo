#ifndef OA_TESTS_LAUNCHER_IMAGE_ID_NATIVE_H
#define OA_TESTS_LAUNCHER_IMAGE_ID_NATIVE_H

#include "openaurora/launcher_image_id.h"

/*
 * Differential for LauncherHelper::getImageId (0xe4aaf4): the shipped
 * 62-way tbh switch is copied verbatim into a fixed mapping and called for
 * every operation id (plus out-of-range values) across several widget
 * states. The only literal it reads is the stack guard at 0x1791d68, which
 * is backed by an owned page with a fixed canary so the check succeeds.
 */
typedef int32_t (*li_fn)(uint32_t,uint32_t);

static void check_launcher_image_id(const unsigned char *elf,size_t bytes)
{
    unsigned char *body=page(0xe4a000,4096),*guard=page(0x1791000,4096);
    size_t off=file_offset(elf,bytes,0xe4aaf4,792);
    assert(crc32(elf+off,792)==UINT32_C(0xa4caa578));
    memcpy(stub_destination(body,0xe4a000,0xe4aaf4,792),elf+off,792);
    { uint32_t canary=UINT32_C(0x4f1bbcdc);memcpy(guard+0xd68,&canary,4); }
    executable(body,4096);
    li_fn fn;{ uint32_t entry=0xe4aaf5;memcpy(&fn,&entry,4); }
    static const uint32_t ops[]={
        0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,
        26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,
        49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,70,100,UINT32_MAX};
    static const uint32_t states[]={0,1,2,3,4,100,UINT32_MAX};
    unsigned compared=0;
    for(unsigned i=0;i<sizeof ops/sizeof ops[0];++i)
        for(unsigned j=0;j<sizeof states/sizeof states[0];++j) {
            int32_t native=fn(ops[i],states[j]);
            int32_t ours=oa_launcher_get_image_id(ops[i],states[j]);
            if(native!=ours) {
                fprintf(stderr,"getImageId op=%u state=%u native=%d ours=%d\n",
                    (unsigned)ops[i],(unsigned)states[j],(int)native,(int)ours);
                exit(1);
            }
            ++compared;
        }
    assert(!munmap(body,4096) && !munmap(guard,4096));
    printf("ok offline shipped ARM launcher image id: %u native/C comparisons, 62-way switch and default match\n",compared);
}

#endif
