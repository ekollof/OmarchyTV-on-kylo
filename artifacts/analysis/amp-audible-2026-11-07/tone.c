/* Minimal ALSA playback probe for the amp acceptance run. Plays a quiet
 * 440 Hz tone on the default PCM for a few seconds; no file or service. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct _snd_pcm snd_pcm_t;
typedef unsigned long snd_pcm_uframes_t;
extern int snd_pcm_open(snd_pcm_t **pcm, const char *name, int stream,
                        int mode);
extern int snd_pcm_set_params(snd_pcm_t *pcm, int format, int access,
                              unsigned channels, unsigned rate,
                              int soft_resample, unsigned latency);
extern long snd_pcm_writei(snd_pcm_t *pcm, const void *buffer,
                           unsigned long size);
extern int snd_pcm_drain(snd_pcm_t *pcm);
extern int snd_pcm_close(snd_pcm_t *pcm);
extern const char *snd_strerror(int err);

enum { STREAM_PLAYBACK = 0, FORMAT_S16_LE = 2, ACCESS_RW_INTERLEAVED = 3 };

int main(int argc, char **argv)
{
    const char *dev = argc > 1 ? argv[1] : "default";
    unsigned rate = 48000, channels = 2;
    double seconds = 4.0, hz = 440.0, amp = 0.05;
    snd_pcm_t *pcm = NULL;
    int rc = snd_pcm_open(&pcm, dev, STREAM_PLAYBACK, 0);
    if (rc < 0) {
        fprintf(stderr, "open %s: %s\n", dev, snd_strerror(rc));
        return 1;
    }
    rc = snd_pcm_set_params(pcm, FORMAT_S16_LE, ACCESS_RW_INTERLEAVED,
                            channels, rate, 1, 100000);
    if (rc < 0) {
        fprintf(stderr, "params: %s\n", snd_strerror(rc));
        snd_pcm_close(pcm);
        return 1;
    }
    unsigned frames = (unsigned)(rate * seconds);
    int16_t *buf = calloc((size_t)frames * channels, sizeof *buf);
    if (!buf) return 1;
    for (unsigned i = 0; i < frames; i++) {
        double v = sin(2.0 * 3.14159265358979323846 * hz * i / rate);
        int16_t s = (int16_t)(v * amp * 32767.0);
        /* Fade the first/last 100 ms to avoid clicks. */
        unsigned edge = rate / 10;
        if (i < edge) s = (int16_t)(s * i / edge);
        if (i + edge > frames) s = (int16_t)(s * (frames - i) / edge);
        for (unsigned c = 0; c < channels; c++)
            buf[(size_t)i * channels + c] = s;
    }
    long written = snd_pcm_writei(pcm, buf, frames);
    if (written < 0)
        fprintf(stderr, "write: %s\n", snd_strerror((int)written));
    snd_pcm_drain(pcm);
    snd_pcm_close(pcm);
    free(buf);
    puts("tone done");
    return written < 0 ? 1 : 0;
}
