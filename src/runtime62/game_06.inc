        if(raw_pressed&SCE_CTRL_START){paused=!paused;acc_us=0;pending_pressed=0;lf2_logf("GAME","pause=%d",paused?1:0);}
        if(paused){
            if(raw_pressed&SCE_CTRL_CIRCLE){result=LF2_MATCH_CANCEL;break;}
            draw_match(font,stage_bg,fighters,cpu_count,difficulty,cheat_enabled,paused,options);continue;
        }

        int side_alive=player_side_alive(fighters,total);
        int enemies_alive=player_has_enemy(fighters,total);
        if(!side_alive||!enemies_alive){
            int outcome=(side_alive&&!enemies_alive)?LF2_MATCH_WIN:LF2_MATCH_LOSS;
            acc_us=0;pending_pressed=0;
            if(!result_since)result_since=now_us;
            if(options->auto_continue){
                if(now_us-result_since>=800000ULL){result=outcome;break;}
            } else if(options->allow_rematch){
                if(raw_pressed&SCE_CTRL_CIRCLE){result=LF2_MATCH_CANCEL;break;}
                if(raw_pressed&SCE_CTRL_CROSS){
                    lf2_logf("GAME","rematch cpus=%d",cpu_count);
                    reset_match(fighters,cpu_count);last_us=sceKernelGetSystemTimeWide();result_since=0;
                }
            } else {
                if(raw_pressed&SCE_CTRL_CIRCLE){result=LF2_MATCH_CANCEL;break;}
                if(raw_pressed&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE)){result=outcome;break;}
            }
            draw_match(font,stage_bg,fighters,cpu_count,difficulty,cheat_enabled,false,options);continue;
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
            pending_pressed=0;update_control(p,&in);
            /* Human P1 keeps the direction chosen by movement. AI-controlled P1
               (Demo) faces its target just like the other computer slots. */
            for(int i=1;i<total;i++){
                fighter_t *c=&fighters[i];if(c->dead)continue;
                fighter_t *target=nearest_living_enemy(c,fighters,total);
                input_t ai=make_ai_input(c,target,difficulty);update_control(c,&ai);
                if(target&&!in_action(c)&&c->frame!=110)c->facing=(target->x<c->x);
            }

            int oldhp[LF2_MAX_CPUS+1];for(int i=0;i<total;i++)oldhp[i]=fighters[i].hp;
            for(int a=0;a<total;a++)for(int b=0;b<total;b++)if(a!=b)apply_hits(&fighters[a],&fighters[b],difficulty);
            for(int i=0;i<total;i++)if(fighters[i].hp!=oldhp[i]){
                if(i==0)lf2_logf("HIT","P1 %s hp %d -> %d",fighters[i].def.name,oldhp[i],fighters[i].hp);
                else lf2_logf("HIT","CPU%d %s hp %d -> %d",i,fighters[i].def.name,oldhp[i],fighters[i].hp);
            }
            acc_us-=sim_step_us;
        }
        draw_match(font,stage_bg,fighters,cpu_count,difficulty,cheat_enabled,false,options);
    }
    if(options->player_hp_out)*options->player_hp_out=fighters[0].hp;
    if(options->player_mp_out)*options->player_mp_out=fighters[0].mp;
    lf2_log_stage("match:cleanup","result=%d cpus=%d p1_hp=%d p1_mp=%d",result,cpu_count,fighters[0].hp,fighters[0].mp);vita2d_wait_rendering_done();
    for(int i=0;i<total;i++)lf2_free_character_textures(&fighters[i].def);
    free(fighters);lf2_log_memory("match cleanup");return result;
}

int lf2_run_match(vita2d_pgf *font, vita2d_texture *stage_bg,
                  int player_index, int player_team,
                  const int *cpu_indices, const int *cpu_teams, int cpu_count,
                  int difficulty, bool cheat_enabled){
    lf2_match_options_t options={0};options.allow_rematch=true;
    return lf2_run_match_ex(font,stage_bg,player_index,player_team,cpu_indices,cpu_teams,cpu_count,difficulty,cheat_enabled,&options);
}
