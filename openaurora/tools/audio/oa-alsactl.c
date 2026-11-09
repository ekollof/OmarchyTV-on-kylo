/* oa-alsactl — minimal ALSA control tool for the TV.
 *
 * The stock image has libasound but no amixer/alsactl. This tool links the
 * vendor library and supports:
 *   list [SUBSTR]            enumerate controls (optional name filter)
 *   get NAME [INDEX]         print one control
 *   set NAME VALUE [INDEX]   write an integer/boolean/enum control
 *   save FILE                text snapshot: name<TAB>index<TAB>type<TAB>values
 *   restore FILE             reapply integer/boolean/enum snapshot values
 *
 * It opens one card per process with --card N (default 0). Read-only unless
 * set/restore is requested. No telemetry, no network, no vendor HAL.
 */
#define _POSIX_C_SOURCE 200809L
#include <alsa/asoundlib.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static snd_ctl_t *ctl;
static int card = 0;

/* Two-pass enumeration: the first call reports the total, alloc_space makes
 * room, the second fills the used entries. A single call leaves used==0 on the
 * TV's ALSA 1.0.29 library. */
static unsigned load_ids(snd_ctl_elem_list_t **out)
{
    *out = NULL;
    snd_ctl_elem_list_t *list;
    if (snd_ctl_elem_list_malloc(&list) < 0) return 0;
    if (snd_ctl_elem_list(ctl, list) < 0) {
        snd_ctl_elem_list_free(list);
        return 0;
    }
    unsigned total = snd_ctl_elem_list_get_count(list);
    if (total && snd_ctl_elem_list_alloc_space(list, total) >= 0) {
        snd_ctl_elem_list_set_offset(list, 0);
        (void)snd_ctl_elem_list(ctl, list);
    }
    *out = list;
    return snd_ctl_elem_list_get_used(list);
}

static int control_by_name(const char *name, unsigned index,
                           snd_ctl_elem_id_t **out)
{
    snd_ctl_elem_list_t *list = NULL;
    unsigned count = load_ids(&list);
    if (!list) return -1;
    for (unsigned i = 0; i < count; i++) {
        snd_ctl_elem_id_t *id;
        if (snd_ctl_elem_id_malloc(&id) < 0) continue;
        snd_ctl_elem_list_get_id(list, i, id);
        if (!strcmp(snd_ctl_elem_id_get_name(id), name) &&
            snd_ctl_elem_id_get_index(id) == index) {
            snd_ctl_elem_list_free(list);
            *out = id;
            return 0;
        }
        snd_ctl_elem_id_free(id);
    }
    snd_ctl_elem_list_free(list);
    return -1;
}

static const char *type_name(snd_ctl_elem_type_t type)
{
    switch (type) {
    case SND_CTL_ELEM_TYPE_BOOLEAN: return "bool";
    case SND_CTL_ELEM_TYPE_INTEGER: return "int";
    case SND_CTL_ELEM_TYPE_ENUMERATED: return "enum";
    case SND_CTL_ELEM_TYPE_BYTES: return "bytes";
    case SND_CTL_ELEM_TYPE_IEC958: return "iec958";
    case SND_CTL_ELEM_TYPE_INTEGER64: return "int64";
    default: return "?";
    }
}

static int print_control(snd_ctl_elem_id_t *id, int verbose)
{
    snd_ctl_elem_info_t *info;
    snd_ctl_elem_value_t *val;
    if (snd_ctl_elem_info_malloc(&info) < 0) return -1;
    if (snd_ctl_elem_value_malloc(&val) < 0) { snd_ctl_elem_info_free(info); return -1; }
    snd_ctl_elem_info_set_id(info, id);
    snd_ctl_elem_value_set_id(val, id);
    int rc = snd_ctl_elem_info(ctl, info);
    if (rc >= 0) rc = snd_ctl_elem_read(ctl, val);
    if (rc < 0) {
        fprintf(stderr, "read %s: %s\n", snd_ctl_elem_id_get_name(id),
                snd_strerror(rc));
        snd_ctl_elem_info_free(info);
        snd_ctl_elem_value_free(val);
        return -1;
    }
    snd_ctl_elem_type_t type = snd_ctl_elem_info_get_type(info);
    unsigned count = snd_ctl_elem_info_get_count(info);
    printf("%s\t%u\t%s\t%u", snd_ctl_elem_id_get_name(id),
           snd_ctl_elem_id_get_index(id), type_name(type), count);
    if (type == SND_CTL_ELEM_TYPE_INTEGER ||
        type == SND_CTL_ELEM_TYPE_BOOLEAN) {
        for (unsigned i = 0; i < count; i++)
            printf("\t%ld", snd_ctl_elem_value_get_integer(val, i));
        if (verbose)
            printf("\tmin=%ld\tmax=%ld",
                   snd_ctl_elem_info_get_min(info),
                   snd_ctl_elem_info_get_max(info));
    } else if (type == SND_CTL_ELEM_TYPE_ENUMERATED) {
        for (unsigned i = 0; i < count; i++) {
            unsigned v = snd_ctl_elem_value_get_enumerated(val, i);
            const char *label = NULL;
            /* The vendor driver fills the item-name table only when info is
             * read again after set_item. Calling get_item_name without the
             * second info call returns item 0 for every index. */
            snd_ctl_elem_info_set_item(info, v);
            if (snd_ctl_elem_info(ctl, info) >= 0)
                label = snd_ctl_elem_info_get_item_name(info);
            if (label && *label)
                printf("\t%u:%s", v, label);
            else
                printf("\t%u", v);
        }
    } else if (type == SND_CTL_ELEM_TYPE_IEC958) {
        snd_aes_iec958_t iec;
        snd_ctl_elem_value_get_iec958(val, &iec);
        printf("\tstatus=%02x%02x%02x%02x", iec.status[0], iec.status[1],
               iec.status[2], iec.status[3]);
    } else if (type == SND_CTL_ELEM_TYPE_BYTES) {
        const unsigned char *b = snd_ctl_elem_value_get_bytes(val);
        for (unsigned i = 0; i < count; i++) printf(" %02x", b[i]);
    } else if (type == SND_CTL_ELEM_TYPE_INTEGER64) {
        for (unsigned i = 0; i < count; i++)
            printf("\t%lld", (long long)snd_ctl_elem_value_get_integer64(val, i));
    }
    putchar('\n');
    snd_ctl_elem_info_free(info);
    snd_ctl_elem_value_free(val);
    return 0;
}

static int list_controls(const char *needle, int verbose)
{
    snd_ctl_elem_list_t *list = NULL;
    unsigned count = load_ids(&list);
    if (!list) return -1;
    for (unsigned i = 0; i < count; i++) {
        snd_ctl_elem_id_t *id;
        if (snd_ctl_elem_id_malloc(&id) < 0) continue;
        snd_ctl_elem_list_get_id(list, i, id);
        if (!needle || strstr(snd_ctl_elem_id_get_name(id), needle))
            (void)print_control(id, verbose);
        snd_ctl_elem_id_free(id);
    }
    snd_ctl_elem_list_free(list);
    return 0;
}

static int get_control(const char *name, unsigned index)
{
    snd_ctl_elem_id_t *id = NULL;
    if (control_by_name(name, index, &id) < 0) return -1;
    int rc = print_control(id, 1);
    snd_ctl_elem_id_free(id);
    return rc;
}

static int set_control(const char *name, unsigned index, long value)
{
    snd_ctl_elem_id_t *id = NULL;
    if (control_by_name(name, index, &id) < 0) return -1;
    snd_ctl_elem_info_t *info;
    snd_ctl_elem_value_t *val;
    if (snd_ctl_elem_info_malloc(&info) < 0 ||
        snd_ctl_elem_value_malloc(&val) < 0) return -1;
    snd_ctl_elem_info_set_id(info, id);
    snd_ctl_elem_value_set_id(val, id);
    int rc = snd_ctl_elem_info(ctl, info);
    if (rc >= 0) {
        snd_ctl_elem_type_t type = snd_ctl_elem_info_get_type(info);
        unsigned count = snd_ctl_elem_info_get_count(info);
        if (type == SND_CTL_ELEM_TYPE_INTEGER ||
            type == SND_CTL_ELEM_TYPE_BOOLEAN) {
            for (unsigned i = 0; i < count; i++)
                snd_ctl_elem_value_set_integer(val, i, value);
        } else if (type == SND_CTL_ELEM_TYPE_ENUMERATED) {
            for (unsigned i = 0; i < count; i++)
                snd_ctl_elem_value_set_enumerated(val, i, (unsigned)value);
        } else {
            rc = -ENOTSUP;
        }
    }
    if (rc >= 0) rc = snd_ctl_elem_write(ctl, val);
    snd_ctl_elem_info_free(info);
    snd_ctl_elem_value_free(val);
    snd_ctl_elem_id_free(id);
    return rc;
}

static int set_values(const char *name, unsigned index,
                      const long *values, unsigned nvalues)
{
    snd_ctl_elem_id_t *id = NULL;
    if (control_by_name(name, index, &id) < 0) return -1;
    snd_ctl_elem_info_t *info;
    snd_ctl_elem_value_t *val;
    if (snd_ctl_elem_info_malloc(&info) < 0 ||
        snd_ctl_elem_value_malloc(&val) < 0) {
        snd_ctl_elem_info_free(info);
        snd_ctl_elem_id_free(id);
        return -1;
    }
    snd_ctl_elem_info_set_id(info, id);
    snd_ctl_elem_value_set_id(val, id);
    int rc = snd_ctl_elem_info(ctl, info);
    if (rc >= 0) {
        snd_ctl_elem_type_t type = snd_ctl_elem_info_get_type(info);
        unsigned count = snd_ctl_elem_info_get_count(info);
        if (nvalues > count) {
            rc = -EINVAL;
        } else if (type != SND_CTL_ELEM_TYPE_INTEGER &&
                   type != SND_CTL_ELEM_TYPE_BOOLEAN &&
                   type != SND_CTL_ELEM_TYPE_ENUMERATED) {
            rc = -ENOTSUP;
        } else if ((rc = snd_ctl_elem_read(ctl, val)) >= 0) {
            for (unsigned i = 0; i < nvalues; i++) {
                if (type == SND_CTL_ELEM_TYPE_ENUMERATED)
                    snd_ctl_elem_value_set_enumerated(val, i, (unsigned)values[i]);
                else
                    snd_ctl_elem_value_set_integer(val, i, values[i]);
            }
            rc = snd_ctl_elem_write(ctl, val);
        }
    }
    snd_ctl_elem_info_free(info);
    snd_ctl_elem_value_free(val);
    snd_ctl_elem_id_free(id);
    return rc;
}

static int set_bytes_control(const char *name, unsigned index,
                             const unsigned char *data, unsigned n)
{
    snd_ctl_elem_id_t *id = NULL;
    if (control_by_name(name, index, &id) < 0) return -1;
    snd_ctl_elem_value_t *val;
    if (snd_ctl_elem_value_malloc(&val) < 0) {
        snd_ctl_elem_id_free(id);
        return -1;
    }
    snd_ctl_elem_value_set_id(val, id);
    snd_ctl_elem_set_bytes(val, (void *)data, n);
    int rc = snd_ctl_elem_write(ctl, val);
    snd_ctl_elem_value_free(val);
    snd_ctl_elem_id_free(id);
    return rc;
}

static int save_controls(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    snd_ctl_elem_list_t *list = NULL;
    unsigned count = load_ids(&list);
    if (!list) { fclose(f); return -1; }
    for (unsigned i = 0; i < count; i++) {
        snd_ctl_elem_id_t *id;
        if (snd_ctl_elem_id_malloc(&id) < 0) continue;
        snd_ctl_elem_list_get_id(list, i, id);
        snd_ctl_elem_info_t *info;
        snd_ctl_elem_value_t *val;
        if (snd_ctl_elem_info_malloc(&info) < 0 ||
            snd_ctl_elem_value_malloc(&val) < 0) {
            snd_ctl_elem_id_free(id);
            continue;
        }
        snd_ctl_elem_info_set_id(info, id);
        snd_ctl_elem_value_set_id(val, id);
        if (snd_ctl_elem_info(ctl, info) >= 0 &&
            snd_ctl_elem_read(ctl, val) >= 0) {
            snd_ctl_elem_type_t type = snd_ctl_elem_info_get_type(info);
            unsigned n = snd_ctl_elem_info_get_count(info);
            fprintf(f, "%s\t%u\t%s\t%u", snd_ctl_elem_id_get_name(id),
                    snd_ctl_elem_id_get_index(id), type_name(type), n);
            if (type == SND_CTL_ELEM_TYPE_INTEGER ||
                type == SND_CTL_ELEM_TYPE_BOOLEAN) {
                for (unsigned k = 0; k < n; k++)
                    fprintf(f, "\t%ld", snd_ctl_elem_value_get_integer(val, k));
            } else if (type == SND_CTL_ELEM_TYPE_ENUMERATED) {
                for (unsigned k = 0; k < n; k++)
                    fprintf(f, "\t%u", snd_ctl_elem_value_get_enumerated(val, k));
            } else if (type == SND_CTL_ELEM_TYPE_BYTES) {
                const unsigned char *b = snd_ctl_elem_value_get_bytes(val);
                fputc('\t', f);
                for (unsigned k = 0; k < n; k++) fprintf(f, "%02x", b[k]);
            } else {
                fprintf(f, "\t-");
            }
            fputc('\n', f);
        }
        snd_ctl_elem_info_free(info);
        snd_ctl_elem_value_free(val);
        snd_ctl_elem_id_free(id);
    }
    snd_ctl_elem_list_free(list);
    fclose(f);
    return 0;
}

static int restore_controls(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[4096];
    int failures = 0;
    while (fgets(line, sizeof line, f)) {
        char name[256];
        unsigned index, n;
        char type[16];
        if (sscanf(line, "%255[^\t]\t%u\t%15[^\t]\t%u", name, &index, type, &n) != 4)
            continue;
        char *p = strchr(line, '\n');
        if (p) *p = 0;
        char *field = line;
        for (int k = 0; k < 4 && field; k++) {
            field = strchr(field, '\t');
            if (field) field++;
        }
        if (!field) continue;
        if (!strcmp(type, "int") || !strcmp(type, "bool") ||
            !strcmp(type, "enum")) {
            long values[64];
            unsigned got = 0;
            if (n > sizeof values / sizeof values[0]) {
                failures++;
                continue;
            }
            while (got < n) {
                char *end = NULL;
                long v = strtol(field, &end, 10);
                if (end == field) break;
                values[got++] = v;
                field = end;
                if (*field == '\t') field++;
            }
            if (got != n || set_values(name, index, values, n) < 0)
                failures++;
        } else if (!strcmp(type, "bytes")) {
            unsigned char buf[256];
            unsigned got = 0;
            if (n > sizeof buf) {
                failures++;
                continue;
            }
            while (got < n && got * 2 + 2 <= strlen(field)) {
                unsigned v;
                if (sscanf(field + got * 2, "%2x", &v) != 1) break;
                buf[got++] = (unsigned char)v;
            }
            if (got != n || set_bytes_control(name, index, buf, n) < 0)
                failures++;
        }
    }
    fclose(f);
    return failures ? -1 : 0;
}

int main(int argc, char **argv)
{
    const char *cmd;
    const char *a1 = NULL, *a2 = NULL;
    const char *rest[64];
    unsigned nrest = 0;
    int arg = 1;
    for (; arg < argc; arg++) {
        if (!strcmp(argv[arg], "--card") && arg + 1 < argc)
            card = atoi(argv[++arg]);
        else break;
    }
    if (arg >= argc) {
        fprintf(stderr, "usage: %s [--card N] list [FILTER] | get NAME [INDEX] | "
                        "set NAME VALUE | setv NAME VALUE [VALUE...] | "
                        "save FILE | restore FILE\n", argv[0]);
        return 2;
    }
    cmd = argv[arg++];
    while (arg < argc && nrest < sizeof rest / sizeof rest[0])
        rest[nrest++] = argv[arg++];
    if (nrest > 0) a1 = rest[0];
    if (nrest > 1) a2 = rest[1];
    char dev[32];
    snprintf(dev, sizeof dev, "hw:%d", card);
    int rc = snd_ctl_open(&ctl, dev, 0);
    if (rc < 0) {
        fprintf(stderr, "open %s: %s\n", dev, snd_strerror(rc));
        return 1;
    }
    if (!strcmp(cmd, "list")) rc = list_controls(a1, 0);
    else if (!strcmp(cmd, "get")) {
        if (!a1) rc = -EINVAL;
        else rc = get_control(a1, a2 ? (unsigned)atoi(a2) : 0);
    } else if (!strcmp(cmd, "set")) {
        if (!a1 || !a2) rc = -EINVAL;
        else rc = set_control(a1, 0, strtol(a2, NULL, 0));
    } else if (!strcmp(cmd, "setv")) {
        long values[64];
        if (!a1 || nrest < 2) {
            rc = -EINVAL;
        } else if (nrest - 1 > sizeof values / sizeof values[0]) {
            rc = -E2BIG;
        } else {
            unsigned n = nrest - 1;
            for (unsigned i = 0; i < n; i++)
                values[i] = strtol(rest[i + 1], NULL, 0);
            rc = set_values(a1, 0, values, n);
        }
    } else if (!strcmp(cmd, "save")) {
        if (!a1) rc = -EINVAL;
        else rc = save_controls(a1);
    } else if (!strcmp(cmd, "restore")) {
        if (!a1) rc = -EINVAL;
        else rc = restore_controls(a1);
    } else {
        rc = -EINVAL;
    }
    if (rc < 0) fprintf(stderr, "%s: %s\n", cmd, snd_strerror(rc));
    snd_ctl_close(ctl);
    return rc < 0 ? 1 : 0;
}
