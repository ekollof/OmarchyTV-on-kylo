#include "openaurora/settings_row_setters.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
/* Percent numerator/denominator wrap mod 2^32 exactly like the shipped
 * subs/muls pair: (value-lo) and (max-lo) are 32-bit differences, scaled by
 * 100 with wraparound. The shipped __aeabi_idiv at 0x1285478 then divides
 * with truncation toward zero. Every zero denominator branches to 0x1285d84,
 * which unconditionally calls raise(8); reject that input with EDOM, leaving
 * the output unchanged. INT32_MIN/-1
 * would be UB in C and wraps to INT32_MIN on the device, so it is explicit. */
int oa_setting_row_percent(int32_t value,int32_t lo,int32_t hi,int32_t *out)
{
    uint32_t num=(uint32_t)((uint32_t)((uint32_t)value-(uint32_t)lo)*UINT32_C(100));
    uint32_t den=(uint32_t)((uint32_t)hi-(uint32_t)lo);
    int32_t n,d;
    memcpy(&n,&num,sizeof n);memcpy(&d,&den,sizeof d);
    if(!out)return EINVAL;
    if(!d)return EDOM;
    *out=n==INT32_MIN && d==-1?INT32_MIN:n/d;
    return 0;
}
static int ready(const struct oa_setting_row_setter_ops *o)
{
    if(!o || !o->row_type || !o->row_min || !o->row_max || !o->child_at ||
        !o->is_created || !o->set_text || !o->set_percent || !o->set_range ||
        !o->store_label || !o->store_value || !o->store_bounds)return EINVAL;
    return 0;
}
int oa_setting_row_set_label(void *row,const struct oa_setting_row_setter_ops *o,const char *label)
{
    void *child;
    if(!row || !label || ready(o))return EINVAL;
    child=o->child_at(row,o->row_type(row)==UINT32_C(1)?UINT32_C(0x3c4):UINT32_C(0x104));
    if(o->is_created(child))o->set_text(child,label);
    o->store_label(row,label);
    return 0;
}
int oa_setting_row_set_progress(void *row,const struct oa_setting_row_setter_ops *o,int32_t value)
{
    int32_t lo,hi,pct;void *bar,*box;char text[12];int n;
    if(!row || ready(o))return EINVAL;
    lo=o->row_min(row);hi=o->row_max(row);
    int error=oa_setting_row_percent(value,lo,hi,&pct);if(error)return error;
    bar=o->child_at(row,o->row_type(row)==UINT32_C(0x17)?UINT32_C(0x1570):UINT32_C(0x1724));
    if(o->is_created(bar))o->set_percent(bar,pct);
    o->store_value(row,value);
    box=o->child_at(row,UINT32_C(0x24e8));
    if(!o->is_created(box))return 0;
    /* The shipped body reloads the stored +0x2dfc word for the format input;
     * it is the same value just stored, so formatting value is identical. */
    n=snprintf(text,sizeof text,"%d",value);
    if(n<0 || n>=(int)sizeof text)return EINVAL;
    o->set_text(box,text);
    return 0;
}
int oa_setting_row_set_bar_range(void *row,const struct oa_setting_row_setter_ops *o,int32_t lo,int32_t hi)
{
    void *bar;
    if(!row || ready(o))return EINVAL;
    bar=o->child_at(row,o->row_type(row)==UINT32_C(9)?UINT32_C(0x13c0):UINT32_C(0x1498));
    if(o->is_created(bar))o->set_range(bar,lo,hi);
    o->store_bounds(row,lo,hi);
    return 0;
}
