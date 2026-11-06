#include "openaurora/langprofile.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define PROFILE_MAX (16u * 1024u * 1024u)
#define LOCALE_MAX 256u
#define RECORD_MAX 100000u

struct record {
    uint32_t hash;
    uint32_t offset;
};

struct oa_langprofile {
    unsigned char *data;
    size_t size;
    size_t base, length, end;
    size_t strings_base;
    unsigned locales, records;
    const char **codes;
    struct record *index;
};

static int rd32(const struct oa_langprofile *p, size_t at, uint32_t *out)
{
    if (at > p->size || p->size - at < 4) return EINVAL;
    *out = (uint32_t)p->data[at] | (uint32_t)p->data[at + 1] << 8 |
           (uint32_t)p->data[at + 2] << 16 | (uint32_t)p->data[at + 3] << 24;
    return 0;
}

static uint32_t key_hash(const char *key)
{
    size_t len = strlen(key);
    uint32_t h = 0xf1d3b597u ^ ((uint32_t)len << 6);
    uint32_t prev = 0, sum = 0, rolling = 0;
    for (size_t i = 0; i < len; i++) {
        uint32_t c = (unsigned char)key[i];
        h = (h + 2u * ~prev) ^ (sum << 9);
        sum += c;
        h = (h + (c << 17)) ^ rolling;
        rolling = c + (rolling << 8);
        h = h + (h << (c % 3));
        h = h + (h >> (prev % 5));
        prev = c;
    }
    return h;
}

static const char *cstr(const struct oa_langprofile *p, size_t at)
{
    if (at >= p->end) return NULL;
    const unsigned char *start = p->data + at;
    const unsigned char *stop = memchr(start, 0, p->end - at);
    return stop ? (const char *)start : NULL;
}

/* ROMProfile::findSubSection/findValue traversal, bounded to the profile. */
static int find_entry(const struct oa_langprofile *p, size_t parent,
                      const char *name, int section, size_t *out)
{
    uint32_t count;
    if (rd32(p, parent + 8, &count) != 0) return EINVAL;
    size_t end = parent + (size_t)count * 20;
    if (end < parent || end > p->size || end - parent > p->size - parent)
        return EINVAL;
    uint32_t wanted = key_hash(name);
    for (size_t at = parent + 20; at <= end;) {
        if (at > p->size || p->size - at < 20) return EINVAL;
        unsigned kind = p->data[at + 4];
        uint32_t id, n;
        if (rd32(p, at, &id) != 0) return EINVAL;
        if ((kind == 1) == section && id == wanted) {
            *out = at;
            return 0;
        }
        if (rd32(p, at + 8, &n) != 0) return EINVAL;
        size_t next = at + (kind == 1 ? 20u * ((size_t)n + 1) : 20u);
        if (next <= at || next > end + 20) return EINVAL;
        at = next;
    }
    return ENOENT;
}

int oa_langprofile_open(struct oa_langprofile **out, const char *path)
{
    if (!out || !path) return EINVAL;
    *out = NULL;
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return errno;
    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0x40 ||
        (uint64_t)st.st_size > PROFILE_MAX) {
        close(fd);
        return EINVAL;
    }
    struct oa_langprofile *p = calloc(1, sizeof *p);
    if (!p) {
        close(fd);
        return ENOMEM;
    }
    p->size = (size_t)st.st_size;
    p->data = malloc(p->size);
    if (!p->data) {
        free(p);
        close(fd);
        return ENOMEM;
    }
    size_t got = 0;
    while (got < p->size) {
        ssize_t n = read(fd, p->data + got, p->size - got);
        if (n <= 0) {
            close(fd);
            oa_langprofile_close(p);
            return EIO;
        }
        got += (size_t)n;
    }
    close(fd);
    if (memcmp(p->data, "EFSL", 4) != 0) {
        oa_langprofile_close(p);
        return EINVAL;
    }
    uint32_t root;
    if (rd32(p, 0x20, &root) != 0 || root != 0) {
        oa_langprofile_close(p);
        return EINVAL;
    }
    size_t section = 0, value = 0;
    if (find_entry(p, 0x30, "Languages", 1, &section) != 0 ||
        find_entry(p, section, "Dictionary", 0, &value) != 0 ||
        p->data[value + 4] != 4) {
        oa_langprofile_close(p);
        return EINVAL;
    }
    uint32_t base, length, languages, records;
    if (rd32(p, value + 12, &base) != 0 || rd32(p, value + 8, &length) != 0 ||
        rd32(p, base, &languages) != 0 ||
        rd32(p, (size_t)base + 4, &records) != 0) {
        oa_langprofile_close(p);
        return EINVAL;
    }
    p->base = base;
    p->length = length;
    p->end = (size_t)base + length;
    if (p->end < p->base || p->end > p->size ||
        languages == 0 || languages > LOCALE_MAX ||
        records == 0 || records > RECORD_MAX ||
        (size_t)records > (p->end - p->base) / 8) {
        oa_langprofile_close(p);
        return EINVAL;
    }
    p->locales = languages;
    p->records = records;
    p->codes = calloc(languages, sizeof *p->codes);
    p->index = calloc(records, sizeof *p->index);
    if (!p->codes || !p->index) {
        oa_langprofile_close(p);
        return ENOMEM;
    }
    size_t at = p->base + 8 + (size_t)records * 8;
    for (unsigned i = 0; i < languages; i++) {
        const char *code = cstr(p, at);
        if (!code) {
            oa_langprofile_close(p);
            return EINVAL;
        }
        p->codes[i] = code;
        at += strlen(code) + 1;
        for (int part = 0; part < 2; part++) {
            const char *extra = cstr(p, at);
            if (!extra) {
                oa_langprofile_close(p);
                return EINVAL;
            }
            at += strlen(extra) + 1;
        }
    }
    p->strings_base = at;
    uint32_t previous = 0;
    for (unsigned i = 0; i < records; i++) {
        uint32_t hash, offset;
        size_t entry = p->base + 8 + (size_t)i * 8;
        if (rd32(p, entry, &hash) != 0 || rd32(p, entry + 4, &offset) != 0 ||
            (i && hash < previous) ||
            (size_t)offset >= p->length ||
            p->strings_base + offset >= p->end) {
            oa_langprofile_close(p);
            return EINVAL;
        }
        previous = hash;
        p->index[i].hash = hash;
        p->index[i].offset = offset;
    }
    *out = p;
    return 0;
}

void oa_langprofile_close(struct oa_langprofile *p)
{
    if (!p) return;
    free(p->data);
    free(p->codes);
    free(p->index);
    free(p);
}

unsigned oa_langprofile_locale_count(const struct oa_langprofile *p)
{
    return p ? p->locales : 0;
}

const char *oa_langprofile_locale(const struct oa_langprofile *p,
                                  unsigned index)
{
    return p && index < p->locales ? p->codes[index] : NULL;
}

int oa_langprofile_locale_index(const struct oa_langprofile *p,
                                const char *code)
{
    if (!p || !code) return -1;
    for (unsigned i = 0; i < p->locales; i++)
        if (!strcmp(p->codes[i], code)) return (int)i;
    return -1;
}

static int formatting_control(unsigned cp)
{
    return cp == 0x200e || cp == 0x200f ||
           (cp >= 0x202a && cp <= 0x202e) ||
           (cp >= 0x2066 && cp <= 0x2069);
}

int oa_langprofile_sanitize(const char *in, char *out, size_t cap)
{
    if (!in || !out || cap == 0) return EINVAL;
    const unsigned char *p = (const unsigned char *)in;
    size_t o = 0;
    while (*p) {
        size_t n = 1;
        unsigned cp = *p;
        if ((*p & 0xe0) == 0xc0 && (p[1] & 0xc0) == 0x80) {
            n = 2;
            cp = ((unsigned)(p[0] & 0x1f) << 6) | (p[1] & 0x3f);
        } else if ((*p & 0xf0) == 0xe0 && (p[1] & 0xc0) == 0x80 &&
                   (p[2] & 0xc0) == 0x80) {
            n = 3;
            cp = ((unsigned)(p[0] & 0x0f) << 12) |
                 ((unsigned)(p[1] & 0x3f) << 6) | (p[2] & 0x3f);
        } else if ((*p & 0xf8) == 0xf0 && (p[1] & 0xc0) == 0x80 &&
                   (p[2] & 0xc0) == 0x80 && (p[3] & 0xc0) == 0x80) {
            n = 4;
            cp = ((unsigned)(p[0] & 0x07) << 18) |
                 ((unsigned)(p[1] & 0x3f) << 12) |
                 ((unsigned)(p[2] & 0x3f) << 6) | (p[3] & 0x3f);
        }
        if (!formatting_control(cp)) {
            if (o + n + 1 > cap) return EOVERFLOW;
            memcpy(out + o, p, n);
            o += n;
        }
        p += n;
    }
    out[o] = '\0';
    return 0;
}

const char *oa_langprofile_string(const struct oa_langprofile *p,
                                  const char *key, const char *locale)
{
    if (!p || !key || !key[0]) return key ? key : "";
    int want = oa_langprofile_locale_index(p, locale);
    if (want < 0) return key;
    uint32_t hash = key_hash(key);
    unsigned lo = 0, hi = p->records;
    while (lo < hi) {
        unsigned mid = lo + (hi - lo) / 2;
        if (p->index[mid].hash < hash) lo = mid + 1;
        else hi = mid;
    }
    if (lo >= p->records || p->index[lo].hash != hash) return key;
    int eng = oa_langprofile_locale_index(p, "eng");
    const char *wanted = NULL, *english = NULL;
    size_t at = p->strings_base + p->index[lo].offset;
    for (unsigned i = 0; i < p->locales; i++) {
        const char *text = cstr(p, at);
        if (!text) return key;
        if ((int)i == want) wanted = text;
        if ((int)i == eng) english = text;
        at += strlen(text) + 1;
    }
    if (wanted && wanted[0]) return wanted;
    if (english && english[0]) return english;
    return key;
}
