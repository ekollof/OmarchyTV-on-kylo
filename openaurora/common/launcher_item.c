#include "openaurora/launcher_item.h"
#include <string.h>

/*
 * LauncherItem::LauncherItem(NovoLauncherItem const&) at 0xe4794c.
 *
 * Offsets are the shipped body's:17 scalar copies and10 source UString slots.
 * Seven strings are copied; the3 advertising slots deliberately construct
 * empty. The default image-path UString is at+0x2c, the shared_ptr pair at
 * +0x8c/+0x90 with a refcount increment on the control block at +4, the
 * -1/0 initialisers, and the empty-thumbnail default image path.
 */

static uint32_t get32(const void *p,uint32_t off)
{
    uint32_t v;
    memcpy(&v,(const unsigned char *)p+off,4);
    return v;
}

static void put32(void *p,uint32_t off,uint32_t v)
{
    memcpy((unsigned char *)p+off,&v,4);
}

static unsigned char byte_at(const void *p,uint32_t off)
{
    return ((const unsigned char *)p)[off];
}

static void put8(void *p,uint32_t off,unsigned char v)
{
    ((unsigned char *)p)[off]=v;
}

void oa_launcher_item_from_novo(void *item,const void *novo,
    const struct oa_launcher_item_ops *ops)
{
    uint32_t control;
    if(!item || !novo || !ops)return;

    /* Leading scalar block: +0 from source +0, +4 from +24, +8 from +12,
     * +12 from +16. */
    put32(item,0x00,get32(novo,0x00));
    put32(item,0x04,get32(novo,0x18));
    put32(item,0x08,get32(novo,0x0c));
    put32(item,0x0c,get32(novo,0x10));

    /* UStrings +0x10/+0x1c/+0x24, then the default-constructed image path at
     * +0x2c, then +0x34/+0x3c. */
    ops->ustr_copy((unsigned char *)item+0x10,(const unsigned char *)novo+0x1c);
    put8(item,0x18,1);
    ops->ustr_copy((unsigned char *)item+0x1c,(const unsigned char *)novo+0x24);
    ops->ustr_copy((unsigned char *)item+0x24,(const unsigned char *)novo+0x2c);
    ops->ustr_default((unsigned char *)item+0x2c);
    ops->ustr_copy((unsigned char *)item+0x34,(const unsigned char *)novo+0x34);
    ops->ustr_copy((unsigned char *)item+0x3c,(const unsigned char *)novo+0x3c);

    /* +0x44/+0x48 from +0x44/+0x48. Advertising/tracking source UStrings
     * at+78/+80/+88 are excluded; construct empty destination slots. */
    put32(item,0x44,get32(novo,0x44));
    put32(item,0x48,get32(novo,0x48));
    ops->ustr_default((unsigned char *)item+0x4c);
    ops->ustr_default((unsigned char *)item+0x54);
    ops->ustr_default((unsigned char *)item+0x5c);

    /* +0x64 from +0x90, +0x68 from +0x08, then the three flag bytes. */
    put32(item,0x64,get32(novo,0x90));
    put32(item,0x68,get32(novo,0x08));
    put8(item,0x6c,byte_at(novo,0x51));
    put8(item,0x6d,byte_at(novo,0x50));
    put8(item,0x6e,byte_at(novo,0x54));

    /* UStrings from +0x58/+0x60. */
    ops->ustr_copy((unsigned char *)item+0x70,(const unsigned char *)novo+0x58);
    ops->ustr_copy((unsigned char *)item+0x78,(const unsigned char *)novo+0x60);

    /* +0x80 from +0x14, +0x84/+0x88 from +0x68/+0x6c. */
    put32(item,0x80,get32(novo,0x14));
    put32(item,0x84,get32(novo,0x68));
    put32(item,0x88,get32(novo,0x6c));

    /* shared_ptr: control block first, refcount increment, then the raw
     * pointer. The shipped body takes the atomic branch when the runtime's
     * gthread probe is non-null (it is in the vendor process). */
    control=get32(novo,0x70);
    put32(item,0x8c,control);
    if(control)
        (void)__atomic_add_fetch(
            (uint32_t *)(uintptr_t)(control+4u),1u,__ATOMIC_SEQ_CST);
    put32(item,0x90,get32(novo,0x74));

    put32(item,0x94,UINT32_C(0xffffffff));
    put32(item,0x98,0);
    put32(item,0x9c,0);
    put32(item,0xa0,0);

    /* When the thumbnail-name UString is non-empty, prefix it with the
     * downloaded-images directory and separator. The shipped body branches on
     * isEmpty with cbz, i.e. it fills the path for non-empty names; an empty
     * name leaves the default-constructed path at +0x2c empty. */
    if(!ops->ustr_is_empty((const unsigned char *)novo+0x3c)) {
        ops->ustr_assign_literal((unsigned char *)item+0x2c,
            OA_LAUNCHER_IMAGE_DIR);
        ops->ustr_append_literal((unsigned char *)item+0x2c,
            OA_LAUNCHER_IMAGE_SEP);
        ops->ustr_append((unsigned char *)item+0x2c,
            (const unsigned char *)novo+0x3c);
    }
}

/* SqliteLauncherDatabaseAdapter::copyLauncherInfoToItem (0xfac334). The field
 * order and the UString assignment order follow the shipped body exactly. */
void oa_launcher_info_to_item(void *item,const struct oa_launcher_info *info,
    const struct oa_launcher_item_ops *ops)
{
    if(!item || !info || !ops)return;

    put32(item,0x00,(uint32_t)info->id);
    put32(item,0x04,(uint32_t)info->parent_id);
    put32(item,0x08,(uint32_t)info->order_index);
    put32(item,0x14,(uint32_t)info->item_type);
    put32(item,0x0c,(uint32_t)info->original_id);
    put32(item,0x10,(uint32_t)info->server_id);
    put32(item,0x18,(uint32_t)info->menu_return_id);

    ops->ustr_assign_literal((unsigned char *)item+0x1c,info->menu_string_id);
    ops->ustr_assign_literal((unsigned char *)item+0x24,info->application_name);
    ops->ustr_assign_literal((unsigned char *)item+0x34,info->thumb_url);
    ops->ustr_assign_literal((unsigned char *)item+0x3c,info->thumb_file_name);
    ops->ustr_assign_literal((unsigned char *)item+0x2c,info->application_url);

    put32(item,0x44,info->application_update_time);
    put32(item,0x48,info->thumb_update_time);
    put32(item,0x4c,info->image_crc);

    put8(item,0x51,info->is_featured);
    put8(item,0x50,info->is_deleted);
    put8(item,0x52,info->is_out_of_sync);
    put8(item,0x53,info->is_moved_by_user);
    put8(item,0x54,info->is_delete_disabled);

    ops->ustr_assign_literal((unsigned char *)item+0x58,info->fvp_application_id);
    ops->ustr_assign_literal((unsigned char *)item+0x60,info->recommendation_url);
    /* Explicit owner-required deviation: initialise these slots empty even
     * when the input record was not produced by our sanitized DB adapter. */
    ops->ustr_assign_literal((unsigned char *)item+0x78,"");
    ops->ustr_assign_literal((unsigned char *)item+0x80,"");
    ops->ustr_assign_literal((unsigned char *)item+0x88,"");

    put32(item,0x90,(uint32_t)info->channel_handle);
}
