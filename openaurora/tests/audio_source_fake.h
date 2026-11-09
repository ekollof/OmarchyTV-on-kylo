#ifndef OPENAURORA_TEST_AUDIO_SOURCE_FAKE_H
#define OPENAURORA_TEST_AUDIO_SOURCE_FAKE_H

enum audio_fake_kind {
    AF_CTRL = 1, AF_SELECT, AF_SET, AF_KITEM, AF_DELAY, AF_SEND
};

struct audio_fake_event {
    int kind;
    int a, b, c, d, e;  /* ctrl: op,w0,w1,w2,w3; select: path,id,-,-,- */
    const char *name;   /* select record name, or NULL */
};

void audio_fake_reset(void);
int audio_fake_count(void);
const struct audio_fake_event *audio_fake_at(int index);

#endif
