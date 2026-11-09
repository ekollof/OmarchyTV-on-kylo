#ifndef OPENAURORA_TEST_AUDIO_SOURCE_FAKE_H
#define OPENAURORA_TEST_AUDIO_SOURCE_FAKE_H

enum audio_fake_kind {
    AF_CTRL = 1, AF_SELECT, AF_SET, AF_KITEM, AF_DELAY
};

struct audio_fake_event {
    int kind;
    int a, b, c;        /* ctrl: op, code, value; select: path, id, name */
    const char *name;   /* select record name, or NULL */
};

void audio_fake_reset(void);
int audio_fake_count(void);
const struct audio_fake_event *audio_fake_at(int index);

#endif
