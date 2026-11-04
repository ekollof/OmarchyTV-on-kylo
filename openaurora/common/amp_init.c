#include <errno.h>
#include "openaurora/amp_init.h"
static void mute(oa_amp_init_state *s,const oa_amp_init_io *io,const oa_amp_gpio *g,unsigned value)
{
    if(g && io->level(io->context,g->pin,value==1?g->level_on:g->level_off))
        s->mute_cached=(uint8_t)(value==1);
}
static void reset(oa_amp_init_state *s,const oa_amp_init_io *io,const oa_amp_gpio *g,unsigned value)
{
    if(g && io->level(io->context,g->pin,value==1?g->level_off:!g->level_off))
        s->reset_cached=(uint8_t)(value==1);
}
int oa_amp_init(oa_amp_init_state *s,const oa_ad82587_profile *p,const oa_amp_init_io *io)
{
    if(!s || !p || !p->speaker_power || !io || !io->prelude || !io->gpio || !io->direction ||
        !io->level || !io->delay_ms || !io->write || !io->subwoofer_enabled ||
        !io->subwoofer_enable || !io->subwoofer_init)return -EINVAL;
    void *c=io->context;
    io->prelude(c);
    const oa_amp_gpio *m=io->gpio(c,1);
    if(m)io->direction(c,m->pin,1);
    mute(s,io,m,0);io->delay_ms(c,5);
    const oa_amp_gpio *r=io->gpio(c,0);
    if(r) {
        io->direction(c,r->pin,1);io->level(c,r->pin,r->level_off);io->delay_ms(c,20);
        io->level(c,r->pin,!r->level_off);io->delay_ms(c,10);
    }
    m=io->gpio(c,1);
    if(m)io->direction(c,m->pin,1);
    reset(s,io,r,1);io->delay_ms(c,10);reset(s,io,r,0);
    mute(s,io,m,0);io->delay_ms(c,1000);
    int rc=oa_ad82587_configure(s->image,s->image,p);
    if(rc)return rc;
    for(unsigned reg=0;reg<OA_AD82587_REGISTERS;++reg)if(!io->write(c,reg,s->image+reg)) {
        io->delay_ms(c,10);mute(s,io,m,1);return 0;
    }
    io->subwoofer_enable(c,io->subwoofer_enabled(c)==1);
    io->subwoofer_init(c);io->delay_ms(c,10);mute(s,io,m,1);return 1;
}
