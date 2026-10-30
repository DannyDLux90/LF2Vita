/* LF2 Stage broad-phase candidate prototype, HOST TESTED but NOT integrated.
 *
 * This header accepts exact world-space rectangles computed by the caller's
 * existing region_world()/object_region_world() implementation. It only
 * excludes fighter pairs whose entire offensive region and entire victim
 * body region are disjoint. All actual ITR, BDY, vrest, arest, catch, weapon,
 * team, zwidth and RNG logic MUST remain in apply_hits in the same order.
 *
 * IMPORTANT: A complete attacker region includes all active fighter-contact
 * ITRs and all possible attacking held-weapon regions; omit none. Recompute
 * bounds when a frame, position, facing, weapon, or other region changes.
 * If unsure whether bounds are complete, set complete=0 to fail open.
 */
#ifndef LF2_STAGE_PAIR_BOUNDS_H
#define LF2_STAGE_PAIR_BOUNDS_H

#include <stdint.h>

typedef struct lf2_pair_bounds {
    int64_t x0, y0, x1, y1; /* half-open, 64-bit to prevent overflow */
    unsigned has_region;
    unsigned complete;
} lf2_pair_bounds_t;

static inline lf2_pair_bounds_t lf2_pair_bounds_begin(void) {
    lf2_pair_bounds_t b={0,0,0,0,0,1};
    return b;
}

/* Called with the EXACT integer rectangle returned by region_world. */
static inline void lf2_pair_bounds_add(
    lf2_pair_bounds_t *b, int x, int y, int w, int h)
{
    if (!b) return;
    /* Unusual DAT values require the unoptimized engine as oracle. */
    if (w <= 0 || h <= 0) { b->complete=0; return; }
    const int64_t x0=x, y0=y, x1=(int64_t)x+w, y1=(int64_t)y+h;
    if (!b->has_region) {
        b->x0=x0; b->y0=y0; b->x1=x1; b->y1=y1;
        b->has_region=1;
    } else {
        if (x0<b->x0) b->x0=x0;
        if (y0<b->y0) b->y0=y0;
        if (x1>b->x1) b->x1=x1;
        if (y1>b->y1) b->y1=y1;
    }
}

/* Conservative only: true => do the full apply_hits test unchanged.
 * false => no positive-width axis-aligned ITR/BDY pair can overlap.
 * Incomplete/empty bounds deliberately fall back to the normal engine.
 */
static inline int lf2_pair_may_contact(
    const lf2_pair_bounds_t *offense, const lf2_pair_bounds_t *body)
{
    if (!offense || !body || !offense->complete || !body->complete ||
        !offense->has_region || !body->has_region)
        return 1;
    return !(offense->x1<=body->x0 || body->x1<=offense->x0 ||
             offense->y1<=body->y0 || body->y1<=offense->y0);
}
#endif
