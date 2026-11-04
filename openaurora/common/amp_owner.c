#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "openaurora/amp_owner.h"
struct oa_amp_owner {
    pthread_t thread;
    pthread_mutex_t mu;
    pthread_cond_t cv;
    pid_t pid;
    int started,stopping,pending,ready;
    uint64_t next_ticket;
    oa_amp_init_io io;
    oa_amp_init_state state;
    oa_ad82587_profile profile;
    char power[32];
    oa_amp_completion completion;
};
static int valid(oa_amp_owner *o)
{ return !o?-EINVAL:o->pid!=getpid()?-ECHILD:0; }
static void *worker(void *v)
{
    oa_amp_owner *o=v;
    for(;;) {
        pthread_mutex_lock(&o->mu);
        while(!o->pending && !o->stopping)pthread_cond_wait(&o->cv,&o->mu);
        if(!o->pending && o->stopping){pthread_mutex_unlock(&o->mu);return NULL;}
        pthread_mutex_unlock(&o->mu);
        /* pending stays set through execution, so submissions cannot mutate
         * these copied inputs. Retained state is exclusively worker-owned. */
        int result=oa_amp_init(&o->state,&o->profile,&o->io);
        pthread_mutex_lock(&o->mu);
        o->completion.native_result=result;o->completion.state=o->state;
        o->pending=0;o->ready=1;
        pthread_mutex_unlock(&o->mu);
    }
}
int oa_amp_owner_create(oa_amp_owner **out,const oa_amp_init_state *initial,const oa_amp_init_io *io)
{
    if(!out || !initial)return -EINVAL;
    if(!io)return -ENOTSUP;
    if(!io->prelude || !io->gpio || !io->direction || !io->level || !io->delay_ms ||
        !io->write || !io->subwoofer_enabled || !io->subwoofer_enable || !io->subwoofer_init)return -EINVAL;
    oa_amp_owner *o=calloc(1,sizeof *o);if(!o)return -ENOMEM;
    o->pid=getpid();o->state=*initial;o->io=*io;
    int rc=pthread_mutex_init(&o->mu,NULL);
    if(rc){free(o);return -rc;}
    rc=pthread_cond_init(&o->cv,NULL);
    if(rc){pthread_mutex_destroy(&o->mu);free(o);return -rc;}
    rc=pthread_create(&o->thread,NULL,worker,o);
    if(rc){pthread_cond_destroy(&o->cv);pthread_mutex_destroy(&o->mu);free(o);return -rc;}
    o->started=1;*out=o;return 0;
}
int oa_amp_owner_submit(oa_amp_owner *o,const oa_ad82587_profile *p,uint64_t *ticket)
{
    int rc=valid(o);if(rc)return rc;
    if(!p || !p->speaker_power || !ticket)return -EINVAL;
    size_t length=strnlen(p->speaker_power,sizeof o->power);
    if(length>=sizeof o->power)return -ENAMETOOLONG;
    pthread_mutex_lock(&o->mu);
    if(o->stopping)rc=-ESHUTDOWN;
    else if(o->pending || o->ready)rc=-EBUSY;
    else if(o->next_ticket==UINT64_MAX)rc=-EOVERFLOW;
    else {
        o->profile=*p;memcpy(o->power,p->speaker_power,length+1);o->profile.speaker_power=o->power;
        o->completion=(oa_amp_completion){.ticket=++o->next_ticket};
        o->pending=1;*ticket=o->next_ticket;pthread_cond_signal(&o->cv);
    }
    pthread_mutex_unlock(&o->mu);return rc;
}
int oa_amp_owner_take(oa_amp_owner *o,oa_amp_completion *out)
{
    int rc=valid(o);if(rc)return rc;
    if(!out)return -EINVAL;
    pthread_mutex_lock(&o->mu);
    if(o->ready){*out=o->completion;o->ready=0;rc=1;}
    pthread_mutex_unlock(&o->mu);return rc;
}
int oa_amp_owner_join(oa_amp_owner *o)
{
    int rc=valid(o);if(rc)return rc;
    pthread_mutex_lock(&o->mu);int started=o->started;pthread_mutex_unlock(&o->mu);
    if(!started)return 0;
    if(pthread_equal(o->thread,pthread_self()))return -EDEADLK;
    rc=pthread_tryjoin_np(o->thread,NULL);
    if(rc)return -rc;
    pthread_mutex_lock(&o->mu);o->started=0;pthread_mutex_unlock(&o->mu);return 0;
}
int oa_amp_owner_quiesce(oa_amp_owner *o)
{
    int rc=valid(o);if(rc)return rc;
    pthread_mutex_lock(&o->mu);o->stopping=1;pthread_cond_signal(&o->cv);pthread_mutex_unlock(&o->mu);
    return oa_amp_owner_join(o);
}
int oa_amp_owner_destroy(oa_amp_owner *o)
{
    int rc=valid(o);if(rc)return rc;
    pthread_mutex_lock(&o->mu);int started=o->started;pthread_mutex_unlock(&o->mu);
    if(started)return -EBUSY;
    pthread_cond_destroy(&o->cv);pthread_mutex_destroy(&o->mu);free(o);return 0;
}
