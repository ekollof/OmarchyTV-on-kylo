/* CEC reply queue: a failed submission drops only that frame, later replies
 * stay ordered and send, replies are never sent after close, and a successful
 * release clears the queue while a failed release keeps it.
 *
 * The module source is included to reach cec_poll/queue_replies; owner send,
 * take and receive are wrapped so no device or worker is needed.
 */
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/cec_owner.h"
#include "openaurora/module.h"

#include "../modules/cec/cec.c"

static int send_fail;
static int sends;
static int receives;
static int take_ready;
static uint64_t next_ticket = 41;
static oa_cec_message sent[8];
static oa_cec_completion take_completion;

static void fake_log(void *priv, int level, const char *fmt, ...)
{
    (void)priv;
    (void)level;
    (void)fmt;
}

static const oa_host_api g_api = {
    .size = sizeof g_api,
    .abi = OA_ABI_VERSION,
    .log = fake_log,
};

int __wrap_oa_cec_owner_send_async(oa_cec_owner *owner,
                                   const oa_cec_message *message,
                                   uint64_t *ticket)
{
    (void)owner;
    assert(message && ticket && sends < 8);
    sent[sends++] = *message;
    if (send_fail) {
        errno = EIO;
        return -1;
    }
    *ticket = next_ticket++;
    take_ready = 1;
    take_completion = (oa_cec_completion){
        .ticket = *ticket, .operation = OA_CEC_ASYNC_SEND, .rc = 0
    };
    return 0;
}

int __wrap_oa_cec_owner_take(oa_cec_owner *owner, oa_cec_completion *out)
{
    (void)owner;
    if (!take_ready)
        return 0;
    *out = take_completion;
    take_ready = 0;
    return 1;
}

int __wrap_oa_cec_owner_receive_async(oa_cec_owner *owner, uint64_t *ticket)
{
    (void)owner;
    (void)ticket;
    receives++;
    errno = ENODEV;
    return -1;
}

static void setup(struct ctx *c, int open)
{
    memset(c, 0, sizeof *c);
    c->api = &g_api;
    c->owner = (oa_cec_owner *)(uintptr_t)1;
    c->enabled = 1;
    c->session_open = open;
    c->reply_head = c->reply_tail = 0;
}

static void queue(struct ctx *c, int opcode)
{
    oa_cec_message m;
    memset(&m, 0, sizeof m);
    m.initiator = 0;
    m.destination = 5;
    m.opcode = opcode;
    queue_replies(c, &m, 1);
}

static void test_drop_only_failed(void)
{
    struct ctx c;
    setup(&c, 1);
    send_fail = 1;
    sends = receives = take_ready = 0;
    queue(&c, 0x70);
    queue(&c, 0x7a);
    assert(c.reply_head == 0 && c.reply_tail == 2);

    cec_poll(&c); /* first submission fails */
    assert(sends == 1 && sent[0].opcode == 0x70);
    assert(c.reply_head == 1 && c.reply_tail == 2); /* only it is dropped */
    assert(c.job_pending == 0 && receives == 0);

    send_fail = 0;
    cec_poll(&c); /* second reply is still attempted */
    assert(sends == 2 && sent[1].opcode == 0x7a);
    assert(c.reply_head == 2 && c.reply_tail == 2 && c.job_pending == 1);

    cec_poll(&c); /* completed send */
    assert(c.job_pending == 0 && c.tx_frames == 1);
    assert(c.reply_head == c.reply_tail);
    puts("ok cec reply queue: failed record dropped, later reply still sent");
}

static void test_no_send_after_close(void)
{
    struct ctx c;
    setup(&c, 0);
    send_fail = 0;
    sends = receives = take_ready = 0;
    queue(&c, 0x70);
    cec_poll(&c);
    assert(sends == 0 && receives == 0);
    assert(c.reply_head == 0 && c.reply_tail == 1);
    puts("ok cec reply queue: nothing is sent while the session is closed");
}

static void test_release_clears(void)
{
    struct ctx c;
    setup(&c, 1);
    queue(&c, 0x70);
    oa_cec_completion release = {
        .operation = OA_CEC_ASYNC_RELEASE, .rc = 0
    };
    c.closing = 1;
    handle_completion(&c, &release);
    assert(c.session_open == 0 && c.closing == 0);
    assert(c.reply_head == c.reply_tail);
    sends = receives = 0;
    cec_poll(&c);
    assert(sends == 0 && receives == 0);
    puts("ok cec reply queue: successful release clears pending replies");

    setup(&c, 1);
    queue(&c, 0x7a);
    release.rc = -1;
    release.error = EIO;
    c.closing = 1;
    handle_completion(&c, &release);
    assert(c.session_open == 1 && c.reply_head == 0 && c.reply_tail == 1);
    puts("ok cec reply queue: failed release keeps the session and queue");
}

int main(void)
{
    test_drop_only_failed();
    test_no_send_after_close();
    test_release_clears();
    puts("all cec reply queue tests passed");
    return 0;
}
