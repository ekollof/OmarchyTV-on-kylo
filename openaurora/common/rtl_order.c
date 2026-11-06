/* Visual-order conversion for the vendor profile's Arabic/Persian strings.
 *
 * The EFSL langprofile stores ara/per values in logical order and already in
 * Arabic presentation forms. The vendor feeds them through its RTL text
 * engine; this project renders with an LTR bitmap font, so it needs the final
 * visual order itself. The conversion is the standard RTL run layout:
 * reverse the run order, keep Latin/digit runs internally left-to-right, and
 * reverse each Arabic run. Bracket mirroring across runs is not implemented.
 * It is deliberately not applied to Hebrew, whose profile values are already
 * stored visual.
 */
#include "openaurora/langprofile.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

#define RTL_MAX_CP 256

static int is_arabic(uint32_t c)
{
    return (c - 0x600u < 0x100u) || (c - 0xfb50u < 0x2b0u) ||
           (c - 0xfe70u < 0x90u);
}

enum { DIR_NEUTRAL = -1, DIR_LTR = 0, DIR_RTL = 1 };

static int strong_dir(uint32_t c)
{
    if (is_arabic(c)) return DIR_RTL;
    if (c < 0x80u) {
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z'))
            return DIR_LTR;
        return DIR_NEUTRAL;
    }
    if (c >= 0x2000u && c <= 0x206fu) return DIR_NEUTRAL;
    return DIR_LTR;
}

static int decode(const char *in, uint32_t *cp, size_t cap, size_t *count)
{
    const unsigned char *p = (const unsigned char *)in;
    size_t n = 0;
    while (*p) {
        uint32_t c;
        size_t len;
        if ((*p & 0x80u) == 0) {
            c = *p;
            len = 1;
        } else if ((*p & 0xe0u) == 0xc0u && (p[1] & 0xc0u) == 0x80u) {
            c = ((uint32_t)(p[0] & 0x1fu) << 6) | (p[1] & 0x3fu);
            len = 2;
        } else if ((*p & 0xf0u) == 0xe0u && (p[1] & 0xc0u) == 0x80u &&
                   (p[2] & 0xc0u) == 0x80u) {
            c = ((uint32_t)(p[0] & 0x0fu) << 12) |
                ((uint32_t)(p[1] & 0x3fu) << 6) | (p[2] & 0x3fu);
            len = 3;
        } else if ((*p & 0xf8u) == 0xf0u && (p[1] & 0xc0u) == 0x80u &&
                   (p[2] & 0xc0u) == 0x80u && (p[3] & 0xc0u) == 0x80u) {
            c = ((uint32_t)(p[0] & 0x07u) << 18) |
                ((uint32_t)(p[1] & 0x3fu) << 12) |
                ((uint32_t)(p[2] & 0x3fu) << 6) | (p[3] & 0x3fu);
            len = 4;
        } else {
            return EINVAL;
        }
        if (n == cap) return EOVERFLOW;
        cp[n++] = c;
        p += len;
    }
    *count = n;
    return 0;
}

static int encode(const uint32_t *cp, size_t count, char *out, size_t cap)
{
    size_t o = 0;
    for (size_t i = 0; i < count; i++) {
        uint32_t c = cp[i];
        if (c < 0x80u) {
            if (o + 2 > cap) return EOVERFLOW;
            out[o++] = (char)c;
        } else if (c < 0x800u) {
            if (o + 3 > cap) return EOVERFLOW;
            out[o++] = (char)(0xc0u | (c >> 6));
            out[o++] = (char)(0x80u | (c & 0x3fu));
        } else if (c < 0x10000u) {
            if (o + 4 > cap) return EOVERFLOW;
            out[o++] = (char)(0xe0u | (c >> 12));
            out[o++] = (char)(0x80u | ((c >> 6) & 0x3fu));
            out[o++] = (char)(0x80u | (c & 0x3fu));
        } else {
            if (o + 5 > cap) return EOVERFLOW;
            out[o++] = (char)(0xf0u | (c >> 18));
            out[o++] = (char)(0x80u | ((c >> 12) & 0x3fu));
            out[o++] = (char)(0x80u | ((c >> 6) & 0x3fu));
            out[o++] = (char)(0x80u | (c & 0x3fu));
        }
    }
    if (o == cap) return EOVERFLOW;
    out[o] = '\0';
    return 0;
}

int oa_langprofile_visual_order(const char *in, char *out, size_t cap)
{
    if (!in || !out || cap == 0) return EINVAL;
    uint32_t cp[RTL_MAX_CP];
    size_t count = 0;
    int rc = decode(in, cp, RTL_MAX_CP, &count);
    if (rc) return rc;
    int any_arabic = 0;
    for (size_t i = 0; i < count; i++)
        if (is_arabic(cp[i])) any_arabic = 1;
    if (!any_arabic) {
        size_t n = strlen(in);
        if (n + 1 > cap) return EOVERFLOW;
        memcpy(out, in, n + 1);
        return 0;
    }

    /* Resolve neutral runs (space/punctuation) against their neighbours.
     * The paragraph direction is RTL here, so mixed-direction neutrals join
     * the RTL side; this keeps a space next to an Arabic word when the run
     * order is reversed. */
    int dir[RTL_MAX_CP];
    for (size_t i = 0; i < count; i++) dir[i] = strong_dir(cp[i]);
    for (size_t i = 0; i < count;) {
        if (dir[i] != DIR_NEUTRAL) {
            i++;
            continue;
        }
        size_t j = i;
        while (j < count && dir[j] == DIR_NEUTRAL) j++;
        int before = i ? dir[i - 1] : DIR_RTL;
        int after = j < count ? dir[j] : DIR_RTL;
        int resolved = before == after ? before : DIR_RTL;
        for (size_t k = i; k < j; k++) dir[k] = resolved;
        i = j;
    }

    /* runs[k] is the inclusive end index; rtl[k] is its direction. */
    size_t runs[RTL_MAX_CP], rtl[RTL_MAX_CP], nruns = 0;
    int prev = dir[0];
    for (size_t i = 1; i < count; i++) {
        int cur = dir[i];
        if (cur != prev) {
            runs[nruns] = i - 1;
            rtl[nruns] = (size_t)prev;
            nruns++;
            prev = cur;
        }
    }
    runs[nruns] = count - 1;
    rtl[nruns] = (size_t)prev;
    nruns++;

    uint32_t visual[RTL_MAX_CP];
    size_t o = 0;
    for (size_t k = nruns; k-- > 0;) {
        size_t begin = k == 0 ? 0 : runs[k - 1] + 1;
        size_t end = runs[k]; /* inclusive */
        if (rtl[k]) {
            for (size_t i = end + 1; i-- > begin;)
                visual[o++] = cp[i];
        } else {
            for (size_t i = begin; i <= end; i++)
                visual[o++] = cp[i];
        }
    }
    return encode(visual, o, out, cap);
}
