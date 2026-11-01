/* Decode a read-only Aurora AOUT cache snapshot from this target firmware.
 * Source: AUDIO-ROUTING-2026-11-01; not a hardware query or configuration tool.
 * Capture 612 bytes at 0x0197a138 in the verified live Aurora process: three
 * 24-byte output records followed by nine 60-byte speaker records. A sample is
 * not atomic and cached state does not prove that audio is physically audible.
 */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

enum { OUTPUT_COUNT = 3, OUTPUT_SIZE = 24, SPEAKER_COUNT = 9, SPEAKER_SIZE = 60,
       SNAPSHOT_SIZE = OUTPUT_COUNT * OUTPUT_SIZE + SPEAKER_COUNT * SPEAKER_SIZE };

static uint32_t word(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static unsigned half(const uint8_t *p)
{
    return (unsigned)p[0] | (unsigned)p[1] << 8;
}
int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s AURORA_AOUT_SNAPSHOT.bin\n", argv[0]);
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) { perror(argv[1]); return 1; }
    uint8_t bytes[SNAPSHOT_SIZE + 1];
    size_t size = fread(bytes, 1, sizeof bytes, file);
    int failed = ferror(file);
    if (fclose(file)) failed = 1;
    if (failed || size != SNAPSHOT_SIZE) {
        fprintf(stderr, "expected exactly %u bytes of AOUT cache\n", (unsigned)SNAPSHOT_SIZE);
        return 1;
    }
    /* Check every identity before printing anything. An address/layout mismatch
     * must not yield a plausible partial table. No host pointer dereferences. */
    for (unsigned i = 0; i < OUTPUT_COUNT; i++)
        if (word(bytes + i * OUTPUT_SIZE) != i) {
            fputs("output identity/layout mismatch\n", stderr); return 1;
        }
    for (unsigned i = 0; i < SPEAKER_COUNT; i++)
        if (word(bytes + OUTPUT_COUNT * OUTPUT_SIZE + i * SPEAKER_SIZE + 8) != i) {
            fputs("speaker identity/layout mismatch\n", stderr); return 1;
        }
    puts("Cached state only; firmware MD5/source and live process must be verified separately.");
    for (unsigned i = 0; i < OUTPUT_COUNT; i++) {
        const uint8_t *p = bytes + i * OUTPUT_SIZE;
        printf("output=%u handle=%08" PRIx32 " speaker_head=%08" PRIx32 "\n",
               i, word(p + 12), word(p + 20));
    }
    for (unsigned i = 0; i < SPEAKER_COUNT; i++) {
        const uint8_t *p = bytes + OUTPUT_COUNT * OUTPUT_SIZE + i * SPEAKER_SIZE;
        printf("speaker=%u present=%u output=%" PRIu32 " hw_mute=%u L=%u R=%u\n",
               i, (unsigned)p[16], word(p + 12), (unsigned)p[18], half(p + 24), half(p + 26));
    }
    return 0;
}
