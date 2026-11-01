/* Read-only query using the maintained blocking session; dedicated process.
 * No HAL init, ALSA config, gain/table/mute/reset write or stock RX consumption. */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "openaurora/audio_client.h"

int main(int argc, char **argv)
{
    if (argc != 2 || strcmp(argv[1], "--get-gain")) {
        fprintf(stderr, "usage: %s --get-gain\n", argv[0]); return 2;
    }
    oa_audio_client client = OA_AUDIO_CLIENT_INIT;
    if (oa_audio_client_open(&client, NULL)) { perror("audio session open"); return 1; }
    oa_audio_opu_gain gain;
    int result = oa_audio_client_gain(&client, 0, 2000, &gain);
    int primary = errno;
    int closed = oa_audio_client_close(&client);
    if (result) { errno = primary; perror("audio gain query"); return 1; }
    if (closed) { perror("audio session close"); return 1; }
    printf("reply device=%u left=%u right=%u; operation status is not present\n", gain.device, gain.left, gain.right);
    return 0;
}
