#ifndef OPENAURORA_SVG_LIST_INPUT_H
#define OPENAURORA_SVG_LIST_INPUT_H
#include <stdint.h>
enum oa_svg_list_query { OA_SVG_LIST_HORIZONTAL,OA_SVG_LIST_PAGE_DOWN_KEY,OA_SVG_LIST_PAGE_UP_KEY };
enum oa_svg_list_action {
    OA_SVG_LIST_SELECT,OA_SVG_LIST_DOWN,OA_SVG_LIST_UP,OA_SVG_LIST_PAGE_DOWN,
    OA_SVG_LIST_PAGE_UP,OA_SVG_LIST_FIRST,OA_SVG_LIST_LAST,OA_SVG_LIST_WINDOW
};
struct oa_svg_list_ops {
    void *context;
    uint32_t (*query)(void *,enum oa_svg_list_query);
    uint32_t (*run)(void *,enum oa_svg_list_action,const void *message);
    const void *message;
};
/* Qualified SVGListBoxBase key/body routing. Fields/decoded keys are immutable
 * pure reads during a call. Select/layout movement precedes page aliases;
 * page-down precedes page-up, then first/last. Aliases are full-width per-widget
 * fields, not guessed global keys. FIRST/LAST/WINDOW receive the original event;
 * the other helpers have no message operand. Unknown key returns unhandled
 * without a callback. handle(type1004) invokes key once, with no WINDOW retry
 * if unhandled; other types invoke qualified UIWindow fallback with original.
 * Caller pins code/context/message and supplies permission/lifetime gates and
 * nonblocking helpers. No pointer is retained or effect inferred from result.
 * Return errno separately from normalized handled0/1. EINVAL runs no callbacks
 * or output writes. These are routers, not the movement/selection bodies. */
int oa_svg_list_key(const struct oa_svg_list_ops *,uint32_t key,int *handled);
int oa_svg_list_handle(const struct oa_svg_list_ops *,uint32_t type,uint32_t key,int *handled);
enum oa_settings_row_query {
    OA_ROW_SELECTION_ID,OA_ROW_NUMERIC,OA_ROW_CHECKED,OA_ROW_CACHE_PRIMARY,OA_ROW_CACHE_SECONDARY
};
struct oa_settings_row_ops {
    void *context;
    uint32_t (*query)(void *,enum oa_settings_row_query);
};
/* SettingItemsListBoxRow::getSelectedItemValue. Full-width type selects exactly
 * one widget getter/cache field, or zero without a getter for unknown types.
 * Returns raw32 bits; no bool/index/string normalization. Query/context/code
 * remain pinned during this synchronous pure read, and nothing is retained.
 * Missing query/output: EINVAL, no callbacks/output write. Actual widget getter
 * implementations and native object initialization remain separate concerns. */
int oa_settings_row_value(const struct oa_settings_row_ops *,uint32_t type,uint32_t *value);
#endif
