#ifndef OA_UI_NAV_TRACE_H
#define OA_UI_NAV_TRACE_H
/* Test display recorder. The shipped UI calls the display service; this
 * snapshot only reports what those calls drew. */
#define OA_SVC_NAV_TRACE "nav.trace"
typedef struct {
    int (*accent)(int *x, int *y, int *w, int *h);
    int (*counts)(int *clears, int *presents, int *surface_w, int *surface_h);
    int (*fill_count)(void);
    int (*fill_at)(int i, int *x, int *y, int *w, int *h, int *r, int *g, int *b);
    /* Text-slide tests need the title blit's x and its first source pixel. Additive: existing
     * users ignore them. */
    int (*blit_count)(void);
    int (*blit_at)(int i, int *x, int *y, int *w, int *h);
    int (*blit_first)(int i, int *r, int *g, int *b, int *a);
    /* Scaled blits (the rail highlight uses source-to-dest scaling). */
    int (*scaled_count)(void);
    int (*scaled_at)(int i, int *x, int *y, int *w, int *h, int *sw, int *sh);
    int (*scaled_first)(int i, int *r, int *g, int *b, int *a);
    /* Last full-screen scene transform (display v2): type, progress, call count. */
    int (*transform)(int *type, float *progress, int *calls);
} oa_nav_trace;
#endif
