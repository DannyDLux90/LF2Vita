            if(raw_pressed&SCE_CTRL_CIRCLE){result=0;break;}
            if(raw_pressed&SCE_CTRL_CROSS){lf2_logf("GAME","rematch cpus=%d",cpu_count);reset_match(fighters,cpu_count);last_us=sceKernelGetSystemTimeWide();}
            draw_match(font,stage_bg,fighters,cpu_count,difficulty,cheat_enabled,false);continue;
        }

        acc_us+=dt_us;
        int sim_guard=0;
        while(acc_us>=sim_step_us && sim_guard++<4){
            input_t in=make_player_input(&pad,pending_pressed);pending_pressed=0;
            fighter_t *p=&fighters[0];update_control(p,&in);
            fighter_t *pt=nearest_living_enemy(p,fighters,total);if(pt&&!in_action(p)&&p->frame!=110)p->facing=(pt->x<p->x);
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
        draw_match(font,stage_bg,fighters,cpu_count,difficulty,cheat_enabled,false);
    }
    lf2_log_stage("match:cleanup","result=%d cpus=%d",result,cpu_count);vita2d_wait_rendering_done();
    for(int i=0;i<total;i++)lf2_free_character_textures(&fighters[i].def);free(fighters);lf2_log_memory("match cleanup");return result;
}
