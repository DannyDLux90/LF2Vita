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
