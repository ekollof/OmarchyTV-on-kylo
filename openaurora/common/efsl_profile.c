#include "openaurora/efsl_profile.h"

#include <string.h>

#define EFSL_BASE_WORD 0x20u
#define EFSL_ROOT 0x30u
#define EFSL_ROOT_COUNT 0x38u

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int section_fits(const oa_efsl_profile *profile, size_t entries,
                        uint32_t count)
{
    if (!profile || !profile->data || entries > profile->size)
        return 0;
    return (size_t)count <= (profile->size - entries) / OA_EFSL_ENTRY_SIZE;
}

uint32_t oa_efsl_hash(const char *key)
{
    if (!key)
        return 0;
    const uint8_t *p = (const uint8_t *)key;
    size_t len = strlen(key);
    /* ROMProfile::getUniqueID seeds with DAT_00c5e18c = 0xf1d3b597 and
     * strlen(key) << 6. */
    uint32_t h = 0xf1d3b597u ^ ((uint32_t)len << 6);
    uint32_t prev = 0;
    uint32_t total = 0;
    uint32_t w = 0;

    for (; *p; p++) {
        /* ldrsb.w: the hash consumes the sign-extended byte. */
        uint32_t c = (uint32_t)(int32_t)(int8_t)*p;
        uint32_t v = h + ((~prev) << 1);
        v ^= total << 9;
        v += c << 17;
        v ^= w;
        w = c + (w << 8);
        /* __aeabi_uidivmod(c, 3) remainder, then __aeabi_uidivmod(prev, 5). */
        v += v << (c % 3u);
        v += v >> (prev % 5u);
        h = v;
        prev = c;
        total += c;
    }
    return h;
}

int oa_efsl_open(oa_efsl_profile *profile, const void *data, size_t size)
{
    if (!profile || !data)
        return -1;
    const uint8_t *d = data;
    size_t first = OA_EFSL_HEADER_SIZE + OA_EFSL_ENTRY_SIZE;
    if (size < first)
        return -1;
    if (memcmp(d, OA_EFSL_MAGIC, 4) != 0)
        return -1;
    /* ROMProfile::initialize only publishes a file base when this word is 0. */
    if (rd32(d + EFSL_BASE_WORD) != 0)
        return -1;
    uint32_t count = rd32(d + EFSL_ROOT_COUNT);
    if ((size_t)count > (size - first) / OA_EFSL_ENTRY_SIZE)
        return -1;
    profile->data = d;
    profile->size = size;
    return 0;
}

int oa_efsl_root(const oa_efsl_profile *profile, oa_efsl_section *out)
{
    if (!profile || !profile->data || !out)
        return -1;
    uint32_t count = rd32(profile->data + EFSL_ROOT_COUNT);
    size_t entries = OA_EFSL_HEADER_SIZE + OA_EFSL_ENTRY_SIZE;
    if (!section_fits(profile, entries, count))
        return -1;
    out->entries = entries;
    out->count = count;
    return 0;
}

int oa_efsl_open_section(const oa_efsl_profile *profile,
                         const oa_efsl_section *parent, const char *name,
                         oa_efsl_section *out)
{
    if (!profile || !profile->data || !parent || !name || !out)
        return -1;
    if (!section_fits(profile, parent->entries, parent->count))
        return -1;
    uint32_t want = oa_efsl_hash(name);
    for (uint32_t i = 0; i < parent->count; i++) {
        size_t off = parent->entries + (size_t)i * OA_EFSL_ENTRY_SIZE;
        const uint8_t *e = profile->data + off;
        uint32_t hash = rd32(e);
        uint8_t type = e[4];
        if (type == 1) {
            uint32_t children = rd32(e + 8);
            if (hash == want) {
                size_t entries = off + OA_EFSL_ENTRY_SIZE;
                if (!section_fits(profile, entries, children))
                    return -1;
                out->entries = entries;
                out->count = children;
                return 0;
            }
            /* findValue skips a subsection and its children together. */
            if (children > parent->count - 1u - i)
                return -1;
            i += children;
        }
    }
    return -1;
}

int oa_efsl_read_value(const oa_efsl_profile *profile,
                       const oa_efsl_section *section, const char *key,
                       void *out, size_t cap, size_t *out_len)
{
    if (!profile || !profile->data || !section || !key)
        return -1;
    if (!section_fits(profile, section->entries, section->count))
        return -1;
    uint32_t want = oa_efsl_hash(key);
    for (uint32_t i = 0; i < section->count; i++) {
        size_t off = section->entries + (size_t)i * OA_EFSL_ENTRY_SIZE;
        const uint8_t *e = profile->data + off;
        uint32_t hash = rd32(e);
        uint8_t type = e[4];
        if (type == 1) {
            uint32_t children = rd32(e + 8);
            if (children > section->count - 1u - i)
                return -1;
            i += children;
            continue;
        }
        if (hash != want)
            continue;
        /* readValue accepts types 2..4 and requires size <= caller length. */
        if (type < 2 || type > 4)
            return -1;
        uint32_t len = rd32(e + 8);
        uint32_t value = rd32(e + 12);
        if ((size_t)len > cap)
            return -1;
        if (len && !out)
            return -1;
        if (value > profile->size || (size_t)len > profile->size - value)
            return -1;
        if (len)
            memcpy(out, profile->data + value, len);
        if (out_len)
            *out_len = len;
        return 0;
    }
    return -1;
}
