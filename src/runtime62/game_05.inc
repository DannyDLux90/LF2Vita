        if(lf2_ui_font_ready()){
            lf2_ui_draw_textf(28,67,0.86f,RGBA8(255,255,255,255),"P1 %s / %s",p->def.name,team_name(p->team));
            lf2_ui_draw_text(18,498,0.68f,RGBA8(255,255,255,255),"D-Pad/Stick move  Square attack  X jump  Circle/L defend  Triangle/R special");
            lf2_ui_draw_textf(405,68,0.68f,RGBA8(235,242,255,255),"Difficulty: %s",difficulty_name(difficulty));
            if(options && options->hud_label && options->hud_label[0])
                lf2_ui_draw_text(405,91,0.68f,RGBA8(255,235,145,255),options->hud_label);
            if(cheat)lf2_ui_draw_text(405,39,0.68f,RGBA8(255,225,80,255),"LF2.NET / CRAZY");
        } else {
            vita2d_pgf_draw_textf(font,28,83,RGBA8(255,255,255,255),0.92f,"P1 %s",p->def.name);
        }
        if(!player_side_alive(fighters,total)||!player_has_enemy(fighters,total)){
            vita2d_draw_rectangle(235,185,490,135,RGBA8(0,0,0,205));
            const char *msg=player_side_won(fighters,total)?"K.O. - Spieler 1 gewinnt":"K.O. - Spieler 1 ist besiegt";
            if(lf2_ui_font_ready()){
                float tw=lf2_ui_text_width(0.92f,msg);
                lf2_ui_draw_text(480.0f-tw*0.5f,216,0.92f,RGBA8(255,255,255,255),msg);
                if(options && options->auto_continue)
                    lf2_ui_draw_text(337,268,0.72f,RGBA8(235,242,255,255),"Weiter...");
                else if(options && !options->allow_rematch)
                    lf2_ui_draw_text(280,268,0.72f,RGBA8(235,242,255,255),"X: Continue     Circle: Menu");
                else
                    lf2_ui_draw_text(302,268,0.72f,RGBA8(235,242,255,255),"X: Rematch     Circle: Menu");
            } else {
                int tw=vita2d_pgf_text_width(font,1.15f,msg);
                vita2d_pgf_draw_text(font,480-tw/2,235,RGBA8(255,255,255,255),1.15f,msg);
            }
        } else if(paused){
            vita2d_draw_rectangle(285,190,390,120,RGBA8(0,0,0,215));
            if(lf2_ui_font_ready()){
                lf2_ui_draw_text(418,216,1.0f,RGBA8(255,255,255,255),"PAUSE");
                lf2_ui_draw_text(330,264,0.72f,RGBA8(235,242,255,255),"START: Resume   Circle: Menu");
            } else {
                vita2d_pgf_draw_text(font,420,235,RGBA8(255,255,255,255),1.1f,"PAUSE");
            }
        }
    }
    vita2d_end_drawing(); vita2d_swap_buffers();
}

static int load_fighter(fighter_t *f,int roster_idx,bool ai,int team,float x,float z,int facing,int slot,int hp) {
    memset(f,0,sizeof(*f)); f->ai=ai;f->team=team; f->x=x; f->z=z; f->facing=facing;
    if(hp<=0)hp=500; f->hp=f->max_hp=hp; f->mp=500;
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

int lf2_run_match_ex(vita2d_pgf *font, vita2d_texture *stage_bg,
                     int player_index, int player_team,
                     const int *cpu_indices, const int *cpu_teams, int cpu_count,
                     int difficulty, bool cheat_enabled,
                     const lf2_match_options_t *options) {
    lf2_match_options_t defaults={0};
    defaults.allow_rematch=true;
    if(!options)options=&defaults;
    if(player_index<0||player_index>=lf2_roster_count||!cpu_indices||!cpu_teams||cpu_count<1||cpu_count>LF2_MAX_CPUS)return -1;
    if(player_team<0||player_team>4)return -1;
    if(difficulty<LF2_DIFF_EASY||difficulty>LF2_DIFF_CRAZY)return -1;
    for(int i=0;i<cpu_count;i++)if(cpu_indices[i]<0||cpu_indices[i]>=lf2_roster_count||cpu_teams[i]<0||cpu_teams[i]>4)return -1;
    lf2_log_stage("match:alloc","fighter_size=%u p1=%s team=%d cpus=%d difficulty=%s",(unsigned)sizeof(fighter_t),lf2_roster[player_index].name,player_team,cpu_count,difficulty_name(difficulty));
    int total=cpu_count+1;fighter_t *fighters=(fighter_t*)calloc((size_t)total,sizeof(*fighters));
    if(!fighters){lf2_logf("ERROR","fighter heap allocation failed count=%d",total);return -10;}
    lf2_log_memory("fighters allocated");
    int pmax=options->player_max_hp>0?options->player_max_hp:500;
    if(load_fighter(&fighters[0],player_index,options->player_ai,player_team,spawn_x[0],spawn_z[0],0,1,pmax)<0){free(fighters);return -2;}
    if(options->player_hp>0&&options->player_hp<pmax)fighters[0].hp=options->player_hp;
    fighters[0].mp=(options->player_mp>0&&options->player_mp<=500)?options->player_mp:500;
    int loaded=1;
    for(int i=0;i<cpu_count;i++){
        int chp=(options->cpu_hp&&options->cpu_hp[i]>0)?options->cpu_hp[i]:500;
        if(load_fighter(&fighters[i+1],cpu_indices[i],true,cpu_teams[i],spawn_x[i+1],spawn_z[i+1],1,i+1,chp)<0){
            vita2d_wait_rendering_done();for(int j=0;j<loaded;j++)lf2_free_character_textures(&fighters[j].def);free(fighters);return -3-i;
        }loaded++;
    }
    lf2_log_stage("match:gpu_ready","p1=%s cpus=%d diff=%s cheat=%d pool_free=%u",fighters[0].def.name,cpu_count,difficulty_name(difficulty),cheat_enabled?1:0,(unsigned)vita2d_pool_free_space());
    lf2_log_stage("match:first_frame_submit","pool_free=%u",(unsigned)vita2d_pool_free_space());
    draw_match(font,stage_bg,fighters,cpu_count,difficulty,cheat_enabled,false,options);vita2d_wait_rendering_done();
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
    uint64_t result_since=0;
    for(;;){
        SceCtrlData pad={0};sceCtrlPeekBufferPositive(0,&pad,1);
        uint32_t raw_pressed=pad.buttons&~prev;prev=pad.buttons;pending_pressed|=raw_pressed;
        uint64_t now_us=sceKernelGetSystemTimeWide();uint64_t dt_us=now_us-last_us;last_us=now_us;
        if(dt_us>100000)dt_us=100000; /* debugger / scheduling stall guard */

        if(options->player_ai && options->allow_cancel && (raw_pressed&SCE_CTRL_CIRCLE)){result=LF2_MATCH_CANCEL;break;}
