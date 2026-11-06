/* Vendor langprofile Dictionary reader.
 *
 * EFSL/ROMProfile/Dictionary layout and the custom string hash are the ones
 * verified in artifacts/analysis/ui-row-labels-2026-11-02. A lookup follows
 * Dictionary::translate: current locale, then English, then the key itself
 * when the selected translation is empty. Returned strings are borrowed from
 * the open profile and remain valid until oa_langprofile_close().
 */
#ifndef OPENAURORA_LANGPROFILE_H
#define OPENAURORA_LANGPROFILE_H

#include <stddef.h>

struct oa_langprofile;

int oa_langprofile_open(struct oa_langprofile **out, const char *path);
void oa_langprofile_close(struct oa_langprofile *profile);

unsigned oa_langprofile_locale_count(const struct oa_langprofile *profile);
const char *oa_langprofile_locale(const struct oa_langprofile *profile,
                                  unsigned index);
int oa_langprofile_locale_index(const struct oa_langprofile *profile,
                                const char *code);

/* Translate key using locale (e.g. "dut"). Unknown locale or missing key
 * returns key. An empty translation falls back to "eng", then to key. */
const char *oa_langprofile_string(const struct oa_langprofile *profile,
                                  const char *key, const char *locale);

/* Remove Unicode directional/format controls that are not font glyphs
 * (U+200E/200F, U+202A..202E, U+2066..2069). This is not BiDi reordering or
 * shaping; RTL visual order remains a separate limitation. Returns 0 and a
 * NUL-terminated string, EOVERFLOW if cap is too small, or EINVAL. */
int oa_langprofile_sanitize(const char *in, char *out, size_t cap);

#endif
