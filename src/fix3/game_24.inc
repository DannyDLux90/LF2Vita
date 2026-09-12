            input_t local_in=make_player_input(&pad,pending_pressed);pending_pressed=0;
            uint32_t lock_local[4]={0},lock_remote[4]={0};
            bool use_lockstep=stage->lockstep_actor_map_enabled&&stage->lockstep_native_vita_input&&stage->lockstep_frame;
            if(use_lockstep){
                lock_local[0]=native_remote_mask_from_input(&local_in);
                if(!stage->lockstep_frame(stage->remote_userdata,lock_local,lock_remote)){network_abort=true;result=LF2_MATCH_CANCEL;break;}
            }
            for(int i=0;i<total;i++)if(fighters[i].active){
                fighter_t *c=&fighters[i];
                if(c->dead){input_t zero={0};update_control(c,&zero);continue;}
                uint8_t ctl=(i<first_enemy_slot&&i<(LF2_STAGE_MAX_ALLIES+1))?stage->lockstep_actor_control[i]:0;
                if(use_lockstep&&(ctl==1||ctl==5)){
                    uint32_t held=(ctl==1)?lock_local[0]:lock_remote[0];
                    int pi=(ctl==1)?0:4;uint32_t pressed=held&~lock_actor_prev[pi];lock_actor_prev[pi]=held;
                    input_t ni=make_native_remote_input(held,pressed);
                    if(!handle_form_input(fighters,total,i,&ni)){fighter_handle_item_input(world,c,&ni);handle_catch_input(fighters,total,i,&ni);update_control(c,&ni);}process_frame_state(fighters,total,i,world);
                    continue;
                }
                if(!use_lockstep&&i==0){
                    if(!handle_form_input(fighters,total,0,&local_in)){fighter_handle_item_input(world,c,&local_in);handle_catch_input(fighters,total,0,&local_in);update_control(c,&local_in);}process_frame_state(fighters,total,0,world);continue;
                }
                /* Enemies authored for the next screen wait just beyond the
                   viewport instead of rushing backwards through the camera. */
                if(i>=first_enemy_slot&&i<=last_enemy_slot&&screen_x(c->x)>930.0f)continue;
                fighter_t *target=nearest_living_enemy(c,fighters,total);
                input_t ai=target?make_ai_input(c,target,difficulty):make_ai_retreat_input(c,nearest_lying_enemy(c,fighters,total));
                if(!handle_form_input(fighters,total,i,&ai)){fighter_handle_item_input(world,c,&ai);handle_catch_input(fighters,total,i,&ai);update_control(c,&ai);}process_frame_state(fighters,total,i,world);
                if(target&&!in_action(c)&&c->frame!=110)c->facing=(target->x<c->x);
            }

            sync_all_catches(fighters,total);
            check_firzen_fusion(fighters,total,world);tick_fusion(fighters,total);
            for(int i=0;i<total;i++)if(fighters[i].active)spawn_fighter_opoints(world,fighters,total,i);
            for(int i=0;i<LF2_MAX_ACTIVE_OBJECTS;i++)if(world->objects[i].active)update_object(world,&world->objects[i],fighters,total);
            object_apply_reflections(world);
            int oldhp[LF2_MAX_ACTORS];for(int i=0;i<total;i++)oldhp[i]=fighters[i].hp;
            for(int a=0;a<total;a++)if(fighters[a].active)for(int b=0;b<total;b++)if(a!=b&&fighters[b].active)apply_hits(world,fighters,total,a,b,difficulty);
            for(int i=0;i<LF2_MAX_ACTIVE_OBJECTS;i++)if(world->objects[i].active)object_apply_hits(world,&world->objects[i],fighters,total,difficulty);
            update_hit_fx(world);
            for(int i=0;i<total;i++)if(fighters[i].active&&fighters[i].hp!=oldhp[i])
                lf2_logf("HIT","Stage slot=%d %s hp %d -> %d",i,fighters[i].def.name,oldhp[i],fighters[i].hp);
            if(stage->lockstep_actor_map_enabled&&stage->lockstep_native_vita_input){network_tu++;if(network_tu==1||(network_tu%30u)==0u){uint32_t dg=stock_state_digest(fighters,total,world,0);lf2_logf("NETSTATE","stage_tu=%u digest=%08X",(unsigned)network_tu,dg);if(stage->lockstep_state)stage->lockstep_state(stage->remote_userdata,network_tu,dg);}}

            /* Stage phases are one continuous world. As soon as a wave has
               settled, open the next authored bound and populate its off-screen
               enemy slots in the same simulation tick. There is deliberately no
               fixed inter-wave delay and no result/GO splash. */
            stage_fill_enemy_slots(fighters,&stage->phases[phase],&queue_pos,difficulty,&stage_cache,world,first_enemy_slot,last_enemy_slot);
            if(queue_pos>=stage->phases[phase].enemy_count&&stage_phase_settled(fighters,first_enemy_slot,last_enemy_slot)){
                if(phase+1>=stage->phase_count){
                    result=LF2_MATCH_WIN;result_since=now;
                }else{
                    int old_bound=stage->phases[phase].bound;
                    phase++;queue_pos=0;
                    g_world_max_x=(float)stage->phases[phase].bound-40.0f;
                    if(g_world_max_x<920.0f)g_world_max_x=920.0f;
                    int n=stage_fill_enemy_slots(fighters,&stage->phases[phase],&queue_pos,difficulty,&stage_cache,world,first_enemy_slot,last_enemy_slot);
                    int items=stage_spawn_phase_items(world,&stage->phases[phase]);
                    lf2_log_stage("stage:wave_advance","phase=%d/%d bound=%d->%d spawned=%d items=%d player_x=%.1f camera=%.1f",
                                  phase+1,stage->phase_count,old_bound,stage->phases[phase].bound,n,items,stage_party_focus(fighters,first_enemy_slot)->x,g_camera_x);
                }
            }
            stage_camera_update(stage_party_focus(fighters,first_enemy_slot),stage->phases[phase].bound);
            acc_us-=sim_step_us;
            if(result==LF2_MATCH_WIN)break;
        }
        if(network_abort)break;
        draw_match(font,stage_bg,fighters,world,total-1,difficulty,cheat_enabled,false,&opt);
        if(result==LF2_MATCH_WIN){
            /* A short final render keeps the last KO/lying frame visible. */
            sceKernelDelayThread(350000);
            break;
        }
        if(!stage->lockstep_actor_map_enabled&&raw_pressed&SCE_CTRL_CIRCLE && (pad.buttons&SCE_CTRL_START)){result=LF2_MATCH_CANCEL;break;}
    }

    if(stage->player_hp_out)*stage->player_hp_out=fighters[0].hp;
    if(stage->player_mp_out)*stage->player_mp_out=fighters[0].mp;
    for(int i=0;i<ally_count;i++){
        if(stage->ally_hp_out)stage->ally_hp_out[i]=fighters[i+1].active?fighters[i+1].hp:0;
        if(stage->ally_mp_out)stage->ally_mp_out[i]=fighters[i+1].active?fighters[i+1].mp:0;
    }
    lf2_log_stage("stage:continuous_end","result=%d phase=%d/%d hp=%d mp=%d camera=%.1f",
                  result,phase+1,stage->phase_count,fighters[0].hp,fighters[0].mp,g_camera_x);
    vita2d_wait_rendering_done();
    for(int i=0;i<total;i++)if(fighters[i].active)unload_fighter_slot(&fighters[i]);
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
