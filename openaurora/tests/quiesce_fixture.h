#ifndef QUIESCE_FIXTURE_H
#define QUIESCE_FIXTURE_H
struct quiesce_observer {
    int busy, fail_init, fail_start, post_start, defer;
    int init, start, stop, destroy, gate, completion;
    unsigned owner;
};
#define QUIESCE_OBSERVER "test.quiesce.observer"
#define QUIESCE_SERVICE "test.quiesce.instance"
#endif
