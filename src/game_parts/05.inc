    vita2d_draw_rectangle(0,468,960,76,RGBA8(3,8,20,185));
    int order[LF2_MAX_CPUS+1];for(int i=0;i<total;i++)order[i]=i;
    for(int i=0;i<total;i++)for(int j=i+1;j<total;j++)if(fighters[order[i]].z>fighters[order[j]].z){int t=order[i];order[i]=order[j];order[j]=t;}
    for(int i=0;i<total;i++)draw_fighter(&fighters[order[i]]);
    draw_bar(28,25,370,19,(float)p->hp,(float)p->max_hp,RGBA8(55,220,90,255));
    draw_bar(28,48,300,11,(float)p->mp,500,RGBA8(70,145,255,255));
    for(int i=0;i<cpu_count;i++){
        fighter_t *c=&fighters[i+1];float y=18.0f+i*26.0f;
        draw_bar(705,y,225,9,(float)c->hp,(float)c->max_hp,RGBA8(230,65,75,255));
        if(font)vita2d_pgf_draw_textf(font,535,(int)y+9,c->dead?RGBA8(150,150,160,255):RGBA8(255,255,255,255),0.62f,"COM%d %s  %s",i+1,c->def.name,team_name(c->team));
    }
    if(font){
        vita2d_pgf_draw_textf(font,28,83,RGBA8(255,255,255,255),0.92f,"P1  %s  •  %s",p->def.name,team_name(p->team));
        vita2d_pgf_draw_text(font,18,523,RGBA8(220,230,245,255),0.68f,"D-Pad/Stick bewegen   □ Angriff   × Sprung   ○/L Abwehr   △/R Spezial   START Pause");
        vita2d_pgf_draw_textf(font,400,84,RGBA8(195,215,245,255),0.68f,"Difficulty: %s",difficulty_name(difficulty));
        if(cheat) vita2d_pgf_draw_text(font,405,56,RGBA8(255,225,80,255),0.76f,"LF2.NET  •  CRAZY freigeschaltet");
        if(p->dead||!player_has_enemy(fighters,total)){
            vita2d_draw_rectangle(235,185,490,135,RGBA8(0,0,0,205));
            const char *msg=p->dead?"K.O. - Spieler 1 ist besiegt":"K.O. - Spieler 1 gewinnt";
            int tw=vita2d_pgf_text_width(font,1.15f,msg);
            vita2d_pgf_draw_text(font,480-tw/2,235,RGBA8(255,255,255,255),1.15f,msg);
            vita2d_pgf_draw_text(font,325,280,RGBA8(190,210,240,255),0.82f,"×: Revanche     ○: zurück zum Menü");
        } else if(paused){
            vita2d_draw_rectangle(285,190,390,120,RGBA8(0,0,0,215));
            vita2d_pgf_draw_text(font,420,235,RGBA8(255,255,255,255),1.1f,"PAUSE");
            vita2d_pgf_draw_text(font,340,275,RGBA8(200,215,240,255),0.75f,"START: weiter   ○: zurück zum Menü");
        }
    }
    vita2d_end_drawing(); vita2d_swap_buffers();
}

static int load_fighter(fighter_t *f,int roster_idx,bool ai,int team,float x,float z,int facing,int slot) {
    memset(f,0,sizeof(*f)); f->ai=ai;f->team=team; f->x=x; f->z=z; f->facing=facing; f->hp=f->max_hp=500; f->mp=500;
    lf2_log_stage("match:character_parse","slot=%s%d roster=%d name=%s team=%d dat=%s",ai?"CPU":"P",ai?slot:1,roster_idx,lf2_roster[roster_idx].name,team,lf2_roster[roster_idx].dat);
    int rc=lf2_load_character(GAME_ROOT,lf2_roster[roster_idx].dat,&f->def);if(rc<0){lf2_logf("ERROR","character parse failed rc=%d",rc);return -1;}
    rc=lf2_load_character_textures(GAME_ROOT,&f->def);if(rc<0){lf2_logf("ERROR","character textures failed rc=%d",rc);lf2_free_character_textures(&f->def);return -2;}
    set_frame(f,0); lf2_logf("INFO","fighter ready slot=%s%d name=%s team=%d sheets=%d",ai?"CPU":"P",ai?slot:1,f->def.name,team,f->def.sheet_count); return 0;
}

static const float spawn_x[LF2_MAX_CPUS+1]={165.0f,795.0f,705.0f,615.0f,525.0f,435.0f,345.0f,255.0f};
static const float spawn_z[LF2_MAX_CPUS+1]={0.0f,-52.0f,-18.0f,18.0f,52.0f,-35.0f,35.0f,0.0f};

static void reset_fighter_state(fighter_t *f,float x,float z,int facing){
    f->x=x;f->z=z;f->y=0;f->vx=f->vy=f->vz=0;f->facing=facing;f->hp=f->max_hp;f->mp=500;f->dead=false;f->invuln=0;f->ai_cooldown=0;f->hit_latched=false;set_frame(f,0);
}

static void reset_match(fighter_t *fighters,int cpu_count){
    reset_fighter_state(&fighters[0],spawn_x[0],spawn_z[0],0);
    for(int i=0;i<cpu_count;i++)reset_fighter_state(&fighters[i+1],spawn_x[i+1],spawn_z[i+1],1);
}

int lf2_run_match(vita2d_pgf *font, vita2d_texture *stage_bg,
                  int player_index, int player_team,
                  const int *cpu_indices, const int *cpu_teams, int cpu_count,
                  int difficulty, bool cheat_enabled) {
    if(player_index<0||player_index>=lf2_roster_count||!cpu_indices||!cpu_teams||cpu_count<1||cpu_count>LF2_MAX_CPUS)return -1;
    if(player_team<0||player_team>4)return -1;
    if(difficulty<LF2_DIFF_EASY||difficulty>LF2_DIFF_CRAZY)return -1;
    for(int i=0;i<cpu_count;i++)if(cpu_indices[i]<0||cpu_indices[i]>=lf2_roster_count||cpu_teams[i]<0||cpu_teams[i]>4)return -1;
    lf2_log_stage("match:alloc","fighter_size=%u p1=%s team=%d cpus=%d difficulty=%s",(unsigned)sizeof(fighter_t),lf2_roster[player_index].name,player_team,cpu_count,difficulty_name(difficulty));
    int total=cpu_count+1;fighter_t *fighters=(fighter_t*)calloc((size_t)total,sizeof(*fighters));
    if(!fighters){lf2_logf("ERROR","fighter heap allocation failed count=%d",total);return -10;}
    lf2_log_memory("fighters allocated");
    if(load_fighter(&fighters[0],player_index,false,player_team,spawn_x[0],spawn_z[0],0,1)<0){free(fighters);return -2;}
    int loaded=1;
    for(int i=0;i<cpu_count;i++){
        if(load_fighter(&fighters[i+1],cpu_indices[i],true,cpu_teams[i],spawn_x[i+1],spawn_z[i+1],1,i+1)<0){
            vita2d_wait_rendering_done();for(int j=0;j<loaded;j++)lf2_free_character_textures(&fighters[j].def);free(fighters);return -3-i;
        }loaded++;
    }
    lf2_log_stage("match:gpu_ready","p1=%s cpus=%d diff=%s cheat=%d pool_free=%u",fighters[0].def.name,cpu_count,difficulty_name(difficulty),cheat_enabled?1:0,(unsigned)vita2d_pool_free_space());
    lf2_log_stage("match:first_frame_submit","pool_free=%u",(unsigned)vita2d_pool_free_space());
    draw_match(font,stage_bg,fighters,cpu_count,difficulty,cheat_enabled,false);vita2d_wait_rendering_done();
    lf2_log_stage("match:first_frame_complete","pool_free=%u",(unsigned)vita2d_pool_free_space());
    uint32_t prev=0;SceCtrlData first={0};sceCtrlPeekBufferPositive(0,&first,1);prev=first.buttons;
    uint32_t pending_pressed=0;
    bool paused=false;int result=0;
    srand(0x4c463256u+(unsigned)player_index*17u+(unsigned)cpu_count*47u+(unsigned)difficulty*101u);
    /* LF2 logic is based on 30 Hz time units. Rendering remains vblank-paced
       at 60 Hz, while gameplay advances at a fixed 30 Hz. This keeps wait,
       walking speeds, gravity, attack windows and animation timing in the
       same time domain as the original data files. */
    const uint64_t sim_step_us=33333;
    uint64_t last_us=sceKernelGetSystemTimeWide(),acc_us=0;
    lf2_logf("ANIM","fixed simulation rate=30Hz render_vblank=60Hz");
    for(;;){
        SceCtrlData pad={0};sceCtrlPeekBufferPositive(0,&pad,1);
        uint32_t raw_pressed=pad.buttons&~prev;prev=pad.buttons;pending_pressed|=raw_pressed;
        uint64_t now_us=sceKernelGetSystemTimeWide();uint64_t dt_us=now_us-last_us;last_us=now_us;
        if(dt_us>100000)dt_us=100000; /* debugger / scheduling stall guard */

        if(raw_pressed&SCE_CTRL_START){paused=!paused;acc_us=0;pending_pressed=0;lf2_logf("GAME","pause=%d",paused?1:0);}
        if(paused){
            if(raw_pressed&SCE_CTRL_CIRCLE){result=0;break;}
            draw_match(font,stage_bg,fighters,cpu_count,difficulty,cheat_enabled,paused);continue;
        }
        if(fighters[0].dead||!player_has_enemy(fighters,total)){
            acc_us=0;pending_pressed=0;
