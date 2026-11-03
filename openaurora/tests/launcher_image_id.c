#include "openaurora/launcher_image_id.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    /* Representative entries from the shipped 62-way switch: the
     * state==1 alternate, the shared blocks, the -1 defaults and the
     * state-independent entries. */
    assert(oa_launcher_get_image_id(0,1)==729);
    assert(oa_launcher_get_image_id(0,0)==730);
    assert(oa_launcher_get_image_id(0,2)==730);
    assert(oa_launcher_get_image_id(26,1)==805);
    assert(oa_launcher_get_image_id(26,0)==806);
    assert(oa_launcher_get_image_id(32,1)==799);
    assert(oa_launcher_get_image_id(50,1)==799);
    assert(oa_launcher_get_image_id(30,1)==-1);
    assert(oa_launcher_get_image_id(31,0)==-1);
    assert(oa_launcher_get_image_id(36,2)==-1);
    assert(oa_launcher_get_image_id(37,1)==-1);
    assert(oa_launcher_get_image_id(42,0)==167);
    assert(oa_launcher_get_image_id(42,1)==167);
    assert(oa_launcher_get_image_id(52,1)==825);
    assert(oa_launcher_get_image_id(52,0)==790);
    assert(oa_launcher_get_image_id(56,1)==847);
    assert(oa_launcher_get_image_id(56,0)==847);
    assert(oa_launcher_get_image_id(61,1)==836);
    assert(oa_launcher_get_image_id(61,0)==837);
    /* Unknown operations take the -1 default. */
    assert(oa_launcher_get_image_id(62,0)==-1);
    assert(oa_launcher_get_image_id(100,1)==-1);
    assert(oa_launcher_get_image_id(UINT32_MAX,1)==-1);
    /* Only state==1 selects the alternate id; every other value is equal. */
    for(uint32_t op=0;op<62;++op) {
        int32_t base=oa_launcher_get_image_id(op,0);
        assert(oa_launcher_get_image_id(op,2)==base);
        assert(oa_launcher_get_image_id(op,3)==base);
        assert(oa_launcher_get_image_id(op,UINT32_MAX)==base);
        int32_t alt=oa_launcher_get_image_id(op,1);
        int unmapped=(op==30||op==31||op==36||op==37);
        if(unmapped) {
            assert(alt==-1 && base==-1);
        } else {
            assert(alt>=1 && alt<=UINT16_MAX);
            assert(base>=1 && base<=UINT16_MAX);
        }
    }
    puts("ok launcher image id: 62-way mapping, state split and default");
    return 0;
}
