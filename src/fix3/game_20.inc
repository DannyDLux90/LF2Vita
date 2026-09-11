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
    if(options->lockstep_actor_map_enabled){
        for(int ai=0;ai<total;ai++){
            uint8_t ctl=options->lockstep_actor_control[ai];
            if(ctl>=1&&ctl<=8){
                fighters[ai].ai=false;fighters[ai].human_no=(int)ctl;
                lf2_log_stage("match:stock_control","actor=%d control=%u fighter=%s team=%d",ai,(unsigned)ctl,fighters[ai].def.name,fighters[ai].team);
            }
        }
    }
    if(options->remote_player_count>0){
        for(int rn=0;rn<options->remote_player_count&&rn<4;rn++){
            int slot=options->remote_player_slots[rn];
            if(slot>0&&slot<total){
                fighter_t *rf=&fighters[slot];rf->ai=false;rf->human_no=2+rn;
                lf2_log_stage("match:remote_player","net_slot=%d actor=%d fighter=%s team=%d",rn,slot,rf->def.name,rf->team);
            }
        }
    }else if(options->remote_player_slot>0&&options->remote_player_slot<total){
        fighter_t *rf=&fighters[options->remote_player_slot];rf->ai=false;rf->human_no=2;
        lf2_log_stage("match:remote_player","slot=%d fighter=%s team=%d",options->remote_player_slot,rf->def.name,rf->team);
    }
    lf2_log_stage("match:gpu_ready","p1=%s cpus=%d diff=%s cheat=%d pool_free=%u",fighters[0].def.name,cpu_count,difficulty_name(difficulty),cheat_enabled?1:0,(unsigned)vita2d_pool_free_space());
    lf2_log_stage("match:first_frame_submit","pool_free=%u",(unsigned)vita2d_pool_free_space());
    draw_match(font,stage_bg,fighters,world,cpu_count,difficulty,cheat_enabled,false,options);vita2d_wait_rendering_done();
    lf2_log_stage("match:first_frame_complete","pool_free=%u",(unsigned)vita2d_pool_free_space());
    uint32_t prev=0;SceCtrlData first={0};sceCtrlPeekBufferPositive(0,&first,1);prev=first.buttons;
    uint32_t pending_pressed=0;
    uint32_t remote_prev[4]={0,0,0,0};
    uint32_t lock_local_prev[4]={0,0,0,0};
    uint32_t lock_actor_prev[8]={0,0,0,0,0,0,0,0};
    bool paused=false;bool network_abort=false;bool reference_done=false;int result=0;
    const bool reference_mode=options->reference_tu_limit>0&&options->lockstep_actor_map_enabled&&options->lockstep_frame;
    srand(0x4c463256u+(unsigned)player_index*17u+(unsigned)cpu_count*47u+(unsigned)difficulty*101u);
    int item_drop_timer=240+(lf2_rng_mod(180));
    uint32_t stock_tu=0;
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
        if(options->remote_player_slot>0&&options->remote_alive&&!options->remote_alive(options->remote_userdata)){
            lf2_log_stage("match:remote_disconnect","slot=%d",options->remote_player_slot);result=LF2_MATCH_CANCEL;break;
        }

        if(!options->lockstep_actor_map_enabled&&options->player_ai && options->allow_cancel && (raw_pressed&SCE_CTRL_CIRCLE)){result=LF2_MATCH_CANCEL;break;}
        if(!options->lockstep_actor_map_enabled&&(raw_pressed&SCE_CTRL_START)){paused=!paused;acc_us=0;pending_pressed=0;lf2_logf("GAME","pause=%d",paused?1:0);}
        if(paused){
            if(raw_pressed&SCE_CTRL_CIRCLE){result=LF2_MATCH_CANCEL;break;}
            draw_match(font,stage_bg,fighters,world,cpu_count,difficulty,cheat_enabled,paused,options);continue;
        }

        int side_alive=player_side_alive(fighters,total);
        int enemies_alive=player_has_enemy(fighters,total);
        if((!side_alive||!enemies_alive) && !(reference_mode && stock_tu<options->reference_tu_limit)){
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
                    reset_match(fighters,cpu_count);world_clear_active(world);item_drop_timer=240+(lf2_rng_mod(180));last_us=sceKernelGetSystemTimeWide();acc_us=0;result_since=0;
                }
            } else {
                if(options->lockstep_actor_map_enabled){
                    /* Stock peers must leave the result state on the same
                       deterministic clock; never consume a Vita-only button. */
                    if(now_us-result_since>=2200000ULL){result=outcome;break;}
                }else{
                    if(raw_pressed&SCE_CTRL_CIRCLE){result=LF2_MATCH_CANCEL;break;}
                    if(raw_pressed&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE)){result=outcome;break;}
                }
            }
            draw_match(font,stage_bg,fighters,world,cpu_count,difficulty,cheat_enabled,false,options);continue;
        }
        result_since=0;

        if(reference_mode&&options->reference_fast_forward)acc_us+=sim_step_us*32u;
        else acc_us+=dt_us;
        int sim_guard=0;
        const int sim_guard_limit=(reference_mode&&options->reference_fast_forward)?32:4;
        while(acc_us>=sim_step_us && sim_guard++<sim_guard_limit){
            fighter_t *p=&fighters[0];
            input_t in;
            if(options->player_ai){
                fighter_t *target=nearest_living_enemy(p,fighters,total);
                in=make_ai_input(p,target,difficulty);
                if(target&&!in_action(p)&&p->frame!=110)p->facing=(target->x<p->x);
            } else {
                in=make_player_input(&pad,pending_pressed);
            }
            uint32_t lock_local_held[4]={0,0,0,0};
            uint32_t lock_remote_held[4]={0,0,0,0};
            bool use_lockstep=options->lockstep_frame&&(options->lockstep_actor_map_enabled||options->remote_player_slot>0||options->remote_player_count>0);
            if(use_lockstep){
                strip_vita_only_shortcuts(&in);
                /* A Vita exposes one physical controller. It always feeds the
                   first stock control slot owned by this machine; the actor map
                   decides whether that is global player 1 or player 5. */
                lock_local_held[0]=basic_remote_mask_from_input(&in);
                if(!options->lockstep_frame(options->remote_userdata,lock_local_held,lock_remote_held)){
                    lf2_log_stage("match:lockstep_disconnect","slot=%d remotes=%d map=%d",options->remote_player_slot,options->remote_player_count,options->lockstep_actor_map_enabled?1:0);
                    network_abort=true;result=LF2_MATCH_CANCEL;break;
                }
            }

            if(options->lockstep_actor_map_enabled&&use_lockstep){
                /* Stock mode: process fighters strictly in actor order, but get
                   each human's controls from its original 1..8 network slot. */
                for(int i=0;i<total;i++){
                    fighter_t *c=&fighters[i];uint8_t ctl=options->lockstep_actor_control[i];
                    if(ctl>=1&&ctl<=8){
                        uint32_t held=(ctl<=4)?lock_local_held[ctl-1]:lock_remote_held[ctl-5];
                        int pi=(int)ctl-1;uint32_t pressed=held&~lock_actor_prev[pi];lock_actor_prev[pi]=held;
                        input_t ri=make_remote_input(held,pressed);
                        if(!handle_form_input(fighters,total,i,&ri)){fighter_handle_item_input(world,c,&ri);handle_catch_input(fighters,total,i,&ri);update_control(c,&ri);}process_frame_state(fighters,total,i,world);
                    }else{
                        fighter_t *target=nearest_living_enemy(c,fighters,total);
                        input_t ai=target?make_ai_input(c,target,difficulty):make_ai_retreat_input(c,nearest_lying_enemy(c,fighters,total));
                        if(!handle_form_input(fighters,total,i,&ai)){fighter_handle_item_input(world,c,&ai);handle_catch_input(fighters,total,i,&ai);update_control(c,&ai);}process_frame_state(fighters,total,i,world);
                        if(target&&!in_action(c)&&c->frame!=110)c->facing=(target->x<c->x);
                    }
                }
            }else{
                if(use_lockstep){
                    uint32_t lp=lock_local_held[0]&~lock_local_prev[0];lock_local_prev[0]=lock_local_held[0];
                    in=make_remote_input(lock_local_held[0],lp);
                }
                pending_pressed=0;if(!handle_form_input(fighters,total,0,&in)){fighter_handle_item_input(world,p,&in);handle_catch_input(fighters,total,0,&in);update_control(p,&in);}process_frame_state(fighters,total,0,world);
                for(int i=1;i<total;i++){
                    fighter_t *c=&fighters[i];int remote_net_slot=-1;
                    if(options->remote_player_count>0){for(int rn=0;rn<options->remote_player_count&&rn<4;rn++)if(options->remote_player_slots[rn]==i){remote_net_slot=rn;break;}}
                    else if(i==options->remote_player_slot)remote_net_slot=0;
                    if(remote_net_slot>=0&&(options->lockstep_frame||options->remote_input)){
                        uint32_t held=options->lockstep_frame?lock_remote_held[remote_net_slot]:options->remote_input(options->remote_userdata);
                        uint32_t pressed=held&~remote_prev[remote_net_slot];remote_prev[remote_net_slot]=held;
                        input_t ri=make_remote_input(held,pressed);
                        if(!handle_form_input(fighters,total,i,&ri)){fighter_handle_item_input(world,c,&ri);handle_catch_input(fighters,total,i,&ri);update_control(c,&ri);}process_frame_state(fighters,total,i,world);
                    }else{
                        fighter_t *target=nearest_living_enemy(c,fighters,total);
                        input_t ai=target?make_ai_input(c,target,difficulty):make_ai_retreat_input(c,nearest_lying_enemy(c,fighters,total));
                        if(!handle_form_input(fighters,total,i,&ai)){fighter_handle_item_input(world,c,&ai);handle_catch_input(fighters,total,i,&ai);update_control(c,&ai);}process_frame_state(fighters,total,i,world);
                        if(target&&!in_action(c)&&c->frame!=110)c->facing=(target->x<c->x);
                    }
                }
            }
            pending_pressed=0;
