                    lf2_logf("CLONE","owner=%s slot=%d form=%s hp=10 action=%d",f->def.name,slot,fighters[slot].def.name,op->action);
                }
            }
            continue;
        }
        for(int n=0;n<count;n++)object_spawn_one(w,f,owner_idx,op,n,count);
    }
}

static void object_region_world(const world_object_t *o,int rx,int ry,int rw,int rh,int *x,int *y){
    lf2_frame_def_t *fr=(lf2_frame_def_t*)&o->cache->def.frames[o->frame];
    if(!o->facing)*x=(int)o->x-fr->centerx+rx;else *x=(int)o->x+1+fr->centerx-rw-rx;
    *y=(int)o->y-fr->centery+ry;
}

static fighter_t *object_nearest_enemy(world_object_t *o,fighter_t *fighters,int total){
    fighter_t *best=NULL;float bd=1.0e30f;if(!o)return NULL;
    for(int i=0;i<total;i++){fighter_t *f=&fighters[i];if(f->dead||i==o->owner)continue;if(o->team&&f->team==o->team)continue;float dx=f->x-o->x,dz=f->z-o->z,d=dx*dx+dz*dz;if(d<bd){bd=d;best=f;}}
    return best;
}

static int object_target_valid(const world_object_t *o,const fighter_t *fighters,int total,int slot,int ally){
    if(!o||!fighters||slot<0||slot>=total)return 0;
    const fighter_t *f=&fighters[slot];if(!f->active||f->dead)return 0;
    if(ally){
        if(slot==o->owner)return 1;
        return o->team!=0&&f->team==o->team;
    }
    if(slot==o->owner)return 0;
    return !(o->team!=0&&f->team==o->team);
}

static int object_pick_target(world_object_t *o,fighter_t *fighters,int total,int ally,int ordinal){
    if(!o||!fighters||total<=0)return -1;
    int candidates[LF2_MAX_ACTORS],n=0;
    for(int i=0;i<total&&n<LF2_MAX_ACTORS;i++)if(object_target_valid(o,fighters,total,i,ally))candidates[n++]=i;
    if(!n)return -1;
    if(ordinal>=0)return candidates[ordinal%n];
    return candidates[rand()%n];
}

static world_object_t *object_spawn_engine_child(object_world_t *w,const world_object_t *parent,
                                                 int oid,int action,int target_slot,
                                                 float dx,float dy,float dz){
    if(!w||!parent||!parent->active)return NULL;
    object_cache_entry_t *cache=object_cache_get(w,oid);if(!cache)return NULL;
    world_object_t *o=world_alloc_object(w);if(!o)return NULL;
    o->cache=cache;o->oid=oid;o->type=cache->type;o->owner=parent->owner;o->team=parent->team;
    o->facing=parent->facing;o->x=parent->x+dx;o->y=parent->y+dy;o->z=parent->z+dz;o->target_slot=target_slot;
    object_set_frame(o,action);if(!o->active)return NULL;
    lf2_logf("OBJECT","engine hit_Fa spawn parent=%d oid=%d action=%d target=%d pos=%.1f,%.1f,%.1f",
             parent->oid,oid,action,target_slot,o->x,o->y,o->z);
    return o;
}

/* Original LF2 implements several type-3 hit_Fa values in the executable,
   not in DAT opoints.  Without these, Jan/Bat/Firzen/Julian specials play the
   casting animation but never create their actual attacks. */
static void object_activate_hitfa(object_world_t *w,world_object_t *o,fighter_t *fighters,int total,int code){
    if(!w||!o||!fighters||o->hitfa_fired)return;o->hitfa_fired=true;
    int enemies[LF2_MAX_ACTORS],ne=0,allies[LF2_MAX_ACTORS],na=0;
    for(int i=0;i<total;i++){
        if(object_target_valid(o,fighters,total,i,0))enemies[ne++]=i;
        if(object_target_valid(o,fighters,total,i,1))allies[na++]=i;
    }
    if(code==5){ /* Jan: one healing angel per team member (including self). */
        for(int n=0;n<na;n++)object_spawn_engine_child(w,o,219,0,allies[n],0.0f,-8.0f,(float)(n-(na-1)*0.5f)*6.0f);
    }else if(code==6){ /* Jan: one devil per enemy, maximum seven. */
        int count=ne<7?ne:7;for(int n=0;n<count;n++)object_spawn_engine_child(w,o,220,0,enemies[n],0.0f,-8.0f,(float)(n-(count-1)*0.5f)*6.0f);
    }else if(code==8){ /* Bat: 3 bats, +1 for each two enemies beyond four. */
        int count=3;if(ne>4)count+=(ne-3)/2;if(count>8)count=8;
        for(int n=0;n<count;n++){int t=ne?enemies[rand()%ne]:-1;object_spawn_engine_child(w,o,225,0,t,0.0f,-4.0f,(float)(n-(count-1)*0.5f)*7.0f);}
    }else if(code==9){ /* Firzen Disaster: fire/ice homing balls, 4..10. */
        int count=ne<=4?4:ne;if(count>10)count=10;if(count<1)count=4;
        for(int n=0;n<count;n++){int t=ne?enemies[rand()%ne]:-1;int oid=(rand()&1)?221:222;object_spawn_engine_child(w,o,oid,0,t,0.0f,-8.0f,(float)(n-(count-1)*0.5f)*5.0f);}
    }else if(code==11){ /* Firzen Arctic Volcano hard-coded explosion set. */
        object_spawn_engine_child(w,o,211,109,-1,0,0,0);
        object_spawn_engine_child(w,o,211,50,-1,0,0,0);
        object_spawn_engine_child(w,o,212,100,-1,0,0,0);
        object_spawn_engine_child(w,o,221,81,-1,0,0,0);
    }else if(code==13){ /* Julian: one random-target Skull Blast. */
        int t=ne?enemies[rand()%ne]:-1;object_spawn_engine_child(w,o,228,0,t,0.0f,0.0f,0.0f);
    }
    lf2_logf("OBJECT","engine hit_Fa activate code=%d owner=%d enemies=%d allies=%d",code,o->owner,ne,na);
}

static fighter_t *object_chase_target(world_object_t *o,fighter_t *fighters,int total,int ally){
    if(!o||!fighters)return NULL;
    if(!object_target_valid(o,fighters,total,o->target_slot,ally))o->target_slot=object_pick_target(o,fighters,total,ally,-1);
    return (o->target_slot>=0&&o->target_slot<total)?&fighters[o->target_slot]:NULL;
}

static void object_apply_chase(world_object_t *o,fighter_t *fighters,int total,int code){
    if(!o||!fighters)return;
    int ally=(code==4);fighter_t *t=object_chase_target(o,fighters,total,ally);if(!t)return;
    float dx=t->x-o->x,dy=(t->y-35.0f)-o->y,dz=t->z-o->z;
    if(code==4&&fabsf(dx)<34.0f&&fabsf(dy)<52.0f&&fabsf(dz)<24.0f){
        t->jan_heal_ticks=100;t->jan_heal_phase=0;
        if(o->cache->def.frames[60].present)object_set_frame(o,60);else o->active=false;
        lf2_logf("STATUS","%s Jan Angel regen=100tu",t->def.name);return;
    }
    if(code==14)dy=0.0f; /* Julian skulls steer only in x/z. */
    float len=sqrtf(dx*dx+dy*dy+dz*dz);if(len<1.0f)return;
    float speed=sqrtf(o->vx*o->vx+o->vy*o->vy+o->vz*o->vz);if(speed<4.0f)speed=(code==14)?10.0f:7.0f;
    float turn=0.14f;if(code==2||code==12)turn=0.20f;else if(code==7)turn=0.17f;else if(code==14)turn=0.12f;
    o->vx=o->vx*(1.0f-turn)+dx/len*speed*turn;
