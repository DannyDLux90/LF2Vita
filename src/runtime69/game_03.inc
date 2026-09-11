static void update_control(fighter_t *f, const input_t *in) {
    static const input_t zero={0};
    if(!in)in=&zero;
    if(f->fall_points>0)f->fall_points--;
    if(f->hit_flash>0)f->hit_flash--;
    if(f->arest_ticks>0)f->arest_ticks--;
    for(int ri=0;ri<=LF2_MAX_CPUS;ri++)if(f->vrest_ticks[ri]>0)f->vrest_ticks[ri]--;
    if(f->sonata_damage_cooldown>0)f->sonata_damage_cooldown--;
    if(f->ice_sword_ticks>0)f->ice_sword_ticks--;
    if(f->drinking&&item_is_bottle(f->held_oid)&&!f->dead){
        /* Bottle healing is intentionally time-based rather than an instant
           pickup bonus. It follows the original 2.00a behaviour closely:
           milk mainly restores HP plus some MP, beer only restores MP. */
        /* PC state 17 can be interrupted by Defend. Releasing Square also
           stops the drink but keeps the remaining bottle units. */
        if(in->defend_pressed||(!in->attack&&f->drink_tick>3)){finish_drink(f,0);return;}
        f->drink_tick++;
        if(f->held_oid==122){
            if((f->drink_tick%5)==0 && f->hp<f->max_hp){f->hp+=8;if(f->hp>f->max_hp)f->hp=f->max_hp;}
            if((f->drink_tick%3)==0 && f->mp<500){f->mp+=5;if(f->mp>500)f->mp=500;}
        }else if(f->mp<500){f->mp+=6;if(f->mp>500)f->mp=500;}
        if(f->held_units>0)f->held_units--;
        if(f->frame<55||f->frame>58){if(f->def.frames[55].present)set_frame(f,55);}
        else update_animation(f,in);
        if(f->held_units<=0){finish_drink(f,1);return;}
        f->x=clampf(f->x,g_world_min_x,g_world_max_x);f->z=clampf(f->z,-70.0f,75.0f);
        return;
    }
    if(f->caught_by>=0 && !f->dead){
        /* The catcher owns position/frame progression via cpoint. The caught
           fighter cannot independently walk/attack until released. */
        f->vx=f->vy=f->vz=0.0f;
        return;
    }

    /* A dead fighter still has physics and a complete knock-down sequence.
       v0.64 stopped simulating the whole match as soon as HP reached zero,
       which left corpses suspended in the exact hit pose. */
    if(f->dead){
        f->frozen_ticks=0;f->burn_ticks=0;f->sonata_ticks=0;
        if(f->y<0.0f||f->vy!=0.0f||f->falling){
            if(!f->falling){f->falling=true;if(f->def.frames[180].present)set_frame(f,180);}
            fighter_physics_step(f);
            if(f->falling)update_animation(f,NULL);
        } else {
            /* Keep the final lying frame; do not let 230/231 walk through
               frame 219 back into a standing pose after death. */
            if(f->frame!=230&&f->frame!=231){
                int lie=(f->facing&&f->def.frames[231].present)?231:230;
                if(f->def.frames[lie].present)set_frame(f,lie);
            }
        }
        return;
    }

    if(f->sonata_ticks>0){
        f->sonata_ticks--;f->status_age++;
        /* Hover/lift toward roughly one character height above the floor. */
        if(f->y>-118.0f)f->y-=2.35f;
        if(f->y<-118.0f)f->y=-118.0f;
        f->vx*=0.70f;f->vz*=0.75f;f->vy=0.0f;
        int fr=180+((f->status_age/2)&3);
        if(f->def.frames[fr].present&&f->frame!=fr)set_frame(f,fr);
        if(f->sonata_ticks==0){
            f->falling=true;f->vy=0.0f;
            if(f->def.frames[180].present)set_frame(f,180);
            lf2_logf("STATUS","%s sonata released y=%.1f",f->def.name,f->y);
        }
        f->x=clampf(f->x,g_world_min_x,g_world_max_x);f->z=clampf(f->z,-70.0f,75.0f);
        return;
    }

    if(f->frozen_ticks>0){
        f->frozen_ticks--;f->status_age++;
        /* First three ticks use the ice impact frame, then the authored
           frozen frame. A frozen airborne fighter still falls as a block. */
        if(f->status_age==3&&f->def.frames[201].present)set_frame(f,201);
        if(f->y<0.0f||f->vy!=0.0f)fighter_physics_step(f);
        if(f->frozen_ticks==0){
            f->falling=true;
            if(f->def.frames[202].present)set_frame(f,202);
            else if(f->def.frames[182].present)set_frame(f,182);
            if(f->vy==0.0f)f->vy=-3.0f;
            lf2_logf("STATUS","%s thaw -> falling",f->def.name);
        }
        return;
    }

    if(f->burn_ticks>0){
        f->burn_ticks--;f->status_age++;
        fighter_physics_step(f);
        update_animation(f,NULL); /* 203<->204 or 205<->206 while burning */
        if(f->burn_ticks==0){
            f->falling=true;
            if(f->def.frames[180].present)set_frame(f,180);
            if(f->vy==0.0f)f->vy=-5.0f;
            lf2_logf("STATUS","%s burn ended -> falling",f->def.name);
        }
        return;
    }

    bool locked=in_action(f)||f->frame==110||f->y<0.0f;
    if(!locked){
        int cmd=resolve_command_frame(f,in);
        if(cmd>0)start_special_frame(f,cmd);
        else if(in->attack_pressed)start_attack(f);
        else if(in->defend_pressed)set_frame(f,110);
        else if(in->jump_pressed){
            set_frame(f,210);f->vy=f->def.jump_height;
            if(in->left)f->vx=-f->def.jump_distance;if(in->right)f->vx=f->def.jump_distance;
            if(in->up)f->vz=-f->def.jump_distancez;if(in->down)f->vz=f->def.jump_distancez;
        } else {
            bool heavy=f->held_oid&&item_is_heavy(f->held_oid);
            bool running=in->run&&f->def.frames[heavy?16:9].present;
            float sx=heavy?(running?f->def.heavy_running_speed:f->def.heavy_walking_speed):(running?f->def.running_speed:f->def.walking_speed);
            float sz=heavy?(running?f->def.heavy_running_speedz:f->def.heavy_walking_speedz):(running?f->def.running_speedz:f->def.walking_speedz);bool moving=false;
            if(in->left){f->x-=sx;f->facing=1;moving=true;}if(in->right){f->x+=sx;f->facing=0;moving=true;}
            if(in->up){f->z-=sz;moving=true;}if(in->down){f->z+=sz;moving=true;}
            if(moving){
                f->walk_phase++;int first=heavy?(running?16:12):(running?9:5),last=heavy?(running?18:15):(running?11:8),count=last-first+1;
                float rate=running?f->def.running_frame_rate:f->def.walking_frame_rate;int period=(int)(rate+0.5f);if(period<1)period=1;
                int wf=first+(f->walk_phase/period)%count;if(f->frame<first||f->frame>last||f->frame!=wf)set_frame(f,wf);
            }else if(heavy&&f->frame>=16&&f->frame<=18&&f->def.frames[19].present)set_frame(f,19);
            else if((f->frame>=5&&f->frame<=19))set_frame(f,0);
        }
    }else if(f->frame==110&&!in->defend&&f->ticks>4)set_frame(f,0);

    fighter_physics_step(f);
    /* Keep regeneration deliberately slower than the continuous costs. */
    if(f->mp<500 && ++f->mp_regen_tick>=2){f->mp++;f->mp_regen_tick=0;}
    update_animation(f,in);
}

static bool rect_overlap_i(int ax,int ay,int aw,int ah,int bx,int by,int bw,int bh) {
    return lf2_rect_overlap(ax,ay,(unsigned)aw,(unsigned)ah,bx,by,(unsigned)bw,(unsigned)bh);
}

static void region_world(const fighter_t *f,int rx,int ry,int rw,int rh,int *x,int *y) {
    lf2_frame_def_t *fr=(lf2_frame_def_t*)&f->def.frames[f->frame];
    if (!f->facing) *x=(int)f->x-fr->centerx+rx;
    else *x=(int)f->x+1+fr->centerx-rw-rx;
    *y=(int)f->y-fr->centery+ry;
}

static void cpoint_world_xy(const fighter_t *f,const lf2_cpoint_def_t *cp,float *x,float *y){
    if(!f||!cp||!x||!y)return;const lf2_frame_def_t *fr=&f->def.frames[f->frame];
    *x=!f->facing?f->x-(float)fr->centerx+(float)cp->x:f->x+(float)fr->centerx-(float)cp->x;
    *y=f->y-(float)fr->centery+(float)cp->y;
}

static void release_caught_pair(fighter_t *fighters,int total,int holder_idx,int victim_idx){
    if(!fighters||holder_idx<0||holder_idx>=total||victim_idx<0||victim_idx>=total)return;
    fighter_t *h=&fighters[holder_idx],*v=&fighters[victim_idx];
    if(h->holding_slot==victim_idx)h->holding_slot=-1;
    if(v->caught_by==holder_idx)v->caught_by=-1;
}

static int caught_is_hurtable(fighter_t *fighters,int total,int victim_idx,int attacker_idx){
    if(!fighters||victim_idx<0||victim_idx>=total)return 1;fighter_t *v=&fighters[victim_idx];
    if(v->caught_by<0||v->caught_by>=total)return 1;
    if(attacker_idx==v->caught_by)return 0; /* catch damage is cpoint-driven */
    fighter_t *h=&fighters[v->caught_by];lf2_frame_def_t *hf=frame_def(h);
    return hf&&hf->cpoint.present&&hf->cpoint.kind==1&&hf->cpoint.hurtable!=0;
}

static void sync_catch_pair(fighter_t *fighters,int total,int holder_idx){
    if(!fighters||holder_idx<0||holder_idx>=total)return;fighter_t *h=&fighters[holder_idx];int vi=h->holding_slot;
    if(vi<0||vi>=total)return;fighter_t *v=&fighters[vi];
    if(!h->active||h->dead||!v->active||v->dead||v->caught_by!=holder_idx){release_caught_pair(fighters,total,holder_idx,vi);return;}
    lf2_frame_def_t *hf=frame_def(h);if(!hf||!hf->cpoint.present||hf->cpoint.kind!=1){release_caught_pair(fighters,total,holder_idx,vi);return;}
    lf2_cpoint_def_t *hc=&hf->cpoint;
    if(hc->vaction){int va=hc->vaction<0?-hc->vaction:hc->vaction;if(va>0&&va<LF2_MAX_FRAMES&&v->def.frames[va].present&&v->frame!=va)set_frame_signed(v,hc->vaction);}
    lf2_frame_def_t *vf=frame_def(v);float tx=h->x+(h->facing?-36.0f:36.0f),ty=h->y-38.0f;
    cpoint_world_xy(h,hc,&tx,&ty);
    if(vf&&vf->cpoint.present&&vf->cpoint.kind==2){
        lf2_cpoint_def_t *vc=&vf->cpoint;
        v->x=!v->facing?tx+(float)vf->centerx-(float)vc->x:tx-(float)vf->centerx+(float)vc->x;
        v->y=ty+(float)vf->centery-(float)vc->y;
    }else{v->x=tx;v->y=ty;}
    v->z=h->z;v->vx=v->vy=v->vz=0.0f;
}

static void sync_all_catches(fighter_t *fighters,int total){for(int i=0;i<total;i++)if(fighters[i].active&&fighters[i].holding_slot>=0)sync_catch_pair(fighters,total,i);}

static void handle_catch_input(fighter_t *fighters,int total,int holder_idx,const input_t *in){
    if(!fighters||!in||holder_idx<0||holder_idx>=total)return;fighter_t *h=&fighters[holder_idx];int vi=h->holding_slot;
    if(vi<0||vi>=total||fighters[vi].caught_by!=holder_idx)return;lf2_frame_def_t *hf=frame_def(h);
    if(!hf||!hf->cpoint.present||hf->cpoint.kind!=1)return;lf2_cpoint_def_t *cp=&hf->cpoint;
    if(cp->dircontrol){if(in->left)h->facing=1;else if(in->right)h->facing=0;}
    int dir=in->left||in->right||in->up||in->down;
    if(in->attack_pressed&&dir&&cp->taction)set_frame_signed(h,cp->taction);
    else if(in->attack_pressed&&cp->aaction)set_frame_signed(h,cp->aaction);
    else if(in->jump_pressed&&cp->jaction)set_frame_signed(h,cp->jaction);
}

static void process_cpoint_event(fighter_t *fighters,int total,int holder_idx){
    if(!fighters||holder_idx<0||holder_idx>=total)return;fighter_t *h=&fighters[holder_idx];int vi=h->holding_slot;
    if(vi<0||vi>=total)return;fighter_t *v=&fighters[vi];if(v->caught_by!=holder_idx)return;
    lf2_frame_def_t *hf=frame_def(h);if(!hf||!hf->cpoint.present||hf->cpoint.kind!=1)return;lf2_cpoint_def_t *cp=&hf->cpoint;
    if(cp->injury>0){int old=v->hp;v->hp-=cp->injury;if(v->hp<0)v->hp=0;if(v->hp==0)v->dead=true;lf2_logf("GRAB","%s catch hit %s hp=%d->%d frame=%d",h->def.name,v->def.name,old,v->hp,h->frame);}
    if(v->dead){release_caught_pair(fighters,total,holder_idx,vi);return;}
    if(cp->throwvx||cp->throwvy){
        if(cp->vaction)set_frame_signed(v,cp->vaction);
        if(cp->throwinjury>0){v->hp-=cp->throwinjury;if(v->hp<0)v->hp=0;if(v->hp==0)v->dead=true;}
        v->falling=true;v->thrown_by=holder_idx;v->vx=(float)(h->facing?-cp->throwvx:cp->throwvx);v->vy=(float)cp->throwvy;
        if(cp->throwvz){int z=abs(cp->throwvz);v->vz=(float)((rand()%(z*2+1))-z);}else v->vz=0.0f;
        release_caught_pair(fighters,total,holder_idx,vi);
        lf2_logf("GRAB","%s threw %s frame=%d vel=%.1f,%.1f,%.1f injury=%d",h->def.name,v->def.name,h->frame,v->vx,v->vy,v->vz,cp->throwinjury);
    }
}

static int are_enemies(const fighter_t *a,const fighter_t *b){
    if(!a||!b||a==b)return 0;
    /* Independent fighters are hostile to everybody. Team members only spare
       fighters that share the same numbered team. */
    return a->team==0 || b->team==0 || a->team!=b->team;
}

static const object_catalog_entry_t *object_catalog_find(int oid){
    for(size_t i=0;i<sizeof(g_object_catalog)/sizeof(g_object_catalog[0]);i++)
        if(g_object_catalog[i].oid==oid)return &g_object_catalog[i];
    return NULL;
}

static object_cache_entry_t *object_cache_get(object_world_t *w,int oid){
    if(!w)return NULL;
    for(int i=0;i<w->cache_count;i++)if(w->cache[i].used&&w->cache[i].oid==oid)return &w->cache[i];
    const object_catalog_entry_t *cat=object_catalog_find(oid);
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
    /* weapon_hp is durability for weapons, but is NOT drink duration. Stock
       Milk/Beer use 100/125 time units respectively. */
    o->durability=item_is_bottle(oid)?item_units_for_oid(oid):(cache->def.weapon_hp>0?cache->def.weapon_hp:item_units_for_oid(oid));
    o->grounded=(y>=0.0f);o->vy=o->grounded?0.0f:0.4f;
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

