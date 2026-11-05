/* Minimal bounded JSON reader for menu documents.
 *
 * Supports the JSON subset used by openaurora menu files: objects, arrays,
 * strings (with standard escapes), numbers, true/false/null. It is a DOM
 * parser, not a streaming parser; callers own the returned tree and free it
 * with oa_json_free(). No file or network access.
 */
#ifndef OPENAURORA_JSON_H
#define OPENAURORA_JSON_H

#include <stddef.h>

enum oa_json_type {
    OA_JSON_NULL,
    OA_JSON_BOOL,
    OA_JSON_NUMBER,
    OA_JSON_STRING,
    OA_JSON_ARRAY,
    OA_JSON_OBJECT,
};

struct oa_json {
    enum oa_json_type type;
    int boolean;
    double number;
    char *string;
    struct oa_json **items; /* array */
    size_t count;
    char **keys;            /* object */
    struct oa_json **values;
};

struct oa_json *oa_json_parse(const char *text, char *err, size_t errcap);
void oa_json_free(struct oa_json *value);
const struct oa_json *oa_json_get(const struct oa_json *object, const char *key);
const char *oa_json_string(const struct oa_json *object, const char *key,
                           const char *fallback);
double oa_json_number(const struct oa_json *object, const char *key,
                      double fallback);
int oa_json_int(const struct oa_json *object, const char *key, int fallback);

#endif
