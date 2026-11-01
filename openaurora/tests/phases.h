#ifndef OPENAURORA_PHASES_TEST_H
#define OPENAURORA_PHASES_TEST_H
#include <stddef.h>
#define PHASE_OBSERVER "phase_observer"
struct phase_observer {
    char sequence[128];
    size_t length;
    unsigned starts[3], suspends[3], resumes[3];
    int running[3], fail_suspend, fail_resume;
    void (*after_suspend)(void *);
    void *after_suspend_ud;
};
#endif
