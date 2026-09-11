                if(now_us-result_since>=2200000ULL){result=outcome;break;}
            } else if(options->allow_rematch){
                if(raw_pressed&SCE_CTRL_CIRCLE){result=LF2_MATCH_CANCEL;break;}
                if(raw_pressed&SCE_CTRL_CROSS){
                    lf2_logf("GAME","rematch cpus=%d",cpu_count);
                    reset_match(fighters,cpu_count);world_clear_active(world);item_drop_timer=240+(rand()%180);last_us=sceKernelGetSystemTimeWide();acc_us=0;result_since=0;
                }
            } else {
                if(raw_pressed&SCE_CTRL_CIRCLE){result=LF2_MATCH_CANCEL;break;}
                if(raw_pressed&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE)){result=outcome;break;}
            }
            draw_match(font,stage_bg,fighters,world,cpu_count,difficulty,cheat_enabled,false,options);continue;
        }
        result_since=0;

        acc_us+=dt_us;
        int sim_guard=0;
        while(acc_us>=sim_step_us && sim_guard++<4){
            fighter_t *p=&fighters[0];
            input_t in;
            if(options->player_ai){
                fighter_t *target=nearest_living_enemy(p,fighters,total);
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
       corpse has completed its falling/lying animation. This removes the last
       artificial inter-wave gap; ordinary VS still renders full KO settling. */
    return !f||!f->active||f->dead;
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
                                  int *queue_pos,int difficulty,stage_asset_cache_t *cache){
    int loaded=0;
    if(!fighters||!ph||!queue_pos||!cache)return 0;
    for(int slot=1;slot<=LF2_MAX_CPUS&&*queue_pos<ph->enemy_count;slot++){
        if(fighters[slot].active&&!stage_enemy_settled(&fighters[slot]))continue;
        if(fighters[slot].active)unload_fighter_slot(&fighters[slot]);
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

int lf2_run_stage_section(vita2d_pgf *font, vita2d_texture *stage_bg,
                          int player_index, int difficulty, bool cheat_enabled,
                          const lf2_stage_runtime_t *stage){
    if(!stage||stage->phase_count<1||stage->phase_count>LF2_STAGE_RUNTIME_MAX_PHASES||
       player_index<0||player_index>=lf2_roster_count)return -1;
    lf2_audio_music_stop();
    g_runtime_cheat=cheat_enabled;
    const int total=LF2_MAX_CPUS+1;
    float old_min=g_world_min_x,old_max=g_world_max_x,old_cam=g_camera_x;
    g_world_min_x=40.0f;g_camera_x=0.0f;
    int final_bound=stage->phases[stage->phase_count-1].bound;
    if(final_bound<920)final_bound=920;
    g_world_max_x=(float)stage->phases[0].bound-40.0f;if(g_world_max_x<920.0f)g_world_max_x=920.0f;

    fighter_t *fighters=(fighter_t*)calloc(LF2_MAX_CPUS+1,sizeof(*fighters));
    object_world_t *world=(object_world_t*)calloc(1,sizeof(*world));
    if(!fighters||!world){free(fighters);free(world);g_world_min_x=old_min;g_world_max_x=old_max;g_camera_x=old_cam;return -2;}
    for(int i=1;i<=LF2_MAX_CPUS;i++)fighters[i].dead=true;
    int start_hp=stage->player_hp>0?stage->player_hp:500;
    if(load_fighter(&fighters[0],player_index,false,1,165.0f,0.0f,0,1,500)<0){
        free(world);free(fighters);g_world_min_x=old_min;g_world_max_x=old_max;g_camera_x=old_cam;return -3;
    }
    fighters[0].hp=start_hp;if(fighters[0].hp>fighters[0].max_hp)fighters[0].hp=fighters[0].max_hp;
    fighters[0].mp=(stage->player_mp>=0&&stage->player_mp<=500)?stage->player_mp:500;

    stage_asset_cache_t stage_cache={0};
    if(stage_asset_preload(&stage_cache,stage)<0){
        lf2_logf("ERROR","Stage fighter preloading failed");
        unload_fighter_slot(&fighters[0]);world_shutdown(world);free(world);free(fighters);
        stage_asset_shutdown(&stage_cache);
        g_world_min_x=old_min;g_world_max_x=old_max;g_camera_x=old_cam;return -4;
    }
    /* Stage-authored milk/beer/weapons are cached before the simulation starts,
       so their first appearance cannot stall a wave transition either. */
    stage_preload_items(world,stage);

    int phase=0,queue_pos=0;
    char hud[96];snprintf(hud,sizeof(hud),"%s",stage->hud_prefix?stage->hud_prefix:"Stage");
    lf2_match_options_t opt={0};opt.allow_rematch=false;opt.allow_cancel=true;opt.hud_label=hud;opt.suppress_result_overlay=true;
    stage_fill_enemy_slots(fighters,&stage->phases[phase],&queue_pos,difficulty,&stage_cache);
    stage_spawn_phase_items(world,&stage->phases[phase]);
    lf2_log_stage("stage:continuous_start","label=%s phases=%d first_bound=%d final_bound=%d",
                  stage->hud_prefix?stage->hud_prefix:"Stage",stage->phase_count,stage->phases[0].bound,final_bound);

    uint32_t prev=0;SceCtrlData first={0};sceCtrlPeekBufferPositive(0,&first,1);prev=first.buttons;
    uint32_t pending_pressed=0;bool paused=false;int result=LF2_MATCH_CANCEL;
    const uint64_t sim_step_us=33333;
    uint64_t last_us=sceKernelGetSystemTimeWide(),acc_us=0,result_since=0;
    srand(0x53544765u+(unsigned)player_index*31u+(unsigned)difficulty*113u);

    for(;;){
        SceCtrlData pad={0};sceCtrlPeekBufferPositive(0,&pad,1);
        uint32_t raw_pressed=pad.buttons&~prev;prev=pad.buttons;pending_pressed|=raw_pressed;
        uint64_t now=sceKernelGetSystemTimeWide(),dt=now-last_us;last_us=now;if(dt>100000)dt=100000;
        if(raw_pressed&SCE_CTRL_START){paused=!paused;acc_us=0;pending_pressed=0;}
        if(paused){
            if(raw_pressed&SCE_CTRL_CIRCLE){result=LF2_MATCH_CANCEL;break;}
            draw_match(font,stage_bg,fighters,world,LF2_MAX_CPUS,difficulty,cheat_enabled,true,&opt);continue;
        }

        if(fighters[0].dead){
            if(!result_since)result_since=now;
            acc_us+=dt;input_t zero={0};int guard=0;
            while(acc_us>=sim_step_us&&guard++<4){
                for(int i=0;i<=LF2_MAX_CPUS;i++)if(fighters[i].active)update_control(&fighters[i],&zero);
                for(int i=0;i<LF2_MAX_ACTIVE_OBJECTS;i++)if(world->objects[i].active)update_object(world,&world->objects[i],fighters,total);
                update_hit_fx(world);stage_camera_update(&fighters[0],stage->phases[phase].bound);acc_us-=sim_step_us;
            }
            draw_match(font,stage_bg,fighters,world,LF2_MAX_CPUS,difficulty,cheat_enabled,false,&opt);
            if((dead_bodies_settled(fighters,total)&&now-result_since>650000ULL)||now-result_since>2200000ULL){result=LF2_MATCH_LOSS;break;}
            continue;
        }

        result_since=0;acc_us+=dt;int guard=0;
        while(acc_us>=sim_step_us&&guard++<4){
            input_t in=make_player_input(&pad,pending_pressed);pending_pressed=0;
            if(!handle_form_input(fighters,total,0,&in)){fighter_handle_item_input(world,&fighters[0],&in);handle_catch_input(fighters,total,0,&in);update_control(&fighters[0],&in);}
            process_frame_state(fighters,total,0,world);
            for(int i=1;i<total;i++)if(fighters[i].active){
                fighter_t *c=&fighters[i];
                if(c->dead){input_t zero={0};update_control(c,&zero);continue;}
                /* Enemies authored for the next screen wait just beyond the
                   viewport instead of rushing backwards through the camera. */
                if(screen_x(c->x)>930.0f)continue;
                fighter_t *target=nearest_living_enemy(c,fighters,total);
                input_t ai=target?make_ai_input(c,target,difficulty):make_ai_retreat_input(c,nearest_lying_enemy(c,fighters,total));
                if(!handle_form_input(fighters,total,i,&ai)){fighter_handle_item_input(world,c,&ai);handle_catch_input(fighters,total,i,&ai);update_control(c,&ai);}
                process_frame_state(fighters,total,i,world);
                if(target&&!in_action(c)&&c->frame!=110)c->facing=(target->x<c->x);
            }

            sync_all_catches(fighters,total);
