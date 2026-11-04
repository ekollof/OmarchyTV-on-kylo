#include "openaurora/settings_row_construct.h"
#include <string.h>

/*
 * Default fields written inline by the shipped constructor after all child
 * objects exist. Order follows the body; the mirror branch rewrites
 * 0x2e4c/0x2e50 to 120/119 when the OSD horizontal mirror is active.
 */
const uint32_t oa_setting_row_field_offsets[OA_ROW_FIELD_COUNT]={
    0x2dd4,0x2de8,0x2dec,0x2df0,0x2df4,0x2df8,0x2dfc,
    0x2e45,0x2e46,0x2e47,0x2e48,0x2e49,
    0x2e4c,0x2e50,0x2e54,0x2e58,0x2e5c,0x2e60,0x2e68,
    0x2e6c,0x2e70,0x2e74,0x2e78,0x2e7c,0x2e80,0x2e84,0x2e88,
    0x2e8c,0x2e90,0x2e94,0x2e98,
    0x2e9c,0x2ea0,0x2ea4,0x2ea8,
    0x2eac,0x2eb0,0x2eb4,0x2eb8,
    0x2ebc,0x2ec0,0x2ec4,0x2ec8,
    0x2ecc,0x2ed0,0x2ed4,0x2ed8
};

static void store32(void *row,uint32_t offset,uint32_t value)
{ memcpy((unsigned char *)row+offset,&value,4); }
static void store8(void *row,uint32_t offset,uint8_t value)
{ ((unsigned char *)row)[offset]=value; }

void oa_setting_row_construct(void *row,void *framework,void *context,
    const struct oa_setting_row_construct_ops *o)
{
    static const struct { uint32_t offset; int kind; } children[]={
        {0x104,1},{0x3c4,1},{0x684,2},{0x1014,3},{0x13c0,4},{0x1498,4},
        {0x1570,5},{0x1724,5},{0x18d8,6},{0x1ca8,1},{0x1f68,1},{0x2228,1},
        {0x24e8,1},{0x27a8,7},{0x2b28,8},{0x2c0c,8},{0x2cf0,8}
    };
    static const uint32_t ustrings[]={
        0x2dd8,0x2de0,0x2e04,0x2e0c,0x2e14,0x2e1c,0x2e24,0x2e2c,0x2e34,0x2e3c
    };
    if(!row || !o)return;
    o->construct_base(row,framework,context);
    for(unsigned i=0;i<sizeof children/sizeof children[0];++i) {
        void *child=(unsigned char *)row+children[i].offset;
        switch(children[i].kind) {
        case 1: o->construct_textbox(child,framework,context); break;
        case 2: o->construct_button(child,framework,context); break;
        case 3: o->construct_selection_box(child,framework,context); break;
        case 4: o->construct_slide_bar(child,framework,context); break;
        case 5: o->construct_progress_bar(child,framework,context); break;
        case 6: o->construct_text_entry_box(child,framework,context); break;
        case 7: o->construct_check_box(child,framework,context); break;
        default: o->construct_image(child,framework,context); break;
        }
    }
    for(unsigned i=0;i<sizeof ustrings/sizeof ustrings[0];++i) {
        void *slot=(unsigned char *)row+ustrings[i];
        /* 0x2e24 uses UString's default constructor in the shipped body. */
        o->construct_ustring(slot,ustrings[i]==0x2e24?NULL:"");
    }
    store32(row,0x2dd4,0);
    store32(row,0x2de8,0);
    store32(row,0x2dec,27);
    store32(row,0x2df0,UINT32_C(0xffffffff));
    store32(row,0x2df4,1);
    store32(row,0x2df8,0);
    store32(row,0x2dfc,0);
    store8(row,0x2e45,0);
    store8(row,0x2e46,0);
    store8(row,0x2e47,0);
    store8(row,0x2e48,0);
    store8(row,0x2e49,1);
    store32(row,0x2e4c,0);
    store32(row,0x2e50,0);
    store32(row,0x2e54,0);
    store32(row,0x2e58,UINT32_C(0xffffffff));
    store32(row,0x2e5c,0);
    store32(row,0x2e60,0);
    store32(row,0x2e68,UINT32_C(0x234));
    {
        void *control=o->control_interface(framework);
        if(o->horizontal_mirror(control)) {
            store32(row,0x2e4c,120);
            store32(row,0x2e50,119);
        }
    }
    store32(row,0x2e6c,UINT32_C(0x1f1));
    store32(row,0x2e70,UINT32_C(0x1f0));
    store32(row,0x2e74,UINT32_C(0x1f3));
    store32(row,0x2e78,UINT32_C(0x1f2));
    store32(row,0x2e7c,UINT32_C(0x1f1));
    store32(row,0x2e80,UINT32_C(0x1f0));
    store32(row,0x2e84,UINT32_C(0x1f3));
    store32(row,0x2e88,UINT32_C(0x1f2));
    store32(row,0x2e8c,UINT32_C(0x1e9));
    store32(row,0x2e90,UINT32_C(0x1e8));
    store32(row,0x2e94,UINT32_C(0x1eb));
    store32(row,0x2e98,UINT32_C(0x1ea));
    store32(row,0x2e9c,UINT32_C(0x1fa));
    store32(row,0x2ea0,UINT32_C(0x1f8));
    store32(row,0x2ea4,UINT32_C(0x1fb));
    store32(row,0x2ea8,UINT32_C(0x1f9));
    store32(row,0x2eac,UINT32_C(0x1f1));
    store32(row,0x2eb0,UINT32_C(0x1f0));
    store32(row,0x2eb4,UINT32_C(0x1f3));
    store32(row,0x2eb8,UINT32_C(0x1f2));
    store32(row,0x2ec0,UINT32_C(0x1fc));
    store32(row,0x2ebc,UINT32_C(0x1fe));
    store32(row,0x2ec8,UINT32_C(0x1fd));
    store32(row,0x2ec4,UINT32_C(0x1ff));
    store32(row,0x2ed8,UINT32_C(0x1f6));
    store32(row,0x2ecc,UINT32_C(0x1f5));
    store32(row,0x2ed0,UINT32_C(0x1f4));
    store32(row,0x2ed4,UINT32_C(0x1f7));
}
