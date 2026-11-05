#include "openaurora/json.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct parser {
    const char *s;
    size_t pos;
    char *err;
    size_t errcap;
    int failed;
};

static void fail(struct parser *p, const char *what)
{
    if (!p->failed && p->err && p->errcap)
        snprintf(p->err, p->errcap, "%s at byte %zu", what, p->pos);
    p->failed = 1;
}

static void skip_ws(struct parser *p)
{
    while (p->s[p->pos] && isspace((unsigned char)p->s[p->pos]))
        p->pos++;
}

static struct oa_json *value(struct parser *p);

static struct oa_json *alloc_value(enum oa_json_type type)
{
    struct oa_json *v = calloc(1, sizeof *v);
    if (v) v->type = type;
    return v;
}

static int hex4(const char *s)
{
    int out = 0;
    for (int i = 0; i < 4; i++) {
        char c = s[i];
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return -1;
        out = out * 16 + d;
    }
    return out;
}

static int append_utf8(char *out, size_t *len, unsigned cp)
{
    if (cp <= 0x7f) {
        out[(*len)++] = (char)cp;
    } else if (cp <= 0x7ff) {
        out[(*len)++] = (char)(0xc0 | (cp >> 6));
        out[(*len)++] = (char)(0x80 | (cp & 0x3f));
    } else if (cp <= 0xffff) {
        out[(*len)++] = (char)(0xe0 | (cp >> 12));
        out[(*len)++] = (char)(0x80 | ((cp >> 6) & 0x3f));
        out[(*len)++] = (char)(0x80 | (cp & 0x3f));
    } else if (cp <= 0x10ffff) {
        out[(*len)++] = (char)(0xf0 | (cp >> 18));
        out[(*len)++] = (char)(0x80 | ((cp >> 12) & 0x3f));
        out[(*len)++] = (char)(0x80 | ((cp >> 6) & 0x3f));
        out[(*len)++] = (char)(0x80 | (cp & 0x3f));
    } else {
        return -1;
    }
    return 0;
}

static char *parse_string(struct parser *p)
{
    if (p->s[p->pos] != '"') {
        fail(p, "expected string");
        return NULL;
    }
    p->pos++;
    size_t cap = 32, len = 0;
    char *out = malloc(cap);
    if (!out) {
        fail(p, "out of memory");
        return NULL;
    }
    while (p->s[p->pos] && p->s[p->pos] != '"') {
        unsigned char c = (unsigned char)p->s[p->pos++];
        if (c < 0x20) {
            fail(p, "control character in string");
            free(out);
            return NULL;
        }
        if (c == '\\') {
            char e = p->s[p->pos++];
            switch (e) {
            case '"': c = '"'; break;
            case '\\': c = '\\'; break;
            case '/': c = '/'; break;
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case 'n': c = '\n'; break;
            case 'r': c = '\r'; break;
            case 't': c = '\t'; break;
            case 'u': {
                int cp = hex4(p->s + p->pos);
                if (cp < 0) {
                    fail(p, "bad \\u escape");
                    free(out);
                    return NULL;
                }
                p->pos += 4;
                /* Combine a UTF-16 surrogate pair when present. */
                if (cp >= 0xd800 && cp <= 0xdbff && p->s[p->pos] == '\\' &&
                    p->s[p->pos + 1] == 'u') {
                    int low = hex4(p->s + p->pos + 2);
                    if (low >= 0xdc00 && low <= 0xdfff) {
                        cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
                        p->pos += 6;
                    }
                }
                if (len + 8 >= cap) {
                    size_t ncap = cap * 2 + 8;
                    char *n = realloc(out, ncap);
                    if (!n) { fail(p, "out of memory"); free(out); return NULL; }
                    out = n;
                    cap = ncap;
                }
                if (append_utf8(out, &len, (unsigned)cp) != 0) {
                    fail(p, "invalid unicode code point");
                    free(out);
                    return NULL;
                }
                continue;
            }
            default:
                fail(p, "bad escape");
                free(out);
                return NULL;
            }
        }
        if (len + 2 >= cap) {
            size_t ncap = cap * 2;
            char *n = realloc(out, ncap);
            if (!n) { fail(p, "out of memory"); free(out); return NULL; }
            out = n;
            cap = ncap;
        }
        out[len++] = (char)c;
    }
    if (p->s[p->pos] != '"') {
        fail(p, "unterminated string");
        free(out);
        return NULL;
    }
    p->pos++;
    out[len] = '\0';
    return out;
}

static struct oa_json *parse_array(struct parser *p)
{
    struct oa_json *a = alloc_value(OA_JSON_ARRAY);
    if (!a) { fail(p, "out of memory"); return NULL; }
    p->pos++; /* [ */
    skip_ws(p);
    if (p->s[p->pos] == ']') {
        p->pos++;
        return a;
    }
    for (;;) {
        struct oa_json *v = value(p);
        if (!v) { oa_json_free(a); return NULL; }
        struct oa_json **n = realloc(a->items, (a->count + 1) * sizeof *n);
        if (!n) { oa_json_free(v); oa_json_free(a); fail(p, "out of memory"); return NULL; }
        a->items = n;
        a->items[a->count++] = v;
        skip_ws(p);
        if (p->s[p->pos] == ',') { p->pos++; continue; }
        if (p->s[p->pos] == ']') { p->pos++; return a; }
        fail(p, "expected ',' or ']'");
        oa_json_free(a);
        return NULL;
    }
}

static struct oa_json *parse_object(struct parser *p)
{
    struct oa_json *o = alloc_value(OA_JSON_OBJECT);
    if (!o) { fail(p, "out of memory"); return NULL; }
    p->pos++; /* { */
    skip_ws(p);
    if (p->s[p->pos] == '}') {
        p->pos++;
        return o;
    }
    for (;;) {
        skip_ws(p);
        char *key = parse_string(p);
        if (!key) { oa_json_free(o); return NULL; }
        skip_ws(p);
        if (p->s[p->pos] != ':') {
            free(key);
            fail(p, "expected ':'");
            oa_json_free(o);
            return NULL;
        }
        p->pos++;
        struct oa_json *v = value(p);
        if (!v) { free(key); oa_json_free(o); return NULL; }
        char **keys = realloc(o->keys, (o->count + 1) * sizeof *keys);
        struct oa_json **vals = realloc(o->values, (o->count + 1) * sizeof *vals);
        if (!keys || !vals) {
            free(key);
            oa_json_free(v);
            oa_json_free(o);
            fail(p, "out of memory");
            return NULL;
        }
        o->keys = keys;
        o->values = vals;
        o->keys[o->count] = key;
        o->values[o->count] = v;
        o->count++;
        skip_ws(p);
        if (p->s[p->pos] == ',') { p->pos++; continue; }
        if (p->s[p->pos] == '}') { p->pos++; return o; }
        fail(p, "expected ',' or '}'");
        oa_json_free(o);
        return NULL;
    }
}

static struct oa_json *value(struct parser *p)
{
    skip_ws(p);
    char c = p->s[p->pos];
    if (c == '{') return parse_object(p);
    if (c == '[') return parse_array(p);
    if (c == '"') {
        char *s = parse_string(p);
        if (!s) return NULL;
        struct oa_json *v = alloc_value(OA_JSON_STRING);
        if (!v) { free(s); fail(p, "out of memory"); return NULL; }
        v->string = s;
        return v;
    }
    if (!strncmp(p->s + p->pos, "true", 4)) {
        p->pos += 4;
        struct oa_json *v = alloc_value(OA_JSON_BOOL);
        if (!v) { fail(p, "out of memory"); return NULL; }
        v->boolean = 1;
        return v;
    }
    if (!strncmp(p->s + p->pos, "false", 5)) {
        p->pos += 5;
        struct oa_json *v = alloc_value(OA_JSON_BOOL);
        if (!v) { fail(p, "out of memory"); return NULL; }
        return v;
    }
    if (!strncmp(p->s + p->pos, "null", 4)) {
        p->pos += 4;
        struct oa_json *v = alloc_value(OA_JSON_NULL);
        if (!v) fail(p, "out of memory");
        return v;
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
        char *end = NULL;
        double d = strtod(p->s + p->pos, &end);
        if (end == p->s + p->pos || !isfinite(d)) {
            fail(p, "invalid number");
            return NULL;
        }
        p->pos = (size_t)(end - p->s);
        struct oa_json *v = alloc_value(OA_JSON_NUMBER);
        if (!v) { fail(p, "out of memory"); return NULL; }
        v->number = d;
        return v;
    }
    fail(p, "expected value");
    return NULL;
}

struct oa_json *oa_json_parse(const char *text, char *err, size_t errcap)
{
    struct parser p = { text, 0, err, errcap, 0 };
    if (!text) {
        if (err && errcap) snprintf(err, errcap, "null input");
        return NULL;
    }
    if (err && errcap) err[0] = '\0';
    struct oa_json *v = value(&p);
    if (!v) return NULL;
    skip_ws(&p);
    if (p.s[p.pos]) {
        fail(&p, "trailing data");
        oa_json_free(v);
        return NULL;
    }
    return v;
}

void oa_json_free(struct oa_json *v)
{
    if (!v) return;
    free(v->string);
    for (size_t i = 0; i < v->count; i++) {
        if (v->type == OA_JSON_OBJECT && v->keys) free(v->keys[i]);
        if (v->items) oa_json_free(v->items[i]);
        if (v->values) oa_json_free(v->values[i]);
    }
    free(v->items);
    free(v->keys);
    free(v->values);
    free(v);
}

const struct oa_json *oa_json_get(const struct oa_json *o, const char *key)
{
    if (!o || o->type != OA_JSON_OBJECT || !key) return NULL;
    for (size_t i = 0; i < o->count; i++)
        if (!strcmp(o->keys[i], key)) return o->values[i];
    return NULL;
}

const char *oa_json_string(const struct oa_json *o, const char *key,
                           const char *fallback)
{
    const struct oa_json *v = oa_json_get(o, key);
    return v && v->type == OA_JSON_STRING ? v->string : fallback;
}

double oa_json_number(const struct oa_json *o, const char *key, double fallback)
{
    const struct oa_json *v = oa_json_get(o, key);
    return v && v->type == OA_JSON_NUMBER ? v->number : fallback;
}

int oa_json_int(const struct oa_json *o, const char *key, int fallback)
{
    double d = oa_json_number(o, key, fallback);
    if (d < -2147483648.0 || d > 2147483647.0) return fallback;
    return (int)d;
}
