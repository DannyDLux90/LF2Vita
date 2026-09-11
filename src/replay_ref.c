#include "replay_ref.h"
#include "game.h"
#include "lf2_rng.h"
#include "log.h"

#include <psp2/io/fcntl.h>
#include <stdlib.h>
#include <string.h>

#define REF_MAGIC0 'L'
#define REF_MAGIC1 '2'
#define REF_MAGIC2 'R'
#define REF_MAGIC3 'F'
#define REF_VERSION 1
#define REF_HEADER_SIZE 40
#define REF_PLAYER_SIZE 48
#define REF_MAX_PACKETS 17815u

static uint16_t rd16(const uint8_t *p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint32_t rd32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static int32_t rdi32(const uint8_t *p){return (int32_t)rd32(p);}

static int read_exact(SceUID fd,void *buf,int size){
    uint8_t *p=(uint8_t*)buf;int got=0;
    while(got<size){int rc=sceIoRead(fd,p+got,size-got);if(rc<=0)return rc<0?rc:-1;got+=rc;}
    return got;
}

static uint32_t decode_key(uint8_t v){
    uint32_t m=0;
    if(v&0x20)m|=LF2_REMOTE_LEFT;if(v&0x10)m|=LF2_REMOTE_RIGHT;
    if(v&0x40)m|=LF2_REMOTE_UP;if(v&0x80)m|=LF2_REMOTE_DOWN;
    if(v&0x08)m|=LF2_REMOTE_ATTACK;if(v&0x04)m|=LF2_REMOTE_JUMP;if(v&0x02)m|=LF2_REMOTE_DEFEND;
    return m;
}

int lf2_ref_replay_load(const char *path,lf2_ref_replay_t *out){
    if(!path||!out)return -1;memset(out,0,sizeof(*out));
    SceUID fd=sceIoOpen(path,SCE_O_RDONLY,0);if(fd<0)return fd;
    uint8_t h[REF_HEADER_SIZE];int rc=read_exact(fd,h,sizeof(h));if(rc!=(int)sizeof(h)){sceIoClose(fd);return -2;}
    if(h[0]!=REF_MAGIC0||h[1]!=REF_MAGIC1||h[2]!=REF_MAGIC2||h[3]!=REF_MAGIC3||rd16(h+4)!=REF_VERSION||rd16(h+6)!=REF_HEADER_SIZE){sceIoClose(fd);return -3;}
    out->mode=rdi32(h+8);out->difficulty=rdi32(h+12);out->background=rdi32(h+16);out->stage_raw=rdi32(h+20);
    out->movie_tus=rd32(h+24);out->packet_count=rd32(h+28);uint32_t rng_len=rd32(h+32);out->flags=rd32(h+36);
    if(out->packet_count>REF_MAX_PACKETS||rng_len!=LF2_REF_RNG_LEN){sceIoClose(fd);return -4;}
    for(int i=0;i<LF2_REF_PLAYER_COUNT;i++){
        uint8_t p[REF_PLAYER_SIZE];rc=read_exact(fd,p,sizeof(p));if(rc!=(int)sizeof(p)){sceIoClose(fd);lf2_ref_replay_free(out);return -5;}
        lf2_ref_player_t *d=&out->players[i];d->role=rdi32(p);d->character=rdi32(p+4);d->team=rdi32(p+8);d->kill=rdi32(p+12);d->attack=rdi32(p+16);
        d->hp_used=rdi32(p+20);d->mp_used=rdi32(p+24);d->picking=rdi32(p+28);d->status=rdi32(p+32);memcpy(d->name,p+36,12);d->name[11]=0;
    }
    rc=read_exact(fd,out->rng,LF2_REF_RNG_LEN);if(rc!=LF2_REF_RNG_LEN){sceIoClose(fd);lf2_ref_replay_free(out);return -6;}
    if(out->packet_count){
        size_t n=(size_t)out->packet_count*4u;out->keys=(uint8_t*)malloc(n);if(!out->keys){sceIoClose(fd);return -7;}
        rc=read_exact(fd,out->keys,(int)n);if(rc!=(int)n){sceIoClose(fd);lf2_ref_replay_free(out);return -8;}
    }
    sceIoClose(fd);out->tu=0;
    lf2_logf("REF","loaded %s mode=%d diff=%d bg=%d stage=%d tus=%u packets=%u",path,out->mode,out->difficulty,out->background,out->stage_raw,(unsigned)out->movie_tus,(unsigned)out->packet_count);
    return 0;
}

void lf2_ref_replay_free(lf2_ref_replay_t *r){if(!r)return;if(r->keys)free(r->keys);memset(r,0,sizeof(*r));}
void lf2_ref_replay_reset(lf2_ref_replay_t *r){if(r)r->tu=0;}
int lf2_ref_replay_enable_rng(const lf2_ref_replay_t *r){if(!r)return -1;lf2_rng_stock_enable(r->rng,LF2_REF_RNG_LEN);lf2_rng_stock_reset(0,0);return lf2_rng_stock_enabled()?0:-2;}

int lf2_ref_replay_vita_difficulty(const lf2_ref_replay_t *r){
    if(!r)return LF2_DIFF_NORMAL;
    switch(r->difficulty){case -1:return LF2_DIFF_CRAZY;case 0:return LF2_DIFF_DIFFICULT;case 1:return LF2_DIFF_NORMAL;case 2:return LF2_DIFF_EASY;default:return LF2_DIFF_NORMAL;}
}

int lf2_ref_replay_vita_background(const lf2_ref_replay_t *r){
    if(!r)return 3;
    /* Stock recording ids -> LF2Vita stage texture order. Lee On Road (99)
       is not currently shipped as a selectable Vita arena. */
    switch(r->background){case 0:return 3;case 1:return 7;case 2:return 0;case 3:return 6;case 4:return 5;case 5:return 8;case 6:return 1;case 7:return 2;case 8:return 4;default:return 3;}
}

bool lf2_ref_replay_clock(void *userdata,uint32_t local_held[4],uint32_t remote_held[4]){
    lf2_ref_replay_t *r=(lf2_ref_replay_t*)userdata;if(!r||!local_held||!remote_held)return 0;
    if(r->tu>=r->movie_tus)return 0;uint32_t packet=r->tu/2u;if(packet>=r->packet_count)return 0;
    const uint8_t *k=r->keys+(size_t)packet*4u;for(int i=0;i<4;i++){local_held[i]=decode_key(k[i]);remote_held[i]=0;}
    r->tu++;return 1;
}

int lf2_ref_replay_build_match(const lf2_ref_replay_t *r,lf2_ref_match_plan_t *out){
    if(!r||!out)return -1;memset(out,0,sizeof(*out));
    int actor=0,human_control=0;
    for(int slot=0;slot<LF2_REF_PLAYER_COUNT;slot++){
        const lf2_ref_player_t *p=&r->players[slot];if(p->role<0)continue;
        int roster=lf2_roster_from_original_id(p->character);if(roster<0)return -10-slot;
        if(actor==0){out->player_index=roster;out->player_team=p->team;}
        else {if(actor>7)return -30;out->cpu_indices[actor-1]=roster;out->cpu_teams[actor-1]=p->team;}
        out->actor_fixture_slot[actor]=slot;
        if(p->role==1){if(human_control>=4)return -40-slot;out->actor_control[actor]=(uint8_t)(1+human_control++);}
        actor++;
    }
    if(actor<2)return -2;
    out->actor_count=actor;out->cpu_count=actor-1;
    out->stage_index=lf2_ref_replay_vita_background(r);out->difficulty=lf2_ref_replay_vita_difficulty(r);
    return 0;
}

int lf2_ref_replay_compare_stats(const lf2_ref_replay_t *r,const lf2_ref_match_plan_t *plan,
                                 const lf2_stock_stat_t *actual,int actual_count){
    if(!r||!plan||!actual)return -1;int mismatches=0;
    int n=plan->actor_count<actual_count?plan->actor_count:actual_count;
    if(actual_count!=plan->actor_count){lf2_logf("REFCMP","actor_count expected=%d actual=%d",plan->actor_count,actual_count);mismatches++;}
    for(int a=0;a<n;a++){
        int slot=plan->actor_fixture_slot[a];const lf2_ref_player_t *e=&r->players[slot];const lf2_stock_stat_t *v=&actual[a];
        int er=lf2_roster_from_original_id(e->character);
#define CMP(field,ev,av) do{int _e=(ev),_a=(av);if(_e!=_a){lf2_logf("REFCMP","actor=%d slot=%d %s expected=%d actual=%d",a,slot+1,(field),_e,_a);mismatches++;}}while(0)
        CMP("roster",er,v->roster_idx);CMP("team",e->team,v->team);CMP("kill",e->kill,v->kills);CMP("attack",e->attack,v->attack);
        CMP("hp_lost",e->hp_used,v->hp_lost);CMP("mp_used",e->mp_used,v->mp_used);CMP("picking",e->picking,v->picking);
#undef CMP
    }
    lf2_logf("REFCMP","done actors=%d mismatches=%d",n,mismatches);return mismatches;
}
