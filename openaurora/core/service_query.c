/* Deterministic stored-inventory queries. See service_db.h. */
#include "openaurora/service_db.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* qsort has no context argument; this helper is used on the control/UI loop
 * thread only. The context is set immediately before the single qsort call. */
static const struct oa_service_list *sort_list;
static int sort_mode;
static int sort_descending;

static int compare_indexes(const void *a, const void *b)
{
    size_t ia = *(const size_t *)a, ib = *(const size_t *)b;
    const struct oa_service_row *ra = &sort_list->rows[ia];
    const struct oa_service_row *rb = &sort_list->rows[ib];
    int result = 0;
    if (sort_mode == OA_SERVICE_SORT_NAME) {
        result = strcasecmp(ra->name, rb->name);
        if (!result) result = strcasecmp(ra->provider, rb->provider);
    } else if (sort_mode == OA_SERVICE_SORT_CHANNEL_NO) {
        if (ra->channel_no != rb->channel_no)
            result = ra->channel_no < rb->channel_no ? -1 : 1;
        else
            result = strcasecmp(ra->name, rb->name);
    }
    if (!result && ra->source != rb->source)
        result = ra->source < rb->source ? -1 : 1;
    if (!result && ra->id != rb->id)
        result = ra->id < rb->id ? -1 : 1;
    if (!result && ia != ib)
        result = ia < ib ? -1 : 1;
    return sort_descending ? -result : result;
}

int oa_service_list_query(const struct oa_service_list *list,
    const struct oa_service_query *query, size_t *indexes, size_t cap,
    size_t *matched)
{
    if (!list || !query || (cap && !indexes)) return -EINVAL;
    if (query->sort > OA_SERVICE_SORT_CHANNEL_NO) return -EINVAL;
    if (matched) *matched = 0;
    if (!list->count) return 0;

    size_t *all = malloc(list->count * sizeof *all);
    if (!all) return -ENOMEM;
    size_t count = 0;
    for (size_t i = 0; i < list->count; i++) {
        const struct oa_service_row *row = &list->rows[i];
        unsigned source = row->source;
        if (source > OA_SERVICE_SOURCE_ANALOG) continue;
        if (query->source_mask && !(query->source_mask & (1u << source)))
            continue;
        if (query->visible_only && !oa_service_visible(row)) continue;
        if (query->available_only && !oa_service_available(row)) continue;
        if (query->exclude_encrypted && oa_service_encrypted(row)) continue;
        if (query->exclude_missing && oa_service_not_found(row)) continue;
        all[count++] = i;
    }
    if (matched) *matched = count;
    sort_list = list;
    sort_mode = query->sort;
    sort_descending = query->descending != 0;
    if (count > 1 && query->sort != OA_SERVICE_SORT_TABLE)
        qsort(all, count, sizeof *all, compare_indexes);
    sort_list = NULL;
    size_t copy = count < cap ? count : cap;
    for (size_t i = 0; i < copy; i++) indexes[i] = all[i];
    free(all);
    return 0;
}
