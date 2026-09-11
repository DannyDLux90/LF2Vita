    vs_setup_t setup;setup_defaults(&setup);setup.cpu_count=0;setup.p1=*fighter;setup.p1_team=0;
    lf2_log_stage("mode:fighter_select","mode=%s cheat=%d",mode,cheat?1:0);
    if(!select_slot(cheat,&setup,0,&setup.p1,&setup.p1_team))return 0;
    *fighter=setup.p1;return 1;
}

static int select_stage_ally_count(bool cheat,vs_setup_t *setup){
    int count=setup->cpu_count;if(count<0||count>LF2_STAGE_MAX_ALLIES)count=0;
    uint32_t prev=sync_buttons();lf2_log_stage("stage:ally_count","open=1");
    for(;;){
        SceCtrlData pad={0};sceCtrlPeekBufferPositive(0,&pad,1);uint32_t pr=pad.buttons&~prev;prev=pad.buttons;
        if(pr&(SCE_CTRL_LEFT|SCE_CTRL_DOWN)){count=count>0?count-1:LF2_STAGE_MAX_ALLIES;ui_join();}
        if(pr&(SCE_CTRL_RIGHT|SCE_CTRL_UP)){count=count<LF2_STAGE_MAX_ALLIES?count+1:0;ui_join();}
        if(pr&SCE_CTRL_CIRCLE){ui_cancel();return 0;}
        if(pr&(SCE_CTRL_SQUARE|SCE_CTRL_CROSS)){
            setup->cpu_count=count;
            for(int i=0;i<count;i++){if(setup->cpus[i]<0||setup->cpus[i]>=lf2_roster_count)setup->cpus[i]=(setup->p1+i+1)%lf2_roster_count;setup->cpu_teams[i]=1;}
            ui_ok();lf2_log_stage("stage:ally_count","count=%d",count);return 1;
        }
        vs_setup_t preview=*setup;preview.cpu_count=count;preview.p1_team=1;
        for(int i=0;i<count;i++){if(preview.cpus[i]<0||preview.cpus[i]>=lf2_roster_count)preview.cpus[i]=(preview.p1+i+1)%lf2_roster_count;preview.cpu_teams[i]=1;}
        draw_character_selection_base(&preview,-1,-1,1,cheat);
        vita2d_draw_rectangle(18,17,315,165,RGBA8(25,67,145,242));
        if(lf2_ui_font_ready()){
            lf2_ui_draw_text(36,37,0.78f,RGBA8(255,255,255,255),"Stage Mitspieler");
            lf2_ui_draw_textf(43,82,1.15f,RGBA8(255,225,90,255),"%d",count);
            lf2_ui_draw_text(82,88,0.64f,RGBA8(235,242,255,255),"COM-Begleiter");
            lf2_ui_draw_text(36,132,0.58f,RGBA8(215,230,255,255),"Links/Rechts Anzahl");
            lf2_ui_draw_text(36,155,0.58f,RGBA8(215,230,255,255),"X/Quadrat OK   Kreis zurueck");
        }
        vita2d_end_drawing();vita2d_swap_buffers();
    }
}

static int select_stage_ally(bool cheat,vs_setup_t *setup,int slot,int *roster_out){
    int vis[32],n=build_visible_indices(cheat,vis,32),vpos=0;
    for(int i=0;i<n;i++)if(vis[i]==*roster_out){vpos=i;break;}
    uint32_t prev=sync_buttons();lf2_log_stage("stage:ally_select","slot=%d open=1",slot);
    for(;;){
        SceCtrlData pad={0};sceCtrlPeekBufferPositive(0,&pad,1);uint32_t pr=pad.buttons&~prev;prev=pad.buttons;
        if(pr&SCE_CTRL_LEFT){vpos=(vpos+n-1)%n;ui_join();}
        if(pr&SCE_CTRL_RIGHT){vpos=(vpos+1)%n;ui_join();}
        if(pr&SCE_CTRL_CIRCLE){ui_cancel();return 0;}
        if(pr&(SCE_CTRL_SQUARE|SCE_CTRL_CROSS)){
            *roster_out=vis[vpos];setup->cpu_teams[slot-1]=1;ui_ok();
            lf2_log_stage("stage:ally_select","slot=%d fighter=%s",slot,lf2_roster[*roster_out].name);return 1;
        }
        draw_character_selection_base(setup,slot,vis[vpos],1,cheat);
        vita2d_draw_rectangle(165,505,630,34,RGBA8(0,5,28,235));
        if(lf2_ui_font_ready())lf2_ui_draw_text(185,510,0.68f,RGBA8(255,255,255,255),"Links/Rechts Fighter   X/Quadrat OK   Kreis zurueck");
        vita2d_end_drawing();vita2d_swap_buffers();
    }
}

static int choose_stage_party(bool cheat,int *fighter,int *ally_count,int *allies){
    vs_setup_t setup;setup_defaults(&setup);setup.p1=*fighter;setup.p1_team=1;setup.cpu_count=*ally_count;
    for(int i=0;i<LF2_STAGE_MAX_ALLIES;i++){setup.cpus[i]=allies[i]>=0?allies[i]:(i+1);setup.cpu_teams[i]=1;}
    lf2_log_stage("mode:fighter_select","mode=Stage Mode cheat=%d",cheat?1:0);
    if(!select_slot(cheat,&setup,0,&setup.p1,&setup.p1_team))return 0;
    setup.p1_team=1;
    if(!select_stage_ally_count(cheat,&setup))return 0;
    for(int i=0;i<setup.cpu_count;i++)if(!select_stage_ally(cheat,&setup,i+1,&setup.cpus[i]))return 0;
    *fighter=setup.p1;*ally_count=setup.cpu_count;
    for(int i=0;i<LF2_STAGE_MAX_ALLIES;i++)allies[i]=setup.cpus[i];
    return 1;
}

static int choose_stage_start(bool cheat,int *stage_choice,int *difficulty){
    /* 0..4 = Stage 1..5, 5 = Survival */
    int sel=*stage_choice;if(sel<0||sel>5)sel=0;int d=*difficulty;int max=cheat?LF2_DIFF_CRAZY:LF2_DIFF_DIFFICULT;uint32_t prev=sync_buttons();
    const char *names[]={"Stage 1","Stage 2","Stage 3","Stage 4","Stage 5","Survival Stage"};
    for(;;){
        SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;
        if(pr&SCE_CTRL_UP){sel=(sel+5)%6;ui_join();}if(pr&SCE_CTRL_DOWN){sel=(sel+1)%6;ui_join();}
        if(pr&SCE_CTRL_LEFT){d=(d+max)%(max+1);ui_join();}if(pr&SCE_CTRL_RIGHT){d=(d+1)%(max+1);ui_join();}
        if(pr&SCE_CTRL_CIRCLE){ui_cancel();return 0;}
        if(pr&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE)){*stage_choice=sel;*difficulty=d;ui_ok();return 1;}
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);vita2d_draw_rectangle(90,74,650,410,RGBA8(4,26,84,238));
        if(lf2_ui_font_ready()){
            lf2_ui_draw_text(115,95,1.0f,RGBA8(255,255,255,255),"Stage Mode");
            for(int i=0;i<6;i++){float y=147+i*47;if(i==sel)vita2d_draw_rectangle(115,y-7,410,39,RGBA8(106,151,229,245));lf2_ui_draw_text(135,y,0.82f,RGBA8(255,255,255,255),names[i]);}
            lf2_ui_draw_text(565,148,0.68f,RGBA8(190,218,255,255),"Difficulty");lf2_ui_draw_text(565,181,0.88f,RGBA8(255,232,120,255),ui_diff_name(d));
            lf2_ui_draw_text(565,250,0.64f,RGBA8(218,234,255,255),"Up/Down: stage");lf2_ui_draw_text(565,278,0.64f,RGBA8(218,234,255,255),"Left/Right: difficulty");
            lf2_ui_draw_text(565,330,0.64f,RGBA8(218,234,255,255),"X/Square: start");lf2_ui_draw_text(565,358,0.64f,RGBA8(218,234,255,255),"Circle: back");
        }
        vita2d_end_drawing();vita2d_swap_buffers();
    }
}

static int stage_background_for_major(int major){
    static const int bg[5]={7,0,6,5,8}; /* Lion Forest, Stanley Prison, Great Wall, Queen's Island, Forbidden Tower */
    return major>=0&&major<5?bg[major]:1;
}

static int run_stage_continuous(vita2d_texture *stage_tex,int fighter,int difficulty,bool cheat,
