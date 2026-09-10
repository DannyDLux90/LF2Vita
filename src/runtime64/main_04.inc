
static void mode_card(const char *title,const char *subtitle,int milliseconds){
    SceInt64 end=sceKernelGetSystemTimeWide()+(SceInt64)(milliseconds>0?milliseconds:1)*1000LL;
    do{
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);
        vita2d_draw_rectangle(0,0,960,544,RGBA8(4,22,72,185));
        vita2d_draw_rectangle(120,180,720,180,RGBA8(4,21,68,235));
        if(lf2_ui_font_ready()){
            float tw=lf2_ui_text_width(1.15f,title?title:"");
            lf2_ui_draw_text(480.0f-tw*0.5f,215,1.15f,RGBA8(255,255,255,255),title?title:"");
            if(subtitle&&*subtitle){tw=lf2_ui_text_width(0.75f,subtitle);lf2_ui_draw_text(480.0f-tw*0.5f,286,0.75f,RGBA8(215,232,255,255),subtitle);}
        }
        vita2d_end_drawing();vita2d_swap_buffers();
    }while(sceKernelGetSystemTimeWide()<end);
}

static int choose_difficulty(const char *mode,bool allow_crazy,int initial){
    int d=initial,max=allow_crazy?LF2_DIFF_CRAZY:LF2_DIFF_DIFFICULT;uint32_t prev=sync_buttons();
    if(d<0||d>max)d=LF2_DIFF_NORMAL;
    for(;;){
        SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;
        if(pr&(SCE_CTRL_LEFT|SCE_CTRL_UP)){d=(d+max)%(max+1);ui_join();}
        if(pr&(SCE_CTRL_RIGHT|SCE_CTRL_DOWN)){d=(d+1)%(max+1);ui_join();}
        if(pr&SCE_CTRL_CIRCLE){ui_cancel();return -1;}
        if(pr&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE)){ui_ok();lf2_logf("MODE","%s difficulty=%s",mode,ui_diff_name(d));return d;}
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);vita2d_draw_rectangle(160,145,640,255,RGBA8(3,25,83,240));
        if(lf2_ui_font_ready()){
            lf2_ui_draw_text(330,176,1.0f,RGBA8(255,255,255,255),mode);
            lf2_ui_draw_text(350,226,0.72f,RGBA8(185,215,255,255),"Difficulty");
            vita2d_draw_rectangle(290,268,380,55,RGBA8(94,137,218,245));
            float tw=lf2_ui_text_width(1.0f,ui_diff_name(d));lf2_ui_draw_text(480.0f-tw*.5f,282,1.0f,RGBA8(255,255,255,255),ui_diff_name(d));
            lf2_ui_draw_text(262,355,0.68f,RGBA8(225,238,255,255),"Left/Right: select    X/Square: OK    Circle: back");
        }
        vita2d_end_drawing();vita2d_swap_buffers();
    }
}

static int choose_single_fighter(bool cheat,const char *mode,int *fighter){
    vs_setup_t setup;setup_defaults(&setup);setup.cpu_count=0;setup.p1=*fighter;setup.p1_team=0;
    lf2_log_stage("mode:fighter_select","mode=%s cheat=%d",mode,cheat?1:0);
    if(!select_slot(cheat,&setup,0,&setup.p1,&setup.p1_team))return 0;
    *fighter=setup.p1;return 1;
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

static int run_stage_match_batch(vita2d_texture *stage_tex,int fighter,int difficulty,bool cheat,
                                 const int *enemies,const int *hp,int count,const char *label,
                                 int *carry_hp,int *carry_mp){
    int teams[LF2_MAX_CPUS];for(int i=0;i<count;i++)teams[i]=2;
    lf2_match_options_t opt={0};opt.allow_rematch=false;opt.auto_continue=true;opt.allow_cancel=true;opt.hud_label=label;
    opt.player_hp=*carry_hp;opt.player_max_hp=500;opt.player_mp=*carry_mp;opt.cpu_hp=hp;opt.player_hp_out=carry_hp;opt.player_mp_out=carry_mp;
    return lf2_run_match_ex(g_font,stage_tex,fighter,1,enemies,teams,count,difficulty,cheat,&opt);
}

static int run_stage(bool cheat){
    int fighter=0,choice=0,difficulty=LF2_DIFF_NORMAL;
    if(!choose_single_fighter(cheat,"Stage Mode",&fighter))return 0;
    if(!choose_stage_start(cheat,&choice,&difficulty))return 0;
    unsigned rng=(unsigned)sceKernelGetSystemTimeWide()^0x53544147u;int p_hp=500,p_mp=500;
    lf2_log_stage("stage_mode:start","fighter=%s choice=%d difficulty=%s",lf2_roster[fighter].name,choice,ui_diff_name(difficulty));

    if(choice==5){
        lf2_stage_t st;if(lf2_stage_load(50,&st)<0){wait_message("Stage Mode Error","data/stage.dat: Survival Stage konnte nicht gelesen werden.");return -1;}
        vita2d_texture *bg=load_stage_texture(1);if(!bg)return -1;
        for(int ph=0;ph<st.phase_count;ph++){
            int enemy[128],hp[128];int n=lf2_stage_build_phase(&st.phases[ph],difficulty,enemy,hp,128,&rng);if(n<=0)continue;
            for(int off=0;off<n;off+=LF2_MAX_CPUS){int batch=n-off;if(batch>LF2_MAX_CPUS)batch=LF2_MAX_CPUS;char label[80];snprintf(label,sizeof(label),"Survival Stage: %d",ph+1);
                lf2_log_stage("stage:phase","survival=%d enemies=%d batch=%d",ph+1,n,batch);
                int rc=run_stage_match_batch(bg,fighter,difficulty,cheat,enemy+off,hp+off,batch,label,&p_hp,&p_mp);
                if(rc!=LF2_MATCH_WIN){free_stage_texture(bg);mode_card("GAME OVER","Survival Stage ended",1200);return rc;}
            }
            /* Original survival supplies bottles between selected waves. Until item pickup is native,
               use a small recovery so the long-form mode remains playable without inventing full heals. */
            if((ph+1)%5==0){p_hp+=60;if(p_hp>500)p_hp=500;p_mp=500;}
        }
        free_stage_texture(bg);mode_card("SURVIVAL CLEAR","All authored survival phases cleared",1600);return LF2_MATCH_WIN;
    }

    for(int major=choice;major<5;major++){
        vita2d_texture *bg=load_stage_texture(stage_background_for_major(major));if(!bg)return -1;
        for(int sub=0;sub<5;sub++){
            int stage_id=major*10+sub;lf2_stage_t st;
            if(lf2_stage_load(stage_id,&st)<0){free_stage_texture(bg);wait_message("Stage Mode Error","Eine Stage-Definition aus data/stage.dat fehlt oder ist ungültig.");return -2;}
            char card[48],subline[64];snprintf(card,sizeof(card),"Stage %d-%d",major+1,sub+1);snprintf(subline,sizeof(subline),"%s   %s",g_stages[stage_background_for_major(major)].name,ui_diff_name(difficulty));mode_card(card,subline,650);
            for(int ph=0;ph<st.phase_count;ph++){
                int enemy[128],hp[128];int n=lf2_stage_build_phase(&st.phases[ph],difficulty,enemy,hp,128,&rng);if(n<=0)continue;
                for(int off=0;off<n;off+=LF2_MAX_CPUS){int batch=n-off;if(batch>LF2_MAX_CPUS)batch=LF2_MAX_CPUS;char label[80];snprintf(label,sizeof(label),"Stage %d-%d  Phase %d/%d",major+1,sub+1,ph+1,st.phase_count);
