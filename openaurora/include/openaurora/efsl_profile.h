/* Read-only parser for the vendor EFSL "ROM profile" container.
 *
 * Recovered from `aurora.elf` (MD5 64c252340d3af6c5d66072611b7bad7a):
 *   ROMProfile::initialize   @0x00c5dd44
 *   ROMProfile::getUniqueID  @0x00c5e100
 *   ROMProfile::findValue    @0x00c5e298
 *   ROMProfile::readValue    @0x00c5e350 (variant, key, buffer, length)
 *   ROMProfile::openSection  @0x00c5e1fc (variant/name path)
 *
 * Container layout (little-endian):
 *   0x00  "EFSL"
 *   0x10  profile name (16 bytes, e.g. "hwprofile")
 *   0x20  must be 0 for ROMProfile::initialize to publish a base pointer
 *   0x30  root section: +0x08 total entry count, entries at +0x14
 *
 * Each entry is 0x14 bytes:
 *   +0x00  key hash (ROMProfile::getUniqueID of the key string)
 *   +0x04  type: 1 = subsection, 2/3/4 = value
 *   +0x08  type 1: child entry count; types 2..4: value byte length
 *   +0x0c  types 2..4: value offset relative to the file base
 *   +0x10  not used by the recovered lookup/read paths
 *
 * Type-1 entries are skipped by the lookup walk together with their children;
 * the root count covers the whole table including children.
 *
 * The vendor trusts the file. This parser validates every count, offset and
 * copy against the caller-provided buffer before use, because the profile is
 * a provider-controlled input.
 */
#ifndef OPENAURORA_EFSL_PROFILE_H
#define OPENAURORA_EFSL_PROFILE_H
#include <stddef.h>
#include <stdint.h>

#define OA_EFSL_MAGIC "EFSL"
#define OA_EFSL_HEADER_SIZE 0x30u
#define OA_EFSL_ENTRY_SIZE 0x14u

typedef struct {
    const uint8_t *data;
    size_t size;
} oa_efsl_profile;

/* One section: entries[i] is at `entries + i * OA_EFSL_ENTRY_SIZE`. */
typedef struct {
    size_t entries;
    uint32_t count;
} oa_efsl_section;

/* ROMProfile::getUniqueID @0x00c5e100. The loop uses the signed byte value
 * (ldrsb) and the DIVMOD remainder of that value for the shift counts. */
uint32_t oa_efsl_hash(const char *key);

/* Validate the EFSL header and the root entry count. 0 = usable. */
int oa_efsl_open(oa_efsl_profile *profile, const void *data, size_t size);

/* Root section at file+0x30. */
int oa_efsl_root(const oa_efsl_profile *profile, oa_efsl_section *out);

/* Find a type-1 child section by name (a single path component; the vendor's
 * openSection also accepts '/'-separated paths, which the profile values used
 * here do not need). */
int oa_efsl_open_section(const oa_efsl_profile *profile,
                         const oa_efsl_section *parent, const char *name,
                         oa_efsl_section *out);

/* findValue + readValue: copy `size` bytes of a type 2/3/4 value into out.
 * The value length must not exceed cap; the whole value must be inside the
 * profile. out_len (optional) receives the copied length. 0 = success. */
int oa_efsl_read_value(const oa_efsl_profile *profile,
                       const oa_efsl_section *section, const char *key,
                       void *out, size_t cap, size_t *out_len);

#endif
