#ifndef OPENAURORA_RELOAD_FIXTURE_H
#define OPENAURORA_RELOAD_FIXTURE_H

#include <stddef.h>

#define RELOAD_OBSERVER "reload_observer"
#define RELOAD_SERVICE "reload_fixture_service"

/* Host-owned observations survive actual dlclose/reopen of the fixture. */
struct reload_observer {
    unsigned init, start, stop, destroy, suspend, resume, restore;
    unsigned value, restored_value, active_value, bad_callback;
    int fail_restore, fail_start, fail_resume, post_on_restore;
    size_t save_need, save_result;
};

#endif
