#include "openaurora/glyph_quad.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    struct oa_glyph_quad q;
    assert(!oa_glyph_quad_make(4,4,0,0,4,4,NULL,&q));
    const float full[24]={-1,1,0,1,0,0, 1,1,0,1,1,0, 1,-1,0,1,1,1, -1,-1,0,1,0,1};
    assert(!memcmp(q.vertex,full,sizeof full));
    const struct oa_font_clip clip={1,1,2,1};
    assert(!oa_glyph_quad_make(4,4,0,0,4,4,&clip,&q));
    const float partial[24]={-.5f,.5f,0,1,.25f,.25f, .5f,.5f,0,1,.75f,.25f,
                            .5f,-0.0f,0,1,.75f,.5f, -.5f,-0.0f,0,1,.25f,.5f};
    assert(!memcmp(q.vertex,partial,sizeof partial));
    assert(!oa_glyph_quad_make(5,5,0,0,5,5,NULL,&q) && q.vertex[6]==1.5f);
    assert(!oa_glyph_quad_make(4,4,-2,-2,8,8,NULL,&q) && q.vertex[0]==-2 && q.vertex[1]==2);
    struct oa_glyph_quad saved=q;
    const struct oa_font_clip outside={20,20,2,2},empty={0,0,0,0},overflow={INT_MAX,0,1,1};
    assert(oa_glyph_quad_make(4,4,0,0,4,4,&outside,&q)==ENOENT);
    assert(oa_glyph_quad_make(4,4,0,0,4,4,&empty,&q)==ENOENT);
    assert(oa_glyph_quad_make(4,4,0,0,4,4,&overflow,&q)==EOVERFLOW);
    assert(oa_glyph_quad_make(4,4,INT_MAX,0,1,1,NULL,&q)==EOVERFLOW);
    assert(oa_glyph_quad_make(4,4,INT_MIN,0,1,1,NULL,&q)==EOVERFLOW);
    assert(oa_glyph_quad_make(1,4,0,0,1,1,NULL,&q)==EINVAL);
    assert(oa_glyph_quad_make(4,4,0,0,0,1,NULL,&q)==EINVAL);
    assert(!memcmp(&saved,&q,sizeof q));
    puts("ok glyph quad: exact full/clipped UV geometry, integer-half odd sizes, no premature viewport clip, atomic invalid/overflow/cull");
    return 0;
}
