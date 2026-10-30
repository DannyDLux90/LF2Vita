/* Native oracle/parity test for a FUTURE, not yet integrated LF2 Stage broad-phase.
 * Build: cc -std=c11 -O2 -Wall -Wextra -Werror -Itools \
 *           tests/native/test_stage_pair_bounds.c -o /tmp/lf2_bounds && /tmp/lf2_bounds
 */
#include "lf2_stage_pair_bounds.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ACTORS 72
#define MAX_REGIONS 4

typedef struct { int x,y,w,h; } rect_t;
typedef struct {
    int enabled, damage, hits;
    int na,nb;
    rect_t atk[MAX_REGIONS], body[MAX_REGIONS];
    lf2_pair_bounds_t aabb_atk, aabb_body;
} actor_t;

static uint32_t state=0x1833beafu;
static uint32_t rand32(void) {
    uint32_t x=state;
    x^=x<<13; x^=x>>17; x^=x<<5;
    state=x;
    return x;
}
static int randbetween(int lo,int hi) {
    return lo+(int)(rand32()%(uint32_t)(hi-lo+1));
}
static int narrow_rect_overlap(rect_t a, rect_t b) {
    const int64_t ax1=(int64_t)a.x+a.w;
    const int64_t bx1=(int64_t)b.x+b.w;
    const int64_t ay1=(int64_t)a.y+a.h;
    const int64_t by1=(int64_t)b.y+b.h;
    if (a.w<=0||a.h<=0||b.w<=0||b.h<=0) return 0;
    return (ax1>b.x && bx1>a.x && ay1>b.y && by1>a.y);
}
static void update_aabb(actor_t *a) {
    a->aabb_atk=lf2_pair_bounds_begin();
    a->aabb_body=lf2_pair_bounds_begin();
    for (int i=0;i<a->na;i++) {
        rect_t r=a->atk[i];
        lf2_pair_bounds_add(&a->aabb_atk,r.x,r.y,r.w,r.h);
    }
    for (int i=0;i<a->nb;i++) {
        rect_t r=a->body[i];
        lf2_pair_bounds_add(&a->aabb_body,r.x,r.y,r.w,r.h);
    }
}
static uint64_t mix(uint64_t h,uint64_t v) {
    return (h^v)*UINT64_C(1099511628211);
}
typedef struct {
    uint64_t hash;
    unsigned long long narrow_calls,skipped_pairs;
    unsigned events;
} run_t;

/* Reference and candidate invoke identical narrow-phase in attacker-major,
 * target-minor order; hits mutate future fighter positions/frames (represented
 * here as rectangle coordinates). The candidate MUST re-evaluate cached AABBs.
 * No LF2-specific damage, catches or special animations are simulated here. */
static run_t run_scene(actor_t *fighters,int n,int optimized) {
    run_t result={UINT64_C(14695981039346656037),0,0,0};
    for (int a=0;a<n;a++) {
        if (!fighters[a].enabled) continue;
        for (int b=0;b<n;b++) {
            if (a==b || !fighters[b].enabled) continue;
            if (optimized && !lf2_pair_may_contact(&fighters[a].aabb_atk,
                                                   &fighters[b].aabb_body)) {
                result.skipped_pairs++;
                continue;
            }
            result.narrow_calls++;
            int hit=0;
            for (int i=0;i<fighters[a].na && !hit;i++) {
                for (int j=0;j<fighters[b].nb;j++) {
                    if (!narrow_rect_overlap(fighters[a].atk[i],
                                            fighters[b].body[j])) continue;
                    result.events++;
                    result.hash=mix(result.hash,(uint64_t)(a+1)*257+(b+1));
                    result.hash=mix(result.hash,(uint64_t)(i+1)*257+(j+1));
                    fighters[a].hits++;
                    fighters[b].damage++;
                    /* Model mutable fighter regions after a successful hit. */
                    fighters[b].body[j].x+=11;
                    fighters[a].atk[i].x+=3;
                    update_aabb(&fighters[b]);
                    update_aabb(&fighters[a]);
                    hit=1;
                    break;
                }
            }
        }
    }
    return result;
}
static void check(int ok,const char *name) {
    if (!ok) { fprintf(stderr,"FAIL: %s\n",name); exit(2); }
}
static void test_bounds_corners(void) {
    lf2_pair_bounds_t a=lf2_pair_bounds_begin(),b=lf2_pair_bounds_begin();
    lf2_pair_bounds_add(&a,0,0,30,30);
    lf2_pair_bounds_add(&b,30,0,10,10);
    check(!lf2_pair_may_contact(&a,&b),"touching edges excluded");
    lf2_pair_bounds_add(&b,29,0,10,10);
    check(lf2_pair_may_contact(&a,&b),"overlapping union included");
    a=lf2_pair_bounds_begin();
    check(lf2_pair_may_contact(&a,&b),"empty region fails open");
    lf2_pair_bounds_add(&a,INT_MAX-9,INT_MAX-9,200,200);
    lf2_pair_bounds_add(&b,INT_MAX,INT_MAX,5,5);
    check(lf2_pair_may_contact(&a,&b),"large positive coordinates");
    a=lf2_pair_bounds_begin();
    b=lf2_pair_bounds_begin();
    lf2_pair_bounds_add(&a,INT_MIN,INT_MIN,200,200);
    lf2_pair_bounds_add(&b,INT_MIN+100,INT_MIN+100,5,5);
    check(lf2_pair_may_contact(&a,&b),"large negative coordinates");
    lf2_pair_bounds_add(&a,0,0,-10,20);
    check(lf2_pair_may_contact(&a,&b),"invalid dimensions fail open");
    a=lf2_pair_bounds_begin();
    b=lf2_pair_bounds_begin();
    lf2_pair_bounds_add(&a,0,0,10,10);
    lf2_pair_bounds_add(&a,900,900,20,20); /* e.g. long-range held weapon */
    lf2_pair_bounds_add(&b,905,905,20,20);
    check(lf2_pair_may_contact(&a,&b),"secondary attack region included");
    a.complete=0;
    check(lf2_pair_may_contact(&a,&b),"uncertain attacker fails open");
    check(lf2_pair_may_contact(NULL,&b),"missing attacker fails open");
}
static void random_actor(actor_t *a) {
    memset(a,0,sizeof(*a));
    a->enabled=randbetween(0,11)!=0;
    a->na=randbetween(0,MAX_REGIONS);
    a->nb=randbetween(0,MAX_REGIONS);
    int centerx=randbetween(-1800,1800);
    int centery=randbetween(-230,230);
    for (int k=0;k<a->na;k++) {
        a->atk[k]=(rect_t){centerx+randbetween(-250,250),
                           centery+randbetween(-120,120),
                           randbetween(1,260),randbetween(1,110)};
    }
    for (int k=0;k<a->nb;k++) {
        a->body[k]=(rect_t){centerx+randbetween(-60,60),
                            centery+randbetween(-80,80),
                            randbetween(1,100),randbetween(1,120)};
    }
    update_aabb(a);
    /* Some authored or runtime regions cannot be bounded confidently.
     * Candidate must NOT cull them. */
    if (randbetween(0,29)==0) a->aabb_atk.complete=0;
    if (randbetween(0,29)==0) a->aabb_body.complete=0;
}
int main(void) {
    test_bounds_corners();
    actor_t *slow=calloc(MAX_ACTORS,sizeof(*slow));
    actor_t *fast=calloc(MAX_ACTORS,sizeof(*fast));
    if(!slow||!fast){fprintf(stderr,"allocation failure\n");return 1;}
    unsigned long long pruned=0, baseline_calls=0;
    for (int scene=0;scene<1800;scene++) {
        int n=scene%3==0?8:(scene%3==1?40:72);
        for (int i=0;i<n;i++) random_actor(&slow[i]);
        memcpy(fast,slow,(size_t)n*sizeof(*slow));
        const run_t reference=run_scene(slow,n,0);
        const run_t candidate=run_scene(fast,n,1);
        if (reference.hash!=candidate.hash ||
            reference.events!=candidate.events) {
            fprintf(stderr,"PARITY FAILED scene=%d actors=%d ref=%u got=%u\n",
                    scene,n,reference.events,candidate.events);
            return 2;
        }
        for (int i=0;i<n;i++) {
            if (slow[i].damage!=fast[i].damage||
                slow[i].hits!=fast[i].hits ||
                memcmp(slow[i].body,fast[i].body,sizeof(slow[i].body))!=0||
                memcmp(slow[i].atk,fast[i].atk,sizeof(slow[i].atk))!=0) {
                fprintf(stderr,"ACTOR STATE MISMATCH scene=%d slot=%d\n",scene,i);
                return 2;
            }
        }
        pruned+=candidate.skipped_pairs;
        baseline_calls+=reference.narrow_calls;
    }
    check(pruned>0,"some disjoint candidate pairs skipped");
    printf("PASS 1800 synthetic scenes (8/40/72 fighters), identical contact event/order/state; "
           "pruned %llu/%llu potential narrow-phase calls. NOT integrated.\n",
           pruned,baseline_calls);
    free(slow);free(fast);
    return 0;
}
