#include "openaurora/launcher_image_id.h"
#include <stddef.h>

/*
 * Pairs are {state == 1, state != 1}, transcribed from the shipped
 * LauncherHelper::getImageId body at 0xe4aaf4. Operations 30, 31, 36 and 37
 * fall through to the -1 default; 42, 56 and 57 ignore the state.
 */
static const int16_t launcher_ids[62][2]={
    {729,730},{731,732},{733,734},{735,736},{737,738},{739,740},
    {741,742},{743,744},{745,746},{749,750},{759,760},{761,762},
    {765,766},{767,768},{769,770},{771,772},{773,774},{775,776},
    {777,778},{779,780},{781,782},{781,782},{781,782},{781,782},
    {783,784},{785,786},{805,806},{787,788},{789,790},{791,792},
    {-1,-1},{-1,-1},{799,800},{807,808},{809,810},{811,812},
    {-1,-1},{-1,-1},{828,829},{832,833},{813,814},{834,835},
    {167,167},{781,782},{781,782},{801,802},{763,764},{763,764},
    {803,804},{747,748},{799,800},{755,756},{825,790},{753,754},
    {751,752},{757,758},{847,847},{848,848},{793,794},{795,796},
    {797,798},{836,837}
};

int32_t oa_launcher_get_image_id(uint32_t operation,uint32_t state)
{
    if(operation>=sizeof launcher_ids/sizeof launcher_ids[0])return -1;
    return launcher_ids[operation][state==1?0:1];
}
