                in=make_ai_input(p,target,difficulty);
                if(target&&!in_action(p)&&p->frame!=110)p->facing=(target->x<p->x);
            } else {
                in=make_player_input(&pad,pending_pressed);
            }
            pending_pressed=0;if(!handle_form_input(fighters,total,0,&in)){fighter_handle_item_input(world,p,&in);handle_catch_input(fighters,total,0,&in);update_control(p,&in);}process_frame_state(fighters,total,0,world);
            /* Human P1 keeps the direction chosen by movement. AI-controlled P1
               (Demo) faces its target just like the other computer slots. */
            for(int i=1;i<total;i++){
                fighter_t *c=&fighters[i];
                fighter_t *target=nearest_living_enemy(c,fighters,total);
                input_t ai=target?make_ai_input(c,target,difficulty):make_ai_retreat_input(c,nearest_lying_enemy(c,fighters,total));
                if(!handle_form_input(fighters,total,i,&ai)){fighter_handle_item_input(world,c,&ai);handle_catch_input(fighters,total,i,&ai);update_control(c,&ai);}process_frame_state(fighters,total,i,world);
                if(target&&!in_action(c)&&c->frame!=110)c->facing=(target->x<c->x);
            }

            sync_all_catches(fighters,total);
            check_firzen_fusion(fighters,total,world);tick_fusion(fighters,total);
            item_director_tick(world,&item_drop_timer);

            /* Opoints are the missing half of LF2 special attacks: the
               character frame only performs the casting animation; these
               entries instantiate the arrow/ball/wind object described by
               oid/action in the original 2.00a data. */
            for(int i=0;i<total;i++)spawn_fighter_opoints(world,fighters,total,i);
            for(int i=0;i<LF2_MAX_ACTIVE_OBJECTS;i++)if(world->objects[i].active)update_object(world,&world->objects[i],fighters,total);
            object_apply_reflections(world);

            int oldhp[LF2_MAX_CPUS+1];for(int i=0;i<total;i++)oldhp[i]=fighters[i].hp;
            for(int a=0;a<total;a++)for(int b=0;b<total;b++)if(a!=b)apply_hits(world,fighters,total,a,b,difficulty);
            for(int i=0;i<LF2_MAX_ACTIVE_OBJECTS;i++)if(world->objects[i].active)object_apply_hits(world,&world->objects[i],fighters,total,difficulty);
            update_hit_fx(world);
            for(int i=0;i<total;i++)if(fighters[i].hp!=oldhp[i]){
                if(i==0)lf2_logf("HIT","P1 %s hp %d -> %d",fighters[i].def.name,oldhp[i],fighters[i].hp);
                else lf2_logf("HIT","CPU%d %s hp %d -> %d",i,fighters[i].def.name,oldhp[i],fighters[i].hp);
            }
            acc_us-=sim_step_us;
        }
        draw_match(font,stage_bg,fighters,world,cpu_count,difficulty,cheat_enabled,false,options);
    }
    if(options->player_hp_out)*options->player_hp_out=fighters[0].hp;
    if(options->player_mp_out)*options->player_mp_out=fighters[0].mp;
    lf2_log_stage("match:cleanup","result=%d cpus=%d p1_hp=%d p1_mp=%d",result,cpu_count,fighters[0].hp,fighters[0].mp);vita2d_wait_rendering_done();
    for(int i=0;i<total;i++){if(fighters[i].hud_face)vita2d_free_texture(fighters[i].hud_face);lf2_free_character_textures(&fighters[i].def);}
    world_shutdown(world);free(world);free(fighters);lf2_log_memory("match cleanup");return result;
}

static void unload_fighter_slot(fighter_t *f){
    if(!f||!f->active){if(f){memset(f,0,sizeof(*f));f->dead=true;}return;}
    if(f->owns_assets){
        vita2d_wait_rendering_done();
        if(f->hud_face)vita2d_free_texture(f->hud_face);
        lf2_free_character_textures(&f->def);
    }
    memset(f,0,sizeof(*f));f->dead=true;f->active=false;
}

/* Stage waves reuse preloaded read-only fighter assets. In v0.67 every freed
   enemy slot parsed its DAT and synchronously uploaded all sprite sheets again
   inside the simulation tick; the hardware log showed ~100-130 ms stalls at
   wave_advance. Sharing one texture set per roster removes that frame hitch and
   also uses less CDRAM when a wave contains several identical Bandits/Hunters. */
typedef struct {
    bool used;
    int roster_idx;
    lf2_character_def_t def;
    vita2d_texture *hud_face;
} stage_fighter_asset_t;

typedef struct {
    stage_fighter_asset_t *entries;
    int count;
    int cap;
} stage_asset_cache_t;

static stage_fighter_asset_t *stage_asset_find(stage_asset_cache_t *c,int roster_idx){
    if(!c)return NULL;
    for(int i=0;i<c->count;i++)if(c->entries[i].used&&c->entries[i].roster_idx==roster_idx)return &c->entries[i];
    return NULL;
}

static stage_fighter_asset_t *stage_asset_load(stage_asset_cache_t *c,int roster_idx){
    stage_fighter_asset_t *hit=stage_asset_find(c,roster_idx);if(hit)return hit;
    if(!c||!c->entries||c->count>=c->cap||roster_idx<0||roster_idx>=lf2_roster_count)return NULL;
    stage_fighter_asset_t *a=&c->entries[c->count];memset(a,0,sizeof(*a));a->roster_idx=roster_idx;
    int rc=lf2_load_character(GAME_ROOT,lf2_roster[roster_idx].dat,&a->def);
    if(rc<0){lf2_logf("ERROR","stage preload parse failed roster=%d rc=%d",roster_idx,rc);return NULL;}
    rc=lf2_load_character_textures(GAME_ROOT,&a->def);
    if(rc<0){lf2_logf("ERROR","stage preload textures failed roster=%d rc=%d",roster_idx,rc);lf2_free_character_textures(&a->def);return NULL;}
    if(a->def.small[0]){
        a->hud_face=lf2_load_bmp_colorkey(a->def.small);
        if(a->hud_face)vita2d_texture_set_filters(a->hud_face,SCE_GXM_TEXTURE_FILTER_POINT,SCE_GXM_TEXTURE_FILTER_POINT);
    }
    a->used=true;c->count++;
    lf2_logf("STAGE","preload fighter roster=%d name=%s sheets=%d",roster_idx,a->def.name,a->def.sheet_count);
    return a;
}

static int stage_asset_preload(stage_asset_cache_t *c,const lf2_stage_runtime_t *stage){
    if(!c||!stage)return -1;
    c->cap=lf2_roster_count;c->entries=(stage_fighter_asset_t*)calloc((size_t)c->cap,sizeof(*c->entries));
    if(!c->entries)return -2;
    for(int i=0;i<stage->ally_count&&i<LF2_STAGE_MAX_ALLIES;i++){
        int roster=stage->ally_roster[i];
        if(roster>=0&&roster<lf2_roster_count&&!stage_asset_find(c,roster)&&!stage_asset_load(c,roster))return -3;
    }
    for(int ph=0;ph<stage->phase_count;ph++){
