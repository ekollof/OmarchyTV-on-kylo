#ifndef OA_TEST_POST_WORKER_H
#define OA_TEST_POST_WORKER_H
#include <stdint.h>
#define POST_WORKER_SERVICE "test.post_worker"
struct post_worker_iface {
    int (*enqueue)(void);
    unsigned (*calls)(void);
    int (*late_post_result)(void);
    uint32_t (*owner)(void);
};
#endif
