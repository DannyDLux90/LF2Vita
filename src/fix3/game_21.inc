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

            for(int i=0;i<total;i++)fighters[i].direct_hurt_latched=false;
            int oldhp[LF2_MAX_CPUS+1];for(int i=0;i<total;i++)oldhp[i]=fighters[i].hp;
            bool stock_compat=options->lockstep_actor_map_enabled;
            for(int a=0;a<total;a++)for(int b=0;b<total;b++)if(a!=b)apply_hits(world,fighters,total,a,b,difficulty,stock_compat);
            for(int i=0;i<LF2_MAX_ACTIVE_OBJECTS;i++)if(world->objects[i].active)object_apply_hits(world,&world->objects[i],fighters,total,difficulty,stock_compat);
            update_hit_fx(world);
            for(int i=0;i<total;i++)if(fighters[i].hp!=oldhp[i]){
                if(i==0)lf2_logf("HIT","P1 %s hp %d -> %d",fighters[i].def.name,oldhp[i],fighters[i].hp);
                else if(fighters[i].human_no==2)lf2_logf("HIT","P2 %s hp %d -> %d",fighters[i].def.name,oldhp[i],fighters[i].hp);
                else lf2_logf("HIT","CPU%d %s hp %d -> %d",i,fighters[i].def.name,oldhp[i],fighters[i].hp);
            }
            if(stock_compat){
                stock_tu++;
                if(stock_tu==1||(stock_tu%30u)==0u){
                    uint32_t digest=stock_state_digest(fighters,total,world,item_drop_timer);
                    lf2_logf("STOCK","tu=%u packet=%u digest=%08X rng_i=%u rng_j=%u rng_calls=%u",
                             (unsigned)stock_tu,(unsigned)((stock_tu-1u)/2u),digest,
                             (unsigned)lf2_rng_stock_i(),(unsigned)lf2_rng_stock_j(),(unsigned)lf2_rng_calls());
                }
                if(reference_mode&&stock_tu>=options->reference_tu_limit){
                    int ref_side_alive=player_side_alive(fighters,total);
                    int ref_enemies_alive=player_has_enemy(fighters,total);
                    result=(ref_side_alive&&!ref_enemies_alive)?LF2_MATCH_WIN:
                           (!ref_side_alive&&ref_enemies_alive)?LF2_MATCH_LOSS:LF2_MATCH_CANCEL;
                    lf2_logf("REF","exact TU limit reached tu=%u result=%d side_alive=%d enemies_alive=%d rng_calls=%u",
                             (unsigned)stock_tu,result,ref_side_alive,ref_enemies_alive,(unsigned)lf2_rng_calls());
                    reference_done=true;
                }
            }
            acc_us-=sim_step_us;
            if(reference_done)break;
        }
        if(network_abort||reference_done)break;
        draw_match(font,stage_bg,fighters,world,cpu_count,difficulty,cheat_enabled,false,options);
    }
    if(options->player_hp_out)*options->player_hp_out=fighters[0].hp;
    if(options->player_mp_out)*options->player_mp_out=fighters[0].mp;
    if(options->stock_stats_count_out)*options->stock_stats_count_out=loaded;
    if(options->stock_stats_out&&options->stock_stats_capacity>0){
        int n=loaded<options->stock_stats_capacity?loaded:options->stock_stats_capacity;
        for(int i=0;i<n;i++)options->stock_stats_out[i]=(lf2_stock_stat_t){
            .roster_idx=fighters[i].roster_idx,.team=fighters[i].team,.hp=fighters[i].hp,
            .attack=fighters[i].stat_attack,.hp_lost=fighters[i].stat_hp_lost,.kills=fighters[i].stat_kills,
            .mp_used=fighters[i].stat_mp_used,.picking=fighters[i].stat_picking,.dead=fighters[i].dead?1:0};
    }
    lf2_log_stage("match:cleanup","result=%d cpus=%d p1_hp=%d p1_mp=%d",result,cpu_count,fighters[0].hp,fighters[0].mp);
    if(options->lockstep_actor_map_enabled){
        for(int i=0;i<loaded;i++)lf2_logf("STOCKSTAT","actor=%d char=%s team=%d hp=%d attack=%d hp_lost=%d kills=%d mp_used=%d picking=%d",
            i,fighters[i].def.name,fighters[i].team,fighters[i].hp,fighters[i].stat_attack,fighters[i].stat_hp_lost,
            fighters[i].stat_kills,fighters[i].stat_mp_used,fighters[i].stat_picking);
    }
    vita2d_wait_rendering_done();
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
