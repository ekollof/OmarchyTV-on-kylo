/* View-only ALSA speaker-gain metadata/value query. Firmware mapping:
 * NDAL3 KItem 0x42 -> card0, integer control "speaker out gain".
 * The paired kernel getter/setter and status limitations are documented in
 * AUDIO-GAIN-TRANSPORT-2026-11-01. This tool has no write operation.
 */
#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>

int main(int argc, char **argv)
{
    if (argc != 2 || strcmp(argv[1], "--read-gain")) {
        fprintf(stderr, "usage: %s --read-gain\n", argv[0]); return 2;
    }
    snd_ctl_t *ctl = NULL;
    snd_ctl_elem_info_t *info = NULL;
    snd_ctl_elem_value_t *value = NULL;
    int rc = snd_ctl_open(&ctl, "hw:0", SND_CTL_READONLY);
    if (rc < 0) goto failed;
    rc = snd_ctl_elem_info_malloc(&info);
    if (rc < 0) goto failed;
    rc = snd_ctl_elem_value_malloc(&value);
    if (rc < 0) goto failed;
    snd_ctl_elem_info_set_interface(info, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_info_set_name(info, "speaker out gain");
    rc = snd_ctl_elem_info(ctl, info);
    if (rc < 0) goto failed;
    unsigned count = snd_ctl_elem_info_get_count(info);
    if (snd_ctl_elem_info_get_type(info) != SND_CTL_ELEM_TYPE_INTEGER || !count || count > 16) {
        fputs("unexpected speaker gain type/count\n", stderr); rc = -1; goto done;
    }
    snd_ctl_elem_value_set_interface(value, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_value_set_name(value, "speaker out gain");
    rc = snd_ctl_elem_read(ctl, value);
    if (rc < 0) goto failed;
    printf("card=0 control=speaker out gain count=%u min=%ld max=%ld values=", count,
           snd_ctl_elem_info_get_min(info), snd_ctl_elem_info_get_max(info));
    for (unsigned i = 0; i < count; i++)
        printf("%s%ld", i ? "," : "", snd_ctl_elem_value_get_integer(value, i));
    putchar('\n'); rc = 0;
    goto done;
failed:
    fprintf(stderr, "ALSA read failed: %s (%d)\n", snd_strerror(rc), rc);
done:
    if (value) snd_ctl_elem_value_free(value);
    if (info) snd_ctl_elem_info_free(info);
    if (ctl) {
        int closed = snd_ctl_close(ctl);
        if (!rc && closed < 0) rc = closed;
    }
    return rc ? 1 : 0;
}
