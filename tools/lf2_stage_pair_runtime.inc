/* Stage candidate prefilter for LF2 Vita Round-27 or later.
 *
 * This is intentionally kept as an opt-in source integration candidate:
 * requires the real fighter_t, lf2_frame_def_t, frame_def() and
 * region_world() from src/game.c to already be defined. Insert ONLY at the
 * top-level boundary after game_15.inc / #undef apply_hits.
 *
 * This stage-only filter is not a substitute for apply_hits(). It determines
 * whether it is safe to avoid calling apply_hits for a specific pair.
 *
 * IMPORTANT: Every call recalculates world bounds from CURRENT fighter state.
 * A hit may immediately alter frame/position/facing and therefore invalidates
 * cached geometry. This conservative no-cache design prioritizes correctness.
 */
#ifndef LF2_STAGE_PAIR_RUNTIME_INCLUDED
#define LF2_STAGE_PAIR_RUNTIME_INCLUDED

#include <math.h>
#include <limits.h>
#include "lf2_stage_pair_bounds.h"

#ifndef LF2_STAGE_BROADPHASE_ENABLED
#define LF2_STAGE_BROADPHASE_ENABLED 0
#endif

static inline int lf2_stage_runtime_contact_itr_kind(const lf2_itr_def_t *it) {
    if (!it) return 0;
    switch (it->kind) {
    case 0: case 4: case 5:
    case 10: case 11: case 15: case 16:
        return 1;
    case 1: case 3:
        return it->catchingact>0 || it->caughtact>0;
    default:
        return 0;
    }
}

#if LF2_STAGE_BROADPHASE_ENABLED
/* The production region_world() computes integer x/y using several signed
 * additions/subtractions. Reject impossible/overflowing authored coordinates
 * BEFORE calling that function, to avoid undefined signed integer overflow.
 * The exact expression mirrors the world-space rule in game_06.inc. */
static inline int lf2_stage_region_is_int_safe(
    const fighter_t *f, const lf2_frame_def_t *frame,
    int rx,int ry,int rw,int rh)
{
    if (!f || !frame || rw<=0 || rh<=0 ||
        !isfinite(f->x) || !isfinite(f->y) ||
        fabsf(f->x)>1000000.f || fabsf(f->y)>1000000.f)
        return 0;
    const int64_t px=(int)f->x,py=(int)f->y;
    /* Mirror the original int expression one operation at a time.
       Testing only the final 64-bit result is NOT sufficient: an
       intermediate int may overflow and later cancel back into range. */
    int64_t wx=px;
    if (!f->facing) {
        wx-=(int64_t)frame->centerx;
        if (wx<INT_MIN || wx>INT_MAX) return 0;
        wx+=(int64_t)rx;
        if (wx<INT_MIN || wx>INT_MAX) return 0;
    } else {
        wx+=1;
        if (wx<INT_MIN || wx>INT_MAX) return 0;
        wx+=(int64_t)frame->centerx;
        if (wx<INT_MIN || wx>INT_MAX) return 0;
        wx-=(int64_t)rw;
        if (wx<INT_MIN || wx>INT_MAX) return 0;
        wx-=(int64_t)rx;
        if (wx<INT_MIN || wx>INT_MAX) return 0;
    }
    int64_t wy=py-(int64_t)frame->centery;
    if (wy<INT_MIN || wy>INT_MAX) return 0;
    wy+=(int64_t)ry;
    if (wy<INT_MIN || wy>INT_MAX) return 0;
    return wx+(int64_t)rw<=INT_MAX &&
           wy+(int64_t)rh<=INT_MAX;
}

static inline int lf2_stage_pair_runtime_may_contact(
    fighter_t *a, fighter_t *b)
{
    /* Fail open on unknown state, held weapons, out-of-range positions.
       apply_held_weapon_hit() precedes normal fighter ITR resolution. */
    if (!a || !b || a->held_oid ||
        !isfinite(a->x) || !isfinite(a->y) ||
        !isfinite(b->x) || !isfinite(b->y) ||
        fabsf(a->x)>1000000.f || fabsf(b->x)>1000000.f ||
        fabsf(a->y)>1000000.f || fabsf(b->y)>1000000.f)
        return 1;

    lf2_frame_def_t *af=frame_def(a), *bf=frame_def(b);
    if (!af || !bf)
        return 1;

    lf2_pair_bounds_t atk=lf2_pair_bounds_begin();
    lf2_pair_bounds_t body=lf2_pair_bounds_begin();
    lf2_pair_bounds_t kind6=lf2_pair_bounds_begin();
    int may_super=(a->frame==60 || a->frame==65) &&
                  a->def.frames[70].present;
    int has_normal_itr=0;

    for (int i=0;i<af->itr_count;i++) {
        lf2_itr_def_t *it=&af->itrs[i];
        if (!lf2_stage_runtime_contact_itr_kind(it)) continue;
        if (!lf2_stage_region_is_int_safe(a,af,it->x,it->y,it->w,it->h)) return 1;
        int x,y;
        region_world(a,it->x,it->y,it->w,it->h,&x,&y);
        lf2_pair_bounds_add(&atk,x,y,it->w,it->h);
        if (it->kind==0) has_normal_itr=1;
    }
    for (int i=0;i<bf->bdy_count;i++) {
        lf2_bdy_def_t *bd=&bf->bdys[i];
        if (!lf2_stage_region_is_int_safe(b,bf,bd->x,bd->y,bd->w,bd->h)) return 1;
        int x,y;
        region_world(b,bd->x,bd->y,bd->w,bd->h,&x,&y);
        lf2_pair_bounds_add(&body,x,y,bd->w,bd->h);
    }
    if (may_super && has_normal_itr) {
        for (int i=0;i<bf->itr_count;i++) {
            lf2_itr_def_t *it=&bf->itrs[i];
            if (it->kind!=6) continue;
            if (!lf2_stage_region_is_int_safe(b,bf,it->x,it->y,it->w,it->h)) return 1;
            int x,y;
            region_world(b,it->x,it->y,it->w,it->h,&x,&y);
            lf2_pair_bounds_add(&kind6,x,y,it->w,it->h);
        }
    }
    return lf2_pair_may_contact_with_passive(
        &atk,&body,(may_super && has_normal_itr)?&kind6:NULL);
}
#define LF2_STAGE_PAIR_RUNTIME_ALLOWED(a,b) \
    lf2_stage_pair_runtime_may_contact((a),(b))
#else
/* Default source installation is behavior-identical: no pair is suppressed.
   Turn on only after full Round-27 native/SDK and on-device checks. */
#define LF2_STAGE_PAIR_RUNTIME_ALLOWED(a,b) 1
#endif

#endif
