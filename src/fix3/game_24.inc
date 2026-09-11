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
            object_apply_reflections(world);
            int oldhp[LF2_MAX_ACTORS];for(int i=0;i<total;i++)oldhp[i]=fighters[i].hp;
            for(int a=0;a<total;a++)if(fighters[a].active)for(int b=0;b<total;b++)if(a!=b&&fighters[b].active)apply_hits(world,fighters,total,a,b,difficulty);
            for(int i=0;i<LF2_MAX_ACTIVE_OBJECTS;i++)if(world->objects[i].active)object_apply_hits(world,&world->objects[i],fighters,total,difficulty);
            update_hit_fx(world);
            for(int i=0;i<total;i++)if(fighters[i].active&&fighters[i].hp!=oldhp[i])
                lf2_logf("HIT","Stage slot=%d %s hp %d -> %d",i,fighters[i].def.name,oldhp[i],fighters[i].hp);

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
                                  phase+1,stage->phase_count,old_bound,stage->phases[phase].bound,n,items,fighters[0].x,g_camera_x);
                }
            }
            stage_camera_update(&fighters[0],stage->phases[phase].bound);
            acc_us-=sim_step_us;
            if(result==LF2_MATCH_WIN)break;
        }
        draw_match(font,stage_bg,fighters,world,total-1,difficulty,cheat_enabled,false,&opt);
        if(result==LF2_MATCH_WIN){
            /* A short final render keeps the last KO/lying frame visible. */
            sceKernelDelayThread(350000);
            break;
        }
        if(raw_pressed&SCE_CTRL_CIRCLE && (pad.buttons&SCE_CTRL_START)){result=LF2_MATCH_CANCEL;break;}
    }

    if(stage->player_hp_out)*stage->player_hp_out=fighters[0].hp;
    if(stage->player_mp_out)*stage->player_mp_out=fighters[0].mp;
    for(int i=0;i<ally_count;i++){
        if(stage->ally_hp_out)stage->ally_hp_out[i]=fighters[i+1].active?fighters[i+1].hp:0;
        if(stage->ally_mp_out)stage->ally_mp_out[i]=fighters[i+1].active?fighters[i+1].mp:0;
    }

    memset(&g_last_stage_summary,0,sizeof(g_last_stage_summary));
    int summary_count=1+ally_count;if(summary_count>LF2_STAGE_SUMMARY_MAX_PLAYERS)summary_count=LF2_STAGE_SUMMARY_MAX_PLAYERS;
    g_last_stage_summary.participant_count=summary_count;
    for(int i=0;i<summary_count;i++){
        fighter_t *f=&fighters[i];
        g_last_stage_summary.roster[i]=f->roster_idx;
        g_last_stage_summary.kills[i]=f->stat_kills;
        g_last_stage_summary.attack[i]=f->stat_attack;
        g_last_stage_summary.hp_lost[i]=f->stat_hp_lost;
        g_last_stage_summary.mp_usage[i]=f->stat_mp_usage;
        g_last_stage_summary.picking[i]=f->stat_picking;
        g_last_stage_summary.hp[i]=f->hp;
        g_last_stage_summary.mp[i]=f->mp;
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
