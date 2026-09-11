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
            stage_fill_enemy_slots(fighters,&stage->phases[phase],&queue_pos,difficulty,&stage_cache);
            if(queue_pos>=stage->phases[phase].enemy_count&&stage_phase_settled(fighters)){
                if(phase+1>=stage->phase_count){
                    result=LF2_MATCH_WIN;result_since=now;
                }else{
                    int old_bound=stage->phases[phase].bound;
                    phase++;queue_pos=0;
                    g_world_max_x=(float)stage->phases[phase].bound-40.0f;
                    if(g_world_max_x<920.0f)g_world_max_x=920.0f;
                    int n=stage_fill_enemy_slots(fighters,&stage->phases[phase],&queue_pos,difficulty,&stage_cache);
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
