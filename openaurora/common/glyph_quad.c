#include "openaurora/glyph_quad.h"
#include <errno.h>
#include <limits.h>
#include <stdint.h>
static int fits(int64_t n) { return n>=INT_MIN && n<=INT_MAX; }
int oa_glyph_quad_make(int sw,int sh,int x,int y,int w,int h,
                       const struct oa_font_clip *clip,struct oa_glyph_quad *out)
{
    if(!out || sw<2 || sh<2 || w<=0 || h<=0) return EINVAL;
    int64_t l=x,t=y,r=(int64_t)x+w,b=(int64_t)y+h;
    if(!fits(r) || !fits(b)) return EOVERFLOW;
    if(clip) {
        if(clip->width<=0 || clip->height<=0) return ENOENT;
        int64_t cr=(int64_t)clip->x+clip->width,cb=(int64_t)clip->y+clip->height;
        if(!fits(cr) || !fits(cb)) return EOVERFLOW;
        if(l<clip->x)l=clip->x;
        if(t<clip->y)t=clip->y;
        if(r>cr)r=cr;
        if(b>cb)b=cb;
        if(r<=l || b<=t) return ENOENT;
    }
    int half_w=sw/2,half_h=sh/2;
    if(!fits(l-half_w) || !fits(r-half_w) || !fits(t-half_h) || !fits(b-half_h))
        return EOVERFLOW;
    float x0=(float)(l-half_w)/(float)half_w,x1=(float)(r-half_w)/(float)half_w;
    float y0=-((float)(t-half_h)/(float)half_h),y1=-((float)(b-half_h)/(float)half_h);
    float u0=(float)(l-x)/(float)w,u1=(float)(r-x)/(float)w;
    float v0=(float)(t-y)/(float)h,v1=(float)(b-y)/(float)h;
    struct oa_glyph_quad q={{x0,y0,0,1,u0,v0, x1,y0,0,1,u1,v0,
                            x1,y1,0,1,u1,v1, x0,y1,0,1,u0,v1}};
    *out=q;return 0;
}
