    for(int i=0;i<cpu_count;i++)if(cpu_indices[i]<0||cpu_indices[i]>=lf2_roster_count||cpu_teams[i]<0||cpu_teams[i]>4)return -1;
    lf2_log_stage("match:alloc","fighter_size=%u p1=%s team=%d cpus=%d difficulty=%s",(unsigned)sizeof(fighter_t),lf2_roster[player_index].name,player_team,cpu_count,difficulty_name(difficulty));
    int total=LF2_MAX_CPUS+1;fighter_t *fighters=(fighter_t*)calloc((size_t)total,sizeof(*fighters));
    if(!fighters){lf2_logf("ERROR","fighter heap allocation failed count=%d",total);return -10;}
    object_world_t *world=(object_world_t*)calloc(1,sizeof(*world));
    if(!world){lf2_logf("ERROR","object world allocation failed bytes=%u",(unsigned)sizeof(*world));free(fighters);return -11;}
    lf2_log_memory("fighters + object world allocated");
    for(int i=cpu_count+1;i<total;i++)fighters[i].dead=true;
    int pmax=options->player_max_hp>0?options->player_max_hp:500;
    if(load_fighter(&fighters[0],player_index,options->player_ai,player_team,spawn_x[0],spawn_z[0],0,1,pmax)<0){free(world);free(fighters);return -2;}
    if(options->player_hp>0&&options->player_hp<pmax)fighters[0].hp=options->player_hp;
    fighters[0].mp=(options->player_mp>0&&options->player_mp<=500)?options->player_mp:500;
    int loaded=1;
    for(int i=0;i<cpu_count;i++){
        int chp=(options->cpu_hp&&options->cpu_hp[i]>0)?options->cpu_hp[i]:500;
        if(load_fighter(&fighters[i+1],cpu_indices[i],true,cpu_teams[i],spawn_x[i+1],spawn_z[i+1],1,i+1,chp)<0){
            vita2d_wait_rendering_done();for(int j=0;j<loaded;j++){if(fighters[j].hud_face)vita2d_free_texture(fighters[j].hud_face);lf2_free_character_textures(&fighters[j].def);}world_shutdown(world);free(world);free(fighters);return -3-i;
        }loaded++;
    }
    lf2_log_stage("match:gpu_ready","p1=%s cpus=%d diff=%s cheat=%d pool_free=%u",fighters[0].def.name,cpu_count,difficulty_name(difficulty),cheat_enabled?1:0,(unsigned)vita2d_pool_free_space());
    lf2_log_stage("match:first_frame_submit","pool_free=%u",(unsigned)vita2d_pool_free_space());
    draw_match(font,stage_bg,fighters,world,cpu_count,difficulty,cheat_enabled,false,options);vita2d_wait_rendering_done();
    lf2_log_stage("match:first_frame_complete","pool_free=%u",(unsigned)vita2d_pool_free_space());
    uint32_t prev=0;SceCtrlData first={0};sceCtrlPeekBufferPositive(0,&first,1);prev=first.buttons;
    uint32_t pending_pressed=0;
    bool paused=false;int result=0;
    srand(0x4c463256u+(unsigned)player_index*17u+(unsigned)cpu_count*47u+(unsigned)difficulty*101u);
    int item_drop_timer=240+(rand()%180);
    /* LF2 logic is based on 30 Hz time units. Rendering remains vblank-paced
       at 60 Hz, while gameplay advances at a fixed 30 Hz. This keeps wait,
       walking speeds, gravity, attack windows and animation timing in the
       same time domain as the original data files. */
    const uint64_t sim_step_us=33333;
    uint64_t last_us=sceKernelGetSystemTimeWide(),acc_us=0;
    lf2_logf("ANIM","fixed simulation rate=30Hz render_vblank=60Hz");
    uint64_t result_since=0;
    for(;;){
        SceCtrlData pad={0};sceCtrlPeekBufferPositive(0,&pad,1);
        uint32_t raw_pressed=pad.buttons&~prev;prev=pad.buttons;pending_pressed|=raw_pressed;
        uint64_t now_us=sceKernelGetSystemTimeWide();uint64_t dt_us=now_us-last_us;last_us=now_us;
        if(dt_us>100000)dt_us=100000; /* debugger / scheduling stall guard */

        if(options->player_ai && options->allow_cancel && (raw_pressed&SCE_CTRL_CIRCLE)){result=LF2_MATCH_CANCEL;break;}
        if(raw_pressed&SCE_CTRL_START){paused=!paused;acc_us=0;pending_pressed=0;lf2_logf("GAME","pause=%d",paused?1:0);}
        if(paused){
            if(raw_pressed&SCE_CTRL_CIRCLE){result=LF2_MATCH_CANCEL;break;}
            draw_match(font,stage_bg,fighters,world,cpu_count,difficulty,cheat_enabled,paused,options);continue;
        }

        int side_alive=player_side_alive(fighters,total);
        int enemies_alive=player_has_enemy(fighters,total);
        if(!side_alive||!enemies_alive){
            int outcome=(side_alive&&!enemies_alive)?LF2_MATCH_WIN:LF2_MATCH_LOSS;
            pending_pressed=0;
            if(!result_since){result_since=now_us;lf2_logf("GAME","round over outcome=%d; settling KO physics",outcome);}
            /* Keep the 30 Hz simulation alive after KO. No new attacks are
               allowed, but bodies, projectiles and hit particles finish their
               authored motion so a fighter never freezes in mid-air. */
            acc_us+=dt_us;int settle_guard=0;input_t zero={0};
            while(acc_us>=sim_step_us&&settle_guard++<4){
                for(int i=0;i<total;i++)update_control(&fighters[i],&zero);
                for(int i=0;i<LF2_MAX_ACTIVE_OBJECTS;i++)if(world->objects[i].active)update_object(world,&world->objects[i],fighters,total);
            object_apply_reflections(world);
                update_hit_fx(world);acc_us-=sim_step_us;
            }
            if(options->auto_continue){
                if(dead_bodies_settled(fighters,total)&&now_us-result_since>=650000ULL){result=outcome;break;}
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
