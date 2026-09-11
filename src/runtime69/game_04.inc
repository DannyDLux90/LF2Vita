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
        /* Vita explicit throw command: while an item is held, L+Square always
           enters the stock LF2 throw frame chain. L by itself remains pickup
           only when the fighter has empty hands. */
        if((in->pickup&&in->attack_pressed)||(in->pickup_pressed&&in->attack))throw_now=1;
        if(!throw_now&&in->attack_pressed){
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
    f->held_oid=o->oid;
    f->held_units=o->durability>0?o->durability:(item_is_bottle(o->oid)?item_units_for_oid(o->oid):(o->cache&&o->cache->def.weapon_hp>0?o->cache->def.weapon_hp:item_units_for_oid(o->oid)));
    f->drinking=false;f->drink_tick=0;
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
        o->x=a->x+(a->facing?-o->attach_dx:o->attach_dx);o->y=a->y+o->attach_dy;o->z=a->z+o->attach_dz;
        return;
    }
    /* hit_Fa on a ball is LF2's chase flag. Apply a conservative homing turn
       rather than teleporting so Dennis/John-style chase objects remain visible. */
    if(o->type==3&&fr->hit_Fa>0){fighter_t *t=object_nearest_enemy(o,fighters,total);if(t){float dx=t->x-o->x,dy=(t->y-35.0f)-o->y,dz=t->z-o->z;float len=sqrtf(dx*dx+dy*dy+dz*dz);if(len>1.0f){float speed=sqrtf(o->vx*o->vx+o->vy*o->vy+o->vz*o->vz);if(speed<4.0f)speed=7.0f;float turn=fr->hit_Fa>=7?0.20f:0.11f;o->vx=o->vx*(1.0f-turn)+dx/len*speed*turn;o->vy=o->vy*(1.0f-turn)+dy/len*speed*turn;o->vz=o->vz*(1.0f-turn)+dz/len*speed*turn;}}}
    o->x+=o->vx;o->y+=o->vy;o->z+=o->vz;
    if(o->type==1&&fr->state==1002)o->vy+=0.45f;
    if(o->type==2&&fr->state==2000)o->vy+=0.45f;
    if(o->type==1&&o->y>=2.0f&&fr->state==1002){
        o->y=0.0f;o->vx=o->vy=o->vz=0.0f;
        if(o->recoverable_weapon){o->pickup_item=true;o->grounded=true;o->owner=-1;o->team=0;object_set_frame(o,object_landing_frame(o->cache));}
        else if(o->cache->def.frames[60].present)object_set_frame(o,60);else o->active=false;
    }
    if(o->type==2&&o->y>=2.0f&&fr->state==2000){
        o->y=0.0f;o->vx=o->vy=o->vz=0.0f;
        if(o->recoverable_weapon){o->pickup_item=true;o->grounded=true;o->owner=-1;o->team=0;object_set_frame(o,object_landing_frame(o->cache));}
        else if(o->cache->def.frames[20].present)object_set_frame(o,20);else o->active=false;
    }
    if(!o->active)return;
    if(o->x<-180||o->x>1140||o->z<-180||o->z>180||o->y>180||o->age>420){o->active=false;return;}
    fr=object_frame_def(o);if(!fr)return;o->ticks++;int duration=fr->wait+1;if(duration<1)duration=1;if(o->ticks<duration)return;
    int next=fr->next;if(next<0){o->facing^=1;next=-next;}
    if(next==999)next=0;else if(next==1000){o->active=false;return;}
    object_set_frame(o,next);
}

static void spawn_hit_fx(object_world_t *w,const fighter_t *victim,int effect){
    if(!w||!victim)return;int kind=(effect==1)?1:((effect==2||effect==20||effect==21||effect==22)?2:((effect==3||effect==30)?3:0));
    int count=kind==1?10:6;for(int n=0;n<count;n++){hit_fx_t *fx=NULL;for(int i=0;i<LF2_MAX_HIT_FX;i++)if(!w->fx[i].active){fx=&w->fx[i];break;}if(!fx)break;
        fx->active=true;fx->kind=kind;fx->x=victim->x+(float)((rand()%17)-8);fx->y=victim->y-43.0f+(float)((rand()%17)-8);fx->z=victim->z;
        fx->vx=(float)((rand()%101)-50)/18.0f;fx->vy=-(float)(25+rand()%60)/14.0f;fx->life=kind==1?18:12;}
}

static void play_hit_sound(int effect,int fall,int injury){
    const char *snd="data/001.wav";
    if(effect==1)snd=(fall>=60||injury>=60)?"data/033.wav":"data/032.wav";
    else if(effect==2||effect==20||effect==21||effect==22)snd=(injury>=60)?"data/071.wav":"data/070.wav";
    else if(effect==3||effect==30)snd="data/065.wav";
    else snd=(fall>=60||injury>=60)?"data/006.wav":"data/001.wav";
    lf2_audio_play(snd);
}

static int is_fire_effect(int effect){return effect==2||effect==20||effect==21||effect==22;}
static int is_ice_effect(int effect){return effect==3||effect==30;}

static int apply_elemental_status(fighter_t *b,int effect,int attacker_facing,const lf2_itr_def_t *it){
    if(!b||!it)return 0;
    if(is_ice_effect(effect)){
        b->burn_ticks=0;b->sonata_ticks=0;b->status_age=0;
        b->frozen_ticks=94; /* 200 intro + 201's authored ~90 TU hold */
        b->falling=false;b->vx=0;b->vz=0;
        if(b->def.frames[200].present)set_frame(b,200);
        lf2_logf("STATUS","%s frozen ticks=%d effect=%d",b->def.name,b->frozen_ticks,effect);
        return 1;
    }
    if(is_fire_effect(effect)){
        b->frozen_ticks=0;b->sonata_ticks=0;b->status_age=0;
        b->burn_ticks=26;
        b->falling=false;
        float push=(float)(it->dvx?it->dvx:6);
        b->vx=attacker_facing?-fabsf(push):fabsf(push);
        b->vy=(float)(it->dvy?it->dvy:-6);
        int fr=(b->facing==attacker_facing)?203:205;
        if(!b->def.frames[fr].present)fr=b->def.frames[203].present?203:220;
        set_frame(b,fr);
        lf2_logf("STATUS","%s burning ticks=%d effect=%d frame=%d",b->def.name,b->burn_ticks,effect,fr);
        return 1;
    }
    return 0;
}
