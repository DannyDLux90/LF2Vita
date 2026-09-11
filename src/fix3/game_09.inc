        lf2_logf("OBJECT","timer oid=%d hit_a=%d duration=%d hit_d=%d",o->oid,fr->hit_a,o->type3_timer,fr->hit_d);
    }
}

static void object_set_frame(world_object_t *o,int id){
    if(!o||!o->cache)return;
    if(id<0||id>=LF2_MAX_FRAMES||!o->cache->def.frames[id].present){o->active=false;return;}
    o->frame=id;o->ticks=0;o->opoint_fired=false;o->hitfa_fired=false;
    lf2_frame_def_t *fr=&o->cache->def.frames[id];
    if(fr->dvx==550)o->vx=0;else if(fr->dvx)o->vx=(float)(o->facing?-fr->dvx:fr->dvx);
    if(fr->dvy==550)o->vy=0;else if(fr->dvy)o->vy=(float)fr->dvy;
    if(fr->dvz==550)o->vz=0;else if(fr->dvz)o->vz=(float)fr->dvz;
    if(fr->sound[0])lf2_audio_play(fr->sound);
    object_prepare_type3_timer(o,fr);
}

static world_object_t *world_alloc_object(object_world_t *w){
    if(!w)return NULL;
    for(int i=0;i<LF2_MAX_ACTIVE_OBJECTS;i++)if(!w->objects[i].active){memset(&w->objects[i],0,sizeof(w->objects[i]));w->objects[i].active=true;w->objects[i].target_slot=-1;return &w->objects[i];}
    static int warned=0;if(!warned++){lf2_logf("WARN","active object pool full (%d)",LF2_MAX_ACTIVE_OBJECTS);}return NULL;
}

static void object_spawn_one(object_world_t *w,fighter_t *owner,int owner_idx,const lf2_opoint_def_t *op,int variant,int count){
    if(!w||!owner||!op||op->oid<=0)return;
    object_cache_entry_t *cache=object_cache_get(w,op->oid);if(!cache)return;
    world_object_t *o=world_alloc_object(w);if(!o)return;
    o->cache=cache;o->oid=op->oid;o->type=cache->type;o->owner=owner_idx;o->team=owner->team;
    int backwards=(op->facing%10)==1;
    if(op->facing==10)o->facing=0;else o->facing=owner->facing^(backwards?1:0);
    lf2_frame_def_t *fr=frame_def(owner);if(!fr){o->active=false;return;}
    if(!owner->facing)o->x=owner->x-(float)fr->centerx+(float)op->x;
    else o->x=owner->x+1.0f+(float)fr->centerx-(float)op->x;
    o->y=owner->y-(float)fr->centery+(float)op->y;o->z=owner->z;
    if(op->kind==2){
        o->attached=true;o->attach_dx=o->x-owner->x;o->attach_dy=o->y-owner->y;o->attach_dz=o->z-owner->z;
        owner->ice_sword_ticks=900;
        lf2_logf("OBJECT","held weapon attach owner=%s oid=%d action=%d duration=%d",owner->def.name,op->oid,op->action,owner->ice_sword_ticks);
    }
    object_set_frame(o,op->action);
    if(!o->active)return;
    /* Explicit opoint launch speeds override values in the first object frame. */
    if(op->dvx)o->vx=(float)(o->facing?-op->dvx:op->dvx);
    if(op->dvy)o->vy=(float)op->dvy;
    if(op->dvz)o->vz=(float)(o->facing?-op->dvz:op->dvz);
    /* LF2's facing: N0 creates N copies. Henry's five-arrow shot relies on
       those copies being spatially distinct; spread them symmetrically while
       preserving the authored centre arrow. */
    if(count>1){float mid=(float)(count-1)*0.5f,off=(float)variant-mid;o->vy+=off*0.85f;o->z+=off*4.0f;}
    lf2_logf("OBJECT","spawn oid=%d type=%d owner=%d action=%d facing=%d copy=%d/%d pos=%.1f,%.1f,%.1f vel=%.1f,%.1f,%.1f",
             o->oid,o->type,owner_idx,o->frame,o->facing,variant+1,count,o->x,o->y,o->z,o->vx,o->vy,o->vz);
}

/* Type-3 objects can themselves contain opoints. This is essential to the
   stock LF2 projectile state machines: energy balls rebound/split by creating
   replacement objects, Firzen's chase objects generate flame/chase children,
   and several projectile disappear frames spawn their follow-up effect. */
static void object_spawn_child_one(object_world_t *w,const world_object_t *parent,
                                   const lf2_opoint_def_t *op,int variant,int count){
    if(!w||!parent||!parent->active||!op||op->oid<=0)return;
    object_cache_entry_t *cache=object_cache_get(w,op->oid);if(!cache)return;
    world_object_t *o=world_alloc_object(w);if(!o)return;
    o->cache=cache;o->oid=op->oid;o->type=cache->type;o->owner=parent->owner;o->team=parent->team;
    int backwards=(op->facing%10)==1;
    if(op->facing==10)o->facing=0;else o->facing=parent->facing^(backwards?1:0);
    lf2_frame_def_t *fr=(lf2_frame_def_t*)&parent->cache->def.frames[parent->frame];
    if(!parent->facing)o->x=parent->x-(float)fr->centerx+(float)op->x;
    else o->x=parent->x+1.0f+(float)fr->centerx-(float)op->x;
    o->y=parent->y-(float)fr->centery+(float)op->y;o->z=parent->z;
    object_set_frame(o,op->action);if(!o->active)return;
    if(op->dvx)o->vx=(float)(o->facing?-op->dvx:op->dvx);
    if(op->dvy)o->vy=(float)op->dvy;
    if(op->dvz)o->vz=(float)(o->facing?-op->dvz:op->dvz);
    if(count>1){float mid=(float)(count-1)*0.5f,off=(float)variant-mid;o->vy+=off*0.85f;o->z+=off*4.0f;}
    lf2_logf("OBJECT","child spawn parent=%d oid=%d owner=%d action=%d copy=%d/%d pos=%.1f,%.1f,%.1f",
             parent->oid,o->oid,o->owner,o->frame,variant+1,count,o->x,o->y,o->z);
}

static int clone_roster_for_oid(int oid){return oid==5?3:(oid==52?22:-1);}

static void spawn_fighter_opoints(object_world_t *w,fighter_t *fighters,int total,int owner_idx){
    if(!w||!fighters||owner_idx<0||owner_idx>=total)return;
    fighter_t *f=&fighters[owner_idx];
    if(f->dead||!f->active||f->opoint_fired)return;lf2_frame_def_t *fr=frame_def(f);if(!fr||fr->opoint_count<=0)return;
    f->opoint_fired=true;
    for(int i=0;i<fr->opoint_count;i++){
        const lf2_opoint_def_t *op=&fr->opoints[i];
        int count=(op->facing>=20)?op->facing/10:1;if(count<1)count=1;if(count>8)count=8;
        int clone_roster=clone_roster_for_oid(op->oid);
        if(clone_roster>=0){
            for(int n=0;n<count;n++){
                int slot=-1;for(int k=1;k<total;k++)if(!fighters[k].active){slot=k;break;}
                if(slot<0){lf2_logf("CLONE","%s clone oid=%d skipped: no free fighter slot",f->def.name,op->oid);break;}
                float side=f->facing?-1.0f:1.0f;float x=clampf(f->x+side*(42.0f+24.0f*n),g_world_min_x,g_world_max_x);
                float z=clampf(f->z+(n?18.0f:-18.0f),-70.0f,75.0f);
                if(load_fighter(&fighters[slot],clone_roster,true,f->team,x,z,f->facing,slot,10)==0){
                    fighters[slot].clone=true;fighters[slot].mp=150;
                    if(op->action>0&&op->action<LF2_MAX_FRAMES&&fighters[slot].def.frames[op->action].present)set_frame(&fighters[slot],op->action);
