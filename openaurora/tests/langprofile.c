/* Vendor langprofile Dictionary: all-locale lookup and the proven current ->
 * English -> key fallback. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/langprofile.h"

int main(int argc, char **argv)
{
    assert(argc == 2);
    struct oa_langprofile *p = NULL;
    assert(oa_langprofile_open(&p, argv[1]) == 0 && p);
    assert(oa_langprofile_locale_count(p) == 41);
    assert(!strcmp(oa_langprofile_locale(p, 0), "eng"));
    assert(!strcmp(oa_langprofile_locale(p, 10), "dut"));
    assert(oa_langprofile_locale_index(p, "eng") == 0);
    assert(oa_langprofile_locale_index(p, "dut") == 10);
    assert(oa_langprofile_locale_index(p, "ger") >= 0);
    assert(oa_langprofile_locale_index(p, "fre") >= 0);
    assert(oa_langprofile_locale_index(p, "nope") == -1);

    assert(!strcmp(oa_langprofile_string(p, "MODE", "eng"), "Mode"));
    assert(!strcmp(oa_langprofile_string(p, "MODE", "dut"), "Modus"));
    assert(!strcmp(oa_langprofile_string(p, "MODE", "ger"), "Modus"));
    assert(!strcmp(oa_langprofile_string(p, "MODE", "fre"), "Mode"));
    assert(!strcmp(oa_langprofile_string(p, "ENERGY_SAVING", "dut"),
                   "Energiebesparing"));
    assert(!strcmp(oa_langprofile_string(p, "ENERGY_SAVING", "eng"),
                   "Energy Saving"));
    assert(!strcmp(oa_langprofile_string(p, "ENERGY_SAVING", "ger"),
                   "Energie sparen"));
    assert(!strcmp(oa_langprofile_string(p, "PICTURE_ZOOM", "dut"),
                   "Beeld zoomen"));
    assert(!strcmp(oa_langprofile_string(p, "NATURAL", "dut"), "Natuurlijk"));
    assert(!strcmp(oa_langprofile_string(p, "MAXIMUM_STR", "dut"), "Maximaal"));
    /* Dutch is empty; Dictionary::translate falls back to English "16:9". */
    assert(!strcmp(oa_langprofile_string(p, "ZOOM_MODE_16_9", "eng"), "16:9"));
    assert(!strcmp(oa_langprofile_string(p, "ZOOM_MODE_16_9", "dut"), "16:9"));
    /* Empty Flemish MODE also falls back to English. */
    assert(!strcmp(oa_langprofile_string(p, "MODE", "fle"), "Mode"));
    /* Unknown locale/key returns the key, matching the explicit path. */
    assert(!strcmp(oa_langprofile_string(p, "MODE", "nope"), "MODE"));
    assert(!strcmp(oa_langprofile_string(p, "NO_SUCH_KEY", "dut"),
                   "NO_SUCH_KEY"));
    oa_langprofile_close(p);

    puts("ok langprofile: 41 locales, localized MODE/ENERGY_SAVING/"
         "PICTURE_ZOOM and eng->key fallback");
    return 0;
}
