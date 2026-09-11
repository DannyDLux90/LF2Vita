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
    int total=1;
    float old_min=g_world_min_x,old_max=g_world_max_x,old_cam=g_camera_x;
    g_world_min_x=40.0f;g_camera_x=0.0f;
    int final_bound=stage->phases[stage->phase_count-1].bound;
    if(final_bound<920)final_bound=920;
    g_world_max_x=(float)stage->phases[0].bound-40.0f;if(g_world_max_x<920.0f)g_world_max_x=920.0f;

    int ally_count=stage->ally_count;if(ally_count<0)ally_count=0;if(ally_count>LF2_STAGE_MAX_ALLIES)ally_count=LF2_STAGE_MAX_ALLIES;
    int first_enemy_slot=1+ally_count;
    int last_enemy_slot=first_enemy_slot+LF2_STAGE_ENEMY_SLOTS-1;
    total=last_enemy_slot+1;
    if(total>LF2_MAX_ACTORS)total=LF2_MAX_ACTORS;
    fighter_t *fighters=(fighter_t*)calloc((size_t)total,sizeof(*fighters));
    object_world_t *world=(object_world_t*)calloc(1,sizeof(*world));
    if(!fighters||!world){free(fighters);free(world);g_world_min_x=old_min;g_world_max_x=old_max;g_camera_x=old_cam;return -2;}
    for(int i=1;i<total;i++)fighters[i].dead=true;

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
    for(int i=0;i<ally_count;i++){
        int slot=i+1,roster=stage->ally_roster[i];
        int hp=stage->ally_hp[i]>0?stage->ally_hp[i]:500;
        int mp=(stage->ally_mp[i]>=0&&stage->ally_mp[i]<=500)?stage->ally_mp[i]:500;
        float ax=165.0f-(float)((i+1)*42),az=spawn_z[slot];
        if(stage_spawn_cached_fighter(&fighters[slot],&stage_cache,roster,true,1,ax,az,0,slot,hp,mp)<0){
            lf2_logf("ERROR","Stage ally spawn failed slot=%d roster=%d",slot,roster);
            fighters[slot].active=false;fighters[slot].dead=true;
        }else{
            fighters[slot].ai_cooldown=5+i*4;
            lf2_log_stage("stage:ally_ready","slot=%d fighter=%s hp=%d mp=%d",slot,fighters[slot].def.name,fighters[slot].hp,fighters[slot].mp);
        }
    }
    /* Stage-authored milk/beer/weapons are cached before the simulation starts,
       so their first appearance cannot stall a wave transition either. */
    stage_preload_items(world,stage);

    int phase=0,queue_pos=0;
    char hud[96];snprintf(hud,sizeof(hud),"%s",stage->hud_prefix?stage->hud_prefix:"Stage");
    lf2_match_options_t opt={0};opt.allow_rematch=false;opt.allow_cancel=true;opt.hud_label=hud;opt.suppress_result_overlay=true;
    stage_fill_enemy_slots(fighters,&stage->phases[phase],&queue_pos,difficulty,&stage_cache,world,first_enemy_slot,last_enemy_slot);
    stage_spawn_phase_items(world,&stage->phases[phase]);
    lf2_log_stage("stage:continuous_start","label=%s phases=%d allies=%d enemy_slots=%d first_bound=%d final_bound=%d",
                  stage->hud_prefix?stage->hud_prefix:"Stage",stage->phase_count,ally_count,last_enemy_slot-first_enemy_slot+1,stage->phases[0].bound,final_bound);

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
            draw_match(font,stage_bg,fighters,world,total-1,difficulty,cheat_enabled,true,&opt);continue;
        }

        if(fighters[0].dead){
            if(!result_since)result_since=now;
            acc_us+=dt;input_t zero={0};int guard=0;
            while(acc_us>=sim_step_us&&guard++<4){
                for(int i=0;i<total;i++)if(fighters[i].active)update_control(&fighters[i],&zero);
                for(int i=0;i<LF2_MAX_ACTIVE_OBJECTS;i++)if(world->objects[i].active)update_object(world,&world->objects[i],fighters,total);
            object_apply_reflections(world);
                update_hit_fx(world);stage_camera_update(&fighters[0],stage->phases[phase].bound);acc_us-=sim_step_us;
            }
            draw_match(font,stage_bg,fighters,world,total-1,difficulty,cheat_enabled,false,&opt);
            if((dead_bodies_settled(fighters,total)&&now-result_since>650000ULL)||now-result_since>2200000ULL){result=LF2_MATCH_LOSS;break;}
            continue;
        }

        result_since=0;acc_us+=dt;int guard=0;
        while(acc_us>=sim_step_us&&guard++<4){
