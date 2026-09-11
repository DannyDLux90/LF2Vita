
static int stage_asset_preload(stage_asset_cache_t *c,const lf2_stage_runtime_t *stage){
    if(!c||!stage)return -1;
    c->cap=lf2_roster_count;c->entries=(stage_fighter_asset_t*)calloc((size_t)c->cap,sizeof(*c->entries));
    if(!c->entries)return -2;
    for(int ph=0;ph<stage->phase_count;ph++){
        const lf2_stage_runtime_phase_t *p=&stage->phases[ph];
        for(int i=0;i<p->enemy_count;i++){
            int roster=p->enemies[i];
            if(!stage_asset_find(c,roster)&&!stage_asset_load(c,roster))return -3;
        }
    }
    lf2_log_stage("stage:preload_ready","fighters=%d",c->count);
    return 0;
}

static void stage_asset_shutdown(stage_asset_cache_t *c){
    if(!c||!c->entries)return;
    vita2d_wait_rendering_done();
    for(int i=0;i<c->count;i++)if(c->entries[i].used){
        if(c->entries[i].hud_face)vita2d_free_texture(c->entries[i].hud_face);
        lf2_free_character_textures(&c->entries[i].def);
    }
    free(c->entries);memset(c,0,sizeof(*c));
}

static int stage_spawn_cached_fighter(fighter_t *f,stage_asset_cache_t *cache,int roster_idx,
                                      float x,float z,int facing,int slot,int hp){
    stage_fighter_asset_t *a=stage_asset_find(cache,roster_idx);if(!a)return -1;
    memset(f,0,sizeof(*f));f->ai=true;f->active=true;f->team=2;f->x=x;f->z=z;f->facing=facing;
    f->roster_idx=roster_idx;f->transform_origin=-1;f->transform_target=-1;f->fusion_partner_slot=-1;f->holding_slot=-1;f->caught_by=-1;
    if(hp<=0)hp=500;f->hp=f->max_hp=hp;f->mp=500;
    /* Shallow-copy the immutable frame/sheet metadata; texture pointers and
       portrait remain owned by stage_asset_cache_t. */
    memcpy(&f->def,&a->def,sizeof(f->def));f->hud_face=a->hud_face;f->owns_assets=false;
    set_frame(f,0);
    lf2_logf("STAGE","reuse fighter slot=%d roster=%d name=%s hp=%d",slot,roster_idx,f->def.name,hp);
    return 0;
}

static void stage_preload_items(object_world_t *world,const lf2_stage_runtime_t *stage){
    if(!world||!stage)return;
    int loaded=0;
    for(int ph=0;ph<stage->phase_count;ph++)for(int i=0;i<stage->phases[ph].item_count;i++){
        int oid=stage->phases[ph].item_oid[i];
        if(object_cache_find_loaded(world,oid))continue;
        if(object_cache_get(world,oid))loaded++;
    }
    if(loaded)lf2_log_stage("stage:item_preload_ready","objects=%d",loaded);
}

static int stage_enemy_settled(const fighter_t *f){
    /* A Stage slot becomes reusable at the actual KO (HP=0), not after the
       corpse has completed its falling/lying animation. Rendering is decoupled
       from slot ownership: a corpse snapshot is preserved before reuse. */
    return !f||!f->active||f->dead;
}

static void stage_preserve_corpse(object_world_t *world,stage_asset_cache_t *cache,const fighter_t *f){
    if(!world||!cache||!f||!f->active||!f->dead)return;
    stage_fighter_asset_t *a=stage_asset_find(cache,f->roster_idx);
    if(!a){
        lf2_logf("STAGE","corpse snapshot skipped name=%s roster=%d: no shared asset",f->def.name,f->roster_idx);
        return;
    }
    if(world->corpse_count>=LF2_MAX_STAGE_CORPSES){
        /* Old corpses far behind the scrolling camera are the least useful.
           Recycle the oldest snapshot rather than blocking wave progression. */
        memmove(&world->corpses[0],&world->corpses[1],sizeof(world->corpses[0])*(LF2_MAX_STAGE_CORPSES-1));
        world->corpse_count=LF2_MAX_STAGE_CORPSES-1;
    }
    stage_corpse_t *c=&world->corpses[world->corpse_count++];memset(c,0,sizeof(*c));
    c->active=true;c->def=&a->def;c->facing=f->facing;c->x=f->x;c->y=0.0f;c->z=f->z;
    int lie=(c->facing&&a->def.frames[231].present)?231:230;
    if(lie<0||lie>=LF2_MAX_FRAMES||!a->def.frames[lie].present){
        lie=(f->frame>=0&&f->frame<LF2_MAX_FRAMES&&a->def.frames[f->frame].present)?f->frame:0;
    }
    c->frame=lie;
    lf2_logf("STAGE","corpse preserved name=%s frame=%d x=%.1f z=%.1f total=%d",a->def.name,c->frame,c->x,c->z,world->corpse_count);
}

static int stage_phase_settled(fighter_t *fighters){
    for(int i=1;i<=LF2_MAX_CPUS;i++)if(!stage_enemy_settled(&fighters[i]))return 0;
    return 1;
}

static float stage_enemy_x(const lf2_stage_runtime_phase_t *ph,int ordinal,float player_x){
    float authored=(float)(ph&&ph->bound>0?ph->bound:900)-120.0f-(float)(ordinal%4)*42.0f;
    float ahead=player_x+610.0f+(float)(ordinal%3)*28.0f;
    float x=authored>ahead?authored:ahead;
    float max=(float)(ph&&ph->bound>0?ph->bound:900)-40.0f;
    if(x>max)x=max;
    if(x<player_x+420.0f)x=player_x+420.0f;
    return x;
}

static int stage_fill_enemy_slots(fighter_t *fighters,const lf2_stage_runtime_phase_t *ph,
                                  int *queue_pos,int difficulty,stage_asset_cache_t *cache,object_world_t *world){
    int loaded=0;
    if(!fighters||!ph||!queue_pos||!cache)return 0;
    for(int slot=1;slot<=LF2_MAX_CPUS&&*queue_pos<ph->enemy_count;slot++){
        if(fighters[slot].active&&!stage_enemy_settled(&fighters[slot]))continue;
        if(fighters[slot].active){stage_preserve_corpse(world,cache,&fighters[slot]);unload_fighter_slot(&fighters[slot]);}
        int q=*queue_pos;
        float x=stage_enemy_x(ph,q,fighters[0].x);
        float z=spawn_z[slot];
        int roster=ph->enemies[q],hp=ph->hp[q]>0?ph->hp[q]:500;
        if(stage_spawn_cached_fighter(&fighters[slot],cache,roster,x,z,1,slot,hp)<0){
            lf2_logf("ERROR","stage enemy cache spawn failed phase-bound=%d q=%d roster=%d",ph->bound,q,roster);
            fighters[slot].active=false;fighters[slot].dead=true;
            (*queue_pos)++;
            continue;
        }
        fighters[slot].ai_cooldown=8+(slot*3);
        lf2_logf("STAGE","spawn phase-bound=%d slot=%d q=%d/%d fighter=%s hp=%d world_x=%.1f",
                 ph->bound,slot,q+1,ph->enemy_count,fighters[slot].def.name,hp,x);
        (*queue_pos)++;loaded++;
    }
    return loaded;
}

static int stage_spawn_phase_items(object_world_t *world,const lf2_stage_runtime_phase_t *ph){
    if(!world||!ph)return 0;int n=0;
    for(int i=0;i<ph->item_count;i++){
        int oid=ph->item_oid[i];
        float x=(float)ph->bound-(float)ph->item_x[i];
        float z=-38.0f+(float)((i*29)%82);
        if(spawn_pickup_item(world,oid,x,z,-165.0f-(float)((i%3)*34)))n++;
    }
    if(n)lf2_log_stage("stage:items","bound=%d count=%d",ph->bound,n);
    return n;
}

static void stage_camera_update(fighter_t *player,int bound){
    float max_cam=(float)bound-920.0f;if(max_cam<0)max_cam=0;
    float target=player?player->x-285.0f:0.0f;
    target=clampf(target,0.0f,max_cam);
    float d=target-g_camera_x;
    if(d>8.0f)d=8.0f;else if(d<-8.0f)d=-8.0f;
    g_camera_x+=d;
}

