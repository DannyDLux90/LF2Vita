    if(!cat){lf2_logf("OBJECT","unsupported oid=%d (not in stock projectile catalog)",oid);return NULL;}
    if(w->cache_count>=LF2_MAX_OBJECT_CACHE){lf2_logf("ERROR","object cache full oid=%d",oid);return NULL;}
    object_cache_entry_t *c=&w->cache[w->cache_count++];memset(c,0,sizeof(*c));c->used=true;c->oid=oid;c->type=cat->type;
    int rc=lf2_load_character(GAME_ROOT,cat->dat,&c->def);
    if(rc<0){lf2_logf("ERROR","object DAT load oid=%d dat=%s rc=%d",oid,cat->dat,rc);c->used=false;return NULL;}
    if(!c->def.name[0])snprintf(c->def.name,sizeof(c->def.name),"OID %d",oid);
    rc=lf2_load_character_textures(GAME_ROOT,&c->def);
    if(rc<0){lf2_logf("ERROR","object texture load oid=%d dat=%s rc=%d",oid,cat->dat,rc);lf2_free_character_textures(&c->def);c->used=false;return NULL;}
    lf2_logf("OBJECT","cache oid=%d type=%d dat=%s sheets=%d",oid,c->type,cat->dat,c->def.sheet_count);lf2_log_memory("object cached");
    return c;
}

static world_object_t *world_alloc_object(object_world_t *w);
static void object_set_frame(world_object_t *o,int id);

/* Stable resting frames in stock LF2 are 64 for light weapons/bottles and 20
   for heavy objects. The impact animation starts at 70 or 21 respectively. */
static int object_ground_frame(const object_cache_entry_t *c){
    if(!c)return 0;
    if(c->def.frames[64].present)return 64;
    if(c->def.frames[20].present)return 20;
    if(c->def.frames[60].present)return 60;
    return 0;
}
static int object_landing_frame(const object_cache_entry_t *c){
    if(!c)return 0;
    if(c->def.frames[70].present)return 70;
    if(c->def.frames[21].present&&c->def.frames[20].present)return 21;
    if(c->def.frames[60].present)return 60;
    return object_ground_frame(c);
}

static world_object_t *spawn_pickup_item(object_world_t *w,int oid,float x,float z,float y){
    if(!w||!item_is_pickup_oid(oid))return NULL;
    object_cache_entry_t *cache=object_cache_get(w,oid);if(!cache)return NULL;
    world_object_t *o=world_alloc_object(w);if(!o)return NULL;
    o->cache=cache;o->oid=oid;o->type=cache->type;o->owner=-1;o->team=0;o->pickup_item=true;
    o->x=clampf(x,g_world_min_x,g_world_max_x);o->z=clampf(z,-65.0f,70.0f);o->y=y;o->facing=0;
    o->durability=cache->def.weapon_hp>0?cache->def.weapon_hp:item_units_for_oid(oid);o->grounded=(y>=0.0f);o->vy=o->grounded?0.0f:0.4f;
    object_set_frame(o,o->grounded?object_ground_frame(cache):0);
    if(!o->active)return NULL;
    lf2_logf("ITEM","spawn oid=%d name=%s pos=%.1f,%.1f,%.1f authored=%d",oid,item_name(oid),o->x,o->y,o->z,y>=0.0f?1:0);
    return o;
}

static int release_held_throw(object_world_t *w,fighter_t *f,int owner_idx){
    if(!w||!f||!f->held_oid||(item_is_bottle(f->held_oid)&&f->drinking))return 0;
    lf2_frame_def_t *ff=frame_def(f);
    if(!ff||ff->state!=15||!ff->wpoint.present)return 0;
    if(ff->wpoint.dvx==0&&ff->wpoint.dvy==0&&ff->wpoint.dvz==0)return 0;
    object_cache_entry_t *cache=NULL;int held_frame=0;float x=f->x,y=f->y-35.0f;
    if(!held_item_frame_and_pose(w,f,&cache,&held_frame,&x,&y,NULL)||!cache)cache=object_cache_get(w,f->held_oid);
    if(!cache)return 0;
    world_object_t *o=world_alloc_object(w);if(!o)return 0;
    int oid=f->held_oid;
    o->cache=cache;o->oid=oid;o->type=cache->type;o->owner=owner_idx;o->team=f->team;
    o->pickup_item=false;o->recoverable_weapon=true;o->grounded=false;o->facing=f->facing;o->x=x;o->y=y;o->z=f->z;o->durability=f->held_units;
    int start=item_is_heavy(oid)?0:40;
    if(start>=LF2_MAX_FRAMES||!cache->def.frames[start].present)start=0;
    object_set_frame(o,start);
    if(!o->active)return 0;
    o->vx=(float)(f->facing?-ff->wpoint.dvx:ff->wpoint.dvx);
    o->vy=(float)ff->wpoint.dvy;
    if(ff->wpoint.dvz){int spread=abs(ff->wpoint.dvz);o->vz=(float)((rand()%(spread*2+1))-spread);}else o->vz=0.0f;
    f->held_oid=0;f->held_units=0;f->drinking=false;f->drink_tick=0;
    lf2_logf("ITEM","%s threw oid=%d name=%s fighter_frame=%d object_frame=%d vel=%.1f,%.1f,%.1f",
             f->def.name,oid,item_name(oid),f->frame,o->frame,o->vx,o->vy,o->vz);
    return 1;
}

static int count_loose_items(const object_world_t *w){
    int n=0;if(!w)return 0;for(int i=0;i<LF2_MAX_ACTIVE_OBJECTS;i++)if(w->objects[i].active&&w->objects[i].pickup_item)n++;return n;
}

static world_object_t *nearest_loose_item(object_world_t *w,const fighter_t *f){
    world_object_t *best=NULL;float bd=1.0e30f;if(!w||!f)return NULL;
    for(int i=0;i<LF2_MAX_ACTIVE_OBJECTS;i++){
        world_object_t *o=&w->objects[i];if(!o->active||!o->pickup_item||!o->grounded)continue;
        float dx=o->x-f->x,dz=o->z-f->z,d=dx*dx+dz*dz;
        if(fabsf(dx)<=78.0f&&fabsf(dz)<=46.0f&&d<bd){best=o;bd=d;}
    }
    return best;
}

static int item_is_simple_throw(int oid){return item_is_heavy(oid)||oid==121;}
static int item_is_direction_throw(int oid){return oid==120||oid==124;}
static int fighter_is_dash_frame(const fighter_t *f){return f&&(f->frame==213||f->frame==214||f->frame==216||f->frame==217);}

/* PC LF2 has a small throw-command hierarchy. Heavy objects/baseballs throw
   on Attack. Knife/boomerang throw on direction+Attack. Bottles and ordinary
   light weapons require a run/jump/dash throw command; otherwise Square keeps
   its normal swing/drink meaning. Pickup remains a dedicated Vita action on L. */
static int fighter_handle_item_input(object_world_t *w,fighter_t *f,input_t *in){
    if(!w||!f||!in||f->dead||!f->active)return 0;
    if(f->held_oid){
        int dir=in->left||in->right||in->up||in->down;
        int airborne=f->y<0.0f||f->vy!=0.0f;
        int dash=fighter_is_dash_frame(f);
        int throw_now=0;
        if(in->attack_pressed){
            if(item_is_simple_throw(f->held_oid))throw_now=1;
            else if(item_is_direction_throw(f->held_oid)&&dir)throw_now=1;
            else if(item_is_bottle(f->held_oid)){if(airborne||dash||(in->run&&dir))throw_now=1;}
            else if(dir&&(airborne||dash||in->run))throw_now=1;
        }
        if(throw_now&&!f->drinking){
            int tf=item_is_heavy(f->held_oid)?50:(airborne||dash?52:45);
            if(tf>=0&&tf<LF2_MAX_FRAMES&&f->def.frames[tf].present){
                set_frame(f,tf);
                lf2_logf("ITEM","%s throw start oid=%d name=%s frame=%d",f->def.name,f->held_oid,item_name(f->held_oid),tf);
                return 1;
            }
        }
        if(item_is_bottle(f->held_oid)&&in->attack&&!f->drinking){
            f->drinking=true;f->drink_tick=0;
            if(f->def.frames[55].present)set_frame(f,55);
            lf2_logf("ITEM","%s starts drinking %s units=%d hp=%d mp=%d",f->def.name,item_name(f->held_oid),f->held_units,f->hp,f->mp);
            return 1;
        }
        return 0;
    }
    /* L is deliberately separate from Square. Accept held L as well as its
       edge so entering the pickup radius while holding L is reliable. */
    if(!in->pickup || in_action(f) || f->falling || f->y<0.0f)return 0;
    world_object_t *o=nearest_loose_item(w,f);if(!o)return 0;
    f->held_oid=o->oid;f->held_units=o->durability>0?o->durability:(o->cache&&o->cache->def.weapon_hp>0?o->cache->def.weapon_hp:item_units_for_oid(o->oid));f->drinking=false;f->drink_tick=0;
    o->active=false;
    int pick_frame=item_is_heavy(f->held_oid)?116:115;if(pick_frame<LF2_MAX_FRAMES&&f->def.frames[pick_frame].present)set_frame(f,pick_frame);
    lf2_audio_play("data/009.wav");
    lf2_logf("ITEM","%s picked oid=%d name=%s units=%d",f->def.name,f->held_oid,item_name(f->held_oid),f->held_units);
    return 1;
}

static void item_director_tick(object_world_t *w,int *timer){
    if(!w||!timer)return;
    if(*timer>0){(*timer)--;return;}
    if(count_loose_items(w)>=4){*timer=180;return;}
    static const int ids[]={100,101,120,121,122,123,124,150,151};
    int oid=ids[rand()%(int)(sizeof(ids)/sizeof(ids[0]))];
    float span=g_world_max_x-g_world_min_x-160.0f;if(span<80.0f)span=80.0f;
    float x=g_world_min_x+80.0f+(float)(rand()%1000)/1000.0f*span;
    float z=-55.0f+(float)(rand()%120);
    spawn_pickup_item(w,oid,x,z,-220.0f-(float)(rand()%80));
    *timer=420+(rand()%360); /* roughly every 14..26 seconds at 30 Hz */
}

static lf2_frame_def_t *object_frame_def(world_object_t *o){
    if(!o||!o->active||!o->cache||o->frame<0||o->frame>=LF2_MAX_FRAMES||!o->cache->def.frames[o->frame].present)return NULL;
    return &o->cache->def.frames[o->frame];
}

static void object_set_frame(world_object_t *o,int id){
    if(!o||!o->cache)return;
    if(id<0||id>=LF2_MAX_FRAMES||!o->cache->def.frames[id].present){o->active=false;return;}
    o->frame=id;o->ticks=0;o->opoint_fired=false;
    lf2_frame_def_t *fr=&o->cache->def.frames[id];
    if(fr->dvx==550)o->vx=0;else if(fr->dvx)o->vx=(float)(o->facing?-fr->dvx:fr->dvx);
    if(fr->dvy==550)o->vy=0;else if(fr->dvy)o->vy=(float)fr->dvy;
    if(fr->dvz==550)o->vz=0;else if(fr->dvz)o->vz=(float)fr->dvz;
    if(fr->sound[0])lf2_audio_play(fr->sound);
}

static world_object_t *world_alloc_object(object_world_t *w){
    if(!w)return NULL;
    for(int i=0;i<LF2_MAX_ACTIVE_OBJECTS;i++)if(!w->objects[i].active){memset(&w->objects[i],0,sizeof(w->objects[i]));w->objects[i].active=true;return &w->objects[i];}
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

static void update_object(object_world_t *w,world_object_t *o,fighter_t *fighters,int total){
    if(!o||!o->active)return;
    if(o->arest_ticks>0)o->arest_ticks--;for(int ri=0;ri<=LF2_MAX_CPUS;ri++)if(o->vrest_ticks[ri]>0)o->vrest_ticks[ri]--;
    lf2_frame_def_t *fr=object_frame_def(o);if(!fr){o->active=false;return;}
    if(o->pickup_item){
        /* Loose stock items use their DAT animation instead of a synthetic
           spinner. In-air frames loop until contact, then 70->71->72->64
           (light items) or 21->20 (heavy items) plays once and remains at the
           stable authored ground frame. */
        if(!o->grounded){
            o->age++;o->x+=o->vx;o->y+=o->vy;o->z+=o->vz;o->vy+=0.62f;o->vx*=0.992f;o->vz*=0.992f;
            if(o->y>=0.0f){
                o->y=0.0f;o->vy=o->vx=o->vz=0.0f;o->grounded=true;
                object_set_frame(o,object_landing_frame(o->cache));
                lf2_logf("ITEM","land oid=%d name=%s x=%.1f z=%.1f frame=%d",o->oid,item_name(o->oid),o->x,o->z,o->frame);
                return;
            }
            o->ticks++;int duration=fr->wait+1;if(duration<1)duration=1;
            if(o->ticks>=duration){
                int next=fr->next;
                if(next==999||next==1000||next<0||next>=40||!o->cache->def.frames[next].present)next=0;
                object_set_frame(o,next);
            }
        }else{
            int stable=object_ground_frame(o->cache);
            if(o->frame!=stable){
                o->ticks++;int duration=fr->wait+1;if(duration<1)duration=1;
                if(o->ticks>=duration){
                    int next=fr->next;
                    if(next==999||next==1000||next<0||next>=LF2_MAX_FRAMES||
                       !o->cache->def.frames[next].present||next==0)next=stable;
                    object_set_frame(o,next);
                }
            }
        }
        return;
    }
    if(fr->state==9998){o->active=false;return;}
    o->age++;
    if(!o->opoint_fired&&fr->opoint_count>0){
        o->opoint_fired=true;
        for(int i=0;i<fr->opoint_count;i++){
            const lf2_opoint_def_t *op=&fr->opoints[i];
            int count=(op->facing>=20)?op->facing/10:1;if(count<1)count=1;if(count>8)count=8;
            for(int n=0;n<count;n++)object_spawn_child_one(w,o,op,n,count);
        }
    }
    if(o->attached){
        if(o->owner<0||o->owner>=total||!fighters[o->owner].active||fighters[o->owner].dead||fighters[o->owner].ice_sword_ticks<=0){o->active=false;return;}
        fighter_t *a=&fighters[o->owner];
        o->facing=a->facing;
