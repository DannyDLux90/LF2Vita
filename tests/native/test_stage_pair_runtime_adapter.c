/* Compiles the actual candidate runtime adapter against a minimal mirror
 * of the public LF2 Fighter/DAT data structures. DOES NOT link Vita engine.
 * All world-coordinate candidates are checked against a narrow-phase oracle.
 */
#include <math.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define LF2_STAGE_BROADPHASE_ENABLED 1

#define NITR 8
#define NBDY 8
#define NFRAMES 75
typedef struct {
    int kind,x,y,w,h,catchingact,caughtact;
} lf2_itr_def_t;
typedef struct {
    int x,y,w,h;
} lf2_bdy_def_t;
typedef struct {
    int present,centerx,centery,itr_count,bdy_count;
    lf2_itr_def_t itrs[NITR];
    lf2_bdy_def_t bdys[NBDY];
} lf2_frame_def_t;
typedef struct {
    lf2_frame_def_t frames[NFRAMES];
} fighter_def_t;
typedef struct {
    float x,y;
    int frame,facing,held_oid;
    fighter_def_t def;
} fighter_t;
static lf2_frame_def_t *frame_def(fighter_t *f) {
    if(f->frame<0||f->frame>=NFRAMES||!f->def.frames[f->frame].present)
        return NULL;
    return &f->def.frames[f->frame];
}
static void region_world(const fighter_t *f,int rx,int ry,int w,int h,
                         int *x,int *y) {
    const lf2_frame_def_t *fr=&f->def.frames[f->frame];
    (void)h;
    *x=!f->facing?(int)f->x-fr->centerx+rx:(int)f->x+1+fr->centerx-w-rx;
    *y=(int)f->y-fr->centery+ry;
}
#include "lf2_stage_pair_runtime.inc"

static uint32_t rng=UINT32_C(0xa136c33d);
static uint32_t next_u32(void) {
    uint32_t x=rng;x^=x<<13;x^=x>>17;x^=x<<5;rng=x;return x;
}
static int rnd(int a,int b) {
    return a+(int)(next_u32()%(unsigned)(b-a+1));
}
static int intersects(int x,int y,int w,int h,int xx,int yy,int ww,int hh) {
    if(w<=0||h<=0||ww<=0||hh<=0)return 0;
    return (int64_t)x+w>xx && (int64_t)xx+ww>x &&
           (int64_t)y+h>yy && (int64_t)yy+hh>y;
}
static int can_interact_unfiltered(fighter_t *a,fighter_t *b) {
    if(a->held_oid)return 1; /* unknown held-object geometry */
    lf2_frame_def_t *af=frame_def(a),*bf=frame_def(b);
    if(!af||!bf)return 1; /* unknown engine fallback */
    for(int i=0;i<af->itr_count;i++) {
        lf2_itr_def_t *it=&af->itrs[i];
        if(!lf2_stage_runtime_contact_itr_kind(it))continue;
        if(it->w<=0||it->h<=0)return 1; /* no pruning on odd DAT */
        int ax,ay;
        region_world(a,it->x,it->y,it->w,it->h,&ax,&ay);
        for(int j=0;j<bf->bdy_count;j++) {
            lf2_bdy_def_t *bd=&bf->bdys[j];
            if(bd->w<=0||bd->h<=0)return 1;
            int bx,by;
            region_world(b,bd->x,bd->y,bd->w,bd->h,&bx,&by);
            if(intersects(ax,ay,it->w,it->h,bx,by,bd->w,bd->h))
                return 1;
        }
        if((a->frame==60||a->frame==65)&&a->def.frames[70].present &&
           it->kind==0) {
            for(int j=0;j<bf->itr_count;j++) {
                lf2_itr_def_t *p=&bf->itrs[j];
                if(p->kind!=6)continue;
                if(p->w<=0||p->h<=0)return 1;
                int px,py;
                region_world(b,p->x,p->y,p->w,p->h,&px,&py);
                if(intersects(ax,ay,it->w,it->h,px,py,p->w,p->h))
                    return 1;
            }
        }
    }
    return 0;
}
static void generate(fighter_t *a, fighter_t *b) {
    memset(a,0,sizeof(*a));memset(b,0,sizeof(*b));
    a->frame=(rnd(0,2)==0)?60:(rnd(0,1)?65:2);
    b->frame=2;
    a->facing=rnd(0,1);b->facing=rnd(0,1);
    a->held_oid=(rnd(0,11)==0)?123:0;
    a->x=(float)rnd(-4000,4000);a->y=(float)rnd(-600,600);
    b->x=a->x+(float)rnd(-1400,1400);
    b->y=a->y+(float)rnd(-300,300);
    a->def.frames[70].present=rnd(0,3)!=0;
    lf2_frame_def_t *af=&a->def.frames[a->frame];
    lf2_frame_def_t *bf=&b->def.frames[b->frame];
    af->present=bf->present=1;
    af->centerx=rnd(0,200);af->centery=rnd(0,200);
    bf->centerx=rnd(0,200);bf->centery=rnd(0,200);
    af->itr_count=rnd(0,NITR);
    bf->itr_count=rnd(0,NITR);
    bf->bdy_count=rnd(0,NBDY);
    const int kinds[]={0,1,3,4,5,6,7,10,11,15,16};
    for(int i=0;i<af->itr_count;i++){
        lf2_itr_def_t *r=&af->itrs[i];
        r->kind=kinds[rnd(0,10)];
        r->x=rnd(-300,300);r->y=rnd(-150,150);
        r->w=rnd(1,200);r->h=rnd(1,150);
        r->catchingact=rnd(0,2)?0:rnd(1,50);
        r->caughtact=rnd(0,2)?0:rnd(1,50);
    }
    for(int i=0;i<bf->itr_count;i++){
        lf2_itr_def_t *r=&bf->itrs[i];
        r->kind=rnd(0,3)?6:0;
        r->x=rnd(-300,300);r->y=rnd(-150,150);
        r->w=rnd(1,200);r->h=rnd(1,150);
    }
    for(int i=0;i<bf->bdy_count;i++) {
        lf2_bdy_def_t *r=&bf->bdys[i];
        r->x=rnd(-150,150);r->y=rnd(-120,120);
        r->w=rnd(1,180);r->h=rnd(1,140);
    }
}
int main(void) {
    fighter_t *a=calloc(1,sizeof(*a)),*b=calloc(1,sizeof(*b));
    if(!a||!b)return 3;
    /* No call to the production-style int arithmetic on overflowing DAT. */
    generate(a,b);
    a->held_oid=0;
    a->x=0.f;a->y=0.f;
    a->def.frames[a->frame].centerx=0;
    a->def.frames[a->frame].itr_count=1;
    a->def.frames[a->frame].itrs[0]=(lf2_itr_def_t){
        .kind=0,.x=INT_MAX-10,.y=0,.w=100,.h=10};
    if(lf2_stage_pair_runtime_may_contact(a,b)!=1){
        fprintf(stderr,"Expected fail-open for attacker coordinate overflow\n");
        return 3;
    }
    generate(a,b);
    a->held_oid=0;b->x=0.f;b->y=0.f;
    b->def.frames[b->frame].centerx=0;
    b->def.frames[b->frame].bdy_count=1;
    b->def.frames[b->frame].bdys[0]=(lf2_bdy_def_t){
        .x=INT_MAX-10,.y=0,.w=100,.h=10};
    if(lf2_stage_pair_runtime_may_contact(a,b)!=1){
        fprintf(stderr,"Expected fail-open for target coordinate overflow\n");
        return 3;
    }
    /* Intermediate overflow that later cancels to a valid final int:
       (int)0 - INT_MIN overflows before rx=-2 is applied. */
    generate(a,b);
    a->held_oid=0;a->x=0.f;a->y=0.f;a->facing=0;
    lf2_frame_def_t *attack_frame=&a->def.frames[a->frame];
    attack_frame->centerx=INT_MIN;attack_frame->centery=0;
    attack_frame->itr_count=1;
    attack_frame->itrs[0]=(lf2_itr_def_t){
        .kind=0,.x=-2,.y=0,.w=1,.h=1};
    if (lf2_stage_region_is_int_safe(a,attack_frame,-2,0,1,1) ||
        lf2_stage_pair_runtime_may_contact(a,b)!=1) {
        fprintf(stderr,"Expected fail-open for attacker intermediate overflow\\n");
        return 3;
    }
    generate(a,b);
    a->held_oid=0;b->x=0.f;b->y=0.f;b->facing=0;
    lf2_frame_def_t *body_frame=&b->def.frames[b->frame];
    body_frame->centerx=0;body_frame->centery=INT_MIN;
    body_frame->bdy_count=1;
    body_frame->bdys[0]=(lf2_bdy_def_t){
        .x=0,.y=-2,.w=1,.h=1};
    if (lf2_stage_region_is_int_safe(b,body_frame,0,-2,1,1) ||
        lf2_stage_pair_runtime_may_contact(a,b)!=1) {
        fprintf(stderr,"Expected fail-open for target y intermediate overflow\\n");
        return 3;
    }
    unsigned rejected=0,possible=0,weapon=0;
    for(int n=0;n<100000;n++){
        generate(a,b);
        int reference=can_interact_unfiltered(a,b);
        int candidate=lf2_stage_pair_runtime_may_contact(a,b);
        if(reference&&!candidate){
            fprintf(stderr,"FALSE NEGATIVE runtime adapter case=%d\n",n);
            return 2;
        }
        if(!candidate)rejected++;
        if(reference)possible++;
        if(a->held_oid){weapon++;if(!candidate)return 2;}
        /* Recompute with immediately mutated coordinates / frame state.
           No cached AABB is allowed to survive this transition. */
        a->x+=(float)rnd(-8,8);
        b->y+=(float)rnd(-8,8);
        a->facing^=1;
        reference=can_interact_unfiltered(a,b);
        candidate=lf2_stage_pair_runtime_may_contact(a,b);
        if(reference&&!candidate){
            fprintf(stderr,"FALSE NEGATIVE mutated case=%d\n",n);
            return 2;
        }
    }
    if(rejected<2000||possible<2000||weapon<1000)return 4;
    printf("PASS 100000 runtime-adapter scenarios x2 (before/after mutation), "
           "0 false negatives; rejected=%u, possible=%u, weapon-fallback=%u. "
           "Host mock structs, NOT Vita runtime.\n",rejected,possible,weapon);
    free(a);free(b);
    return 0;
}
