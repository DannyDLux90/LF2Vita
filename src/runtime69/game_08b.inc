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
    stage_fill_enemy_slots(fighters,&stage->phases[phase],&queue_pos,difficulty,&stage_cache,world);
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
            check_firzen_fusion(fighters,total,world);tick_fusion(fighters,total);
            for(int i=0;i<total;i++)if(fighters[i].active)spawn_fighter_opoints(world,fighters,total,i);
            for(int i=0;i<LF2_MAX_ACTIVE_OBJECTS;i++)if(world->objects[i].active)update_object(world,&world->objects[i],fighters,total);
            int oldhp[LF2_MAX_CPUS+1];for(int i=0;i<=LF2_MAX_CPUS;i++)oldhp[i]=fighters[i].hp;
            for(int a=0;a<=LF2_MAX_CPUS;a++)if(fighters[a].active)for(int b=0;b<=LF2_MAX_CPUS;b++)if(a!=b&&fighters[b].active)apply_hits(world,fighters,total,a,b,difficulty);
            for(int i=0;i<LF2_MAX_ACTIVE_OBJECTS;i++)if(world->objects[i].active)object_apply_hits(world,&world->objects[i],fighters,total,difficulty);
            update_hit_fx(world);
            for(int i=0;i<=LF2_MAX_CPUS;i++)if(fighters[i].active&&fighters[i].hp!=oldhp[i])
                lf2_logf("HIT","Stage slot=%d %s hp %d -> %d",i,fighters[i].def.name,oldhp[i],fighters[i].hp);

            /* Stage phases are one continuous world. As soon as a wave has
               settled, open the next authored bound and populate its off-screen
               enemy slots in the same simulation tick. There is deliberately no
               fixed inter-wave delay and no result/GO splash. */
            stage_fill_enemy_slots(fighters,&stage->phases[phase],&queue_pos,difficulty,&stage_cache,world);
            if(queue_pos>=stage->phases[phase].enemy_count&&stage_phase_settled(fighters)){
                if(phase+1>=stage->phase_count){
                    result=LF2_MATCH_WIN;result_since=now;
                }else{
                    int old_bound=stage->phases[phase].bound;
                    phase++;queue_pos=0;
                    g_world_max_x=(float)stage->phases[phase].bound-40.0f;
                    if(g_world_max_x<920.0f)g_world_max_x=920.0f;
                    int n=stage_fill_enemy_slots(fighters,&stage->phases[phase],&queue_pos,difficulty,&stage_cache,world);
                    int items=stage_spawn_phase_items(world,&stage->phases[phase]);
                    lf2_log_stage("stage:wave_advance","phase=%d/%d bound=%d->%d spawned=%d items=%d player_x=%.1f camera=%.1f",
                                  phase+1,stage->phase_count,old_bound,stage->phases[phase].bound,n,items,fighters[0].x,g_camera_x);
                }
            }
            stage_camera_update(&fighters[0],stage->phases[phase].bound);
            acc_us-=sim_step_us;
            if(result==LF2_MATCH_WIN)break;
        }
        draw_match(font,stage_bg,fighters,world,LF2_MAX_CPUS,difficulty,cheat_enabled,false,&opt);
        if(result==LF2_MATCH_WIN){
            /* A short final render keeps the last KO/lying frame visible. */
            sceKernelDelayThread(350000);
            break;
        }
        if(raw_pressed&SCE_CTRL_CIRCLE && (pad.buttons&SCE_CTRL_START)){result=LF2_MATCH_CANCEL;break;}
    }

    if(stage->player_hp_out)*stage->player_hp_out=fighters[0].hp;
    if(stage->player_mp_out)*stage->player_mp_out=fighters[0].mp;
    lf2_log_stage("stage:continuous_end","result=%d phase=%d/%d hp=%d mp=%d camera=%.1f",
                  result,phase+1,stage->phase_count,fighters[0].hp,fighters[0].mp,g_camera_x);
    vita2d_wait_rendering_done();
    for(int i=0;i<=LF2_MAX_CPUS;i++)if(fighters[i].active)unload_fighter_slot(&fighters[i]);
    stage_asset_shutdown(&stage_cache);
    world_shutdown(world);free(world);free(fighters);
    g_world_min_x=old_min;g_world_max_x=old_max;g_camera_x=old_cam;
    return result;
}

int lf2_run_match(vita2d_pgf *font, vita2d_texture *stage_bg,
                  int player_index, int player_team,
                  const int *cpu_indices, const int *cpu_teams, int cpu_count,
                  int difficulty, bool cheat_enabled){
    lf2_match_options_t options={0};options.allow_rematch=true;
    return lf2_run_match_ex(font,stage_bg,player_index,player_team,cpu_indices,cpu_teams,cpu_count,difficulty,cheat_enabled,&options);
}
