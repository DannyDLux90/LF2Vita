/* Native property test for LF2 fighter region_world-style mirroring.
 * No VitaSDK or original assets required. This models only geometric
 * candidate completeness; it does NOT simulate LF2 damage or animations.
 */
#include "lf2_stage_pair_bounds.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REGION_MAX 5
#define CASES 200000

typedef struct {int x,y,w,h;} rect_t;
typedef struct {
    rect_t attack[REGION_MAX], bdy[REGION_MAX], kind6[REGION_MAX];
    int na,nb,np, can_super, held_weapon;
    lf2_pair_bounds_t attacks, bodies, passive;
} situation_t;

static uint32_t randstate=UINT32_C(0xc417cffe);
static uint32_t next_random(void) {
    uint32_t x=randstate;
    x^=x<<13;x^=x>>17;x^=x<<5;
    randstate=x;
    return x;
}
static int range(int lo,int hi) {
    return lo+(int)(next_random()%(unsigned)(hi-lo+1));
}

/* Exact x/y rule visible in historical src/fix3/game_06.inc.
 * IMPORTANT: this implementation is for host-only oracle testing.
 * A production integration must call the ACTUAL Round-27 region_world.
 */
static rect_t world_rect(float fx,float fy,int centerx,int centery,
                         int facing,int rx,int ry,int rw,int rh)
{
    rect_t r;
    r.x=!facing?(int)fx-centerx+rx:(int)fx+1+centerx-rw-rx;
    r.y=(int)fy-centery+ry;
    r.w=rw;r.h=rh;
    return r;
}
static int overlap(rect_t a,rect_t b) {
    if (a.w<=0||a.h<=0||b.w<=0||b.h<=0) return 0;
    return (int64_t)a.x+a.w>b.x && (int64_t)b.x+b.w>a.x &&
           (int64_t)a.y+a.h>b.y && (int64_t)b.y+b.h>a.y;
}
static void add(lf2_pair_bounds_t *a,rect_t r) {
    lf2_pair_bounds_add(a,r.x,r.y,r.w,r.h);
}
static void initialize(situation_t *s) {
    s->attacks=lf2_pair_bounds_begin();
    s->bodies=lf2_pair_bounds_begin();
    s->passive=lf2_pair_bounds_begin();
    float ax=(float)range(-3000,3000)+((next_random()&1)?0.5f:0.f);
    float ay=(float)range(-500,500)+((next_random()&1)?0.5f:0.f);
    float bx=ax+(float)range(-500,500);
    float by=ay+(float)range(-300,300);
    int acx=range(0,200),acy=range(0,160);
    int bcx=range(0,200),bcy=range(0,160);
    int afacing=range(0,1),bfacing=range(0,1);
    s->na=range(0,REGION_MAX);
    s->nb=range(0,REGION_MAX);
    s->np=range(0,REGION_MAX);
    s->can_super=range(0,5)==0;
    s->held_weapon=range(0,9)==0;
    for(int i=0;i<s->na;i++){
        s->attack[i]=world_rect(ax,ay,acx,acy,afacing,
                                range(-180,180),range(-130,130),
                                range(1,260),range(1,110));
        add(&s->attacks,s->attack[i]);
    }
    for(int i=0;i<s->nb;i++){
        s->bdy[i]=world_rect(bx,by,bcx,bcy,bfacing,
                             range(-120,120),range(-110,110),
                             range(1,130),range(1,120));
        add(&s->bodies,s->bdy[i]);
    }
    for(int i=0;i<s->np;i++){
        s->kind6[i]=world_rect(bx,by,bcx,bcy,bfacing,
                               range(-200,200),range(-150,150),
                               range(1,180),range(1,130));
        add(&s->passive,s->kind6[i]);
    }
    if(s->held_weapon) lf2_pair_bounds_uncertain(&s->attacks);
}
static int reference_contact(const situation_t *s) {
    if(s->held_weapon)return 1; /* unknown weapon geometry -> mandatory fallback */
    for(int a=0;a<s->na;a++){
        if(s->can_super)for(int p=0;p<s->np;p++)
            if(overlap(s->attack[a],s->kind6[p]))return 1;
        for(int b=0;b<s->nb;b++)if(overlap(s->attack[a],s->bdy[b]))
            return 1;
    }
    return 0;
}
int main(void) {
    unsigned skipped=0,possible=0,weapon_checks=0,passive_contacts=0;
    situation_t s;
    for(int i=0;i<CASES;i++){
        memset(&s,0,sizeof(s));
        initialize(&s);
        const int actual=reference_contact(&s);
        const int may=lf2_pair_may_contact_with_passive(
            &s.attacks,&s.bodies,s.can_super?&s.passive:NULL);
        if(actual&&!may) {
            fprintf(stderr,"FALSE NEGATIVE scene=%d mirrored geometry\n",i);
            return 2;
        }
        if(!may)skipped++;
        if(actual)possible++;
        if(s.held_weapon){weapon_checks++;if(!may)return 3;}
        if(s.can_super && s.np>0)passive_contacts++;
    }
    if(skipped<1000 || possible<1000 || weapon_checks<1000 || passive_contacts<1000)
        return 4;
    printf("PASS %d mirrored-world region tests, 0 false negatives, "
           "%u safely rejected; %u potential events, %u weapon fallbacks, "
           "%u kind-6 region checks. Host geometry only.\n",
           CASES,skipped,possible,weapon_checks,passive_contacts);
    return 0;
}
