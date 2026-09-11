            float y=350;if(sel==6)vita2d_draw_rectangle(90,y-7,770,42,RGBA8(83,132,215,235));lf2_ui_draw_textf(110,y,0.68f,RGBA8(255,255,255,255),"Background: %s",g_stages[*stage].name);y=402;if(sel==7)vita2d_draw_rectangle(90,y-7,770,42,RGBA8(83,132,215,235));lf2_ui_draw_textf(110,y,0.68f,RGBA8(255,255,255,255),"Difficulty: %s",ui_diff_name(*diff));lf2_ui_draw_text(205,462,0.64f,RGBA8(220,235,255,255),"Up/Down: option  Left/Right: value  X/Square: Fight  Circle: Back");}
        vita2d_end_drawing();vita2d_swap_buffers();}
}

static int run_battle(bool cheat){
    int p1=0,ally=10,ac=2,enemy_hero=1,enemy_type=10,ec=3,stage=rand()%STAGE_COUNT,diff=LF2_DIFF_NORMAL;
    if(!battle_config(cheat,&p1,&ally,&ac,&enemy_hero,&enemy_type,&ec,&stage,&diff))return 0;
    int cpus[LF2_MAX_CPUS],teams[LF2_MAX_CPUS],n=0;for(int i=0;i<ac&&n<LF2_MAX_CPUS;i++){cpus[n]=ally;teams[n++]=1;}cpus[n]=enemy_hero;teams[n++]=2;for(int i=0;i<ec&&n<LF2_MAX_CPUS;i++){cpus[n]=enemy_type;teams[n++]=2;}
    vita2d_texture *bg=load_stage_texture(stage);if(!bg)return -1;lf2_match_options_t opt={0};opt.allow_rematch=true;opt.hud_label="Battle Mode";lf2_log_stage("battle:start","team1=%s + %dx%s team2=%s + %dx%s",lf2_roster[p1].name,ac,lf2_roster[ally].name,lf2_roster[enemy_hero].name,ec,lf2_roster[enemy_type].name);int rc=lf2_run_match_ex(g_font,bg,p1,1,cpus,teams,n,diff,cheat,&opt);free_stage_texture(bg);return rc;
}

static int run_demo(void){
    lf2_log_stage("demo:start","zero-player random teams");unsigned match=0;for(;;){int p1=rand()%10,cpus[7],teams[7];for(int i=0;i<7;i++){cpus[i]=rand()%10;teams[i]=i<3?1:2;}int stage=rand()%STAGE_COUNT;vita2d_texture *bg=load_stage_texture(stage);if(!bg)return -1;char label[64];snprintf(label,sizeof(label),"Demo %u - Circle: Menu",++match);lf2_match_options_t opt={0};opt.player_ai=true;opt.auto_continue=true;opt.allow_cancel=true;opt.hud_label=label;int rc=lf2_run_match_ex(g_font,bg,p1,1,cpus,teams,7,LF2_DIFF_NORMAL,false,&opt);free_stage_texture(bg);if(rc==LF2_MATCH_CANCEL){lf2_log_stage("demo:closed","matches=%u",match);return 0;}mode_card("DEMO",g_stages[stage].name,250);}
}

static int playback_recording(void){
    /* The v2.00a bundle contains nine demo .lfr files. Keep the original file picker flow now,
       while the deterministic LFR event-stream decoder is implemented separately from live gameplay. */
    static const char *files[]={"Demo_1on1.lfr","Demo_2on2.lfr","Demo_Battle.lfr","Demo_Stage_1.lfr","Demo_Stage_2.lfr","Demo_Stage_3.lfr","Demo_Stage_4.lfr","Demo_Stage_5.lfr","Demo_Survival.lfr","Demo_VS.lfr"};int count=(int)(sizeof(files)/sizeof(files[0])),sel=0;uint32_t prev=sync_buttons();
    for(;;){SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;if(pr&SCE_CTRL_UP){sel=(sel+count-1)%count;ui_join();}if(pr&SCE_CTRL_DOWN){sel=(sel+1)%count;ui_join();}if(pr&SCE_CTRL_CIRCLE){ui_cancel();return 0;}if(pr&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE)){ui_ok();lf2_logf("MODE","playback selected recording/%s",files[sel]);wait_message("Playback Recording","LFR-Datei erkannt. Der deterministische 2.00a-Replay-Decoder folgt in einem separaten kleinen Update.");prev=sync_buttons();}
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);vita2d_draw_rectangle(120,40,720,470,RGBA8(3,24,80,242));if(lf2_ui_font_ready()){lf2_ui_draw_text(150,60,1.0f,RGBA8(255,255,255,255),"Playback Recording");for(int i=0;i<count;i++){float y=112+i*34;if(i==sel)vita2d_draw_rectangle(145,y-5,650,30,RGBA8(92,139,221,235));lf2_ui_draw_text(165,y,0.68f,RGBA8(255,255,255,255),files[i]);}lf2_ui_draw_text(175,472,0.64f,RGBA8(220,235,255,255),"X/Square: Open    Circle: Back");}vita2d_end_drawing();vita2d_swap_buffers();}
}

static int network_play_menu(void){
    const char *items[]={"Vita Ad-Hoc - Host","Vita Ad-Hoc - Game Join","PC LF2 2.00a - Host","PC LF2 2.00a - Game Join","Back"};
    const int count=5;int sel=0;uint32_t prev=sync_buttons();lf2_log_stage("network:menu","transport=scaffold pc_tcp_port=12345");
    for(;;){
        SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;
        if(pr&SCE_CTRL_UP){sel=(sel+count-1)%count;ui_join();}if(pr&SCE_CTRL_DOWN){sel=(sel+1)%count;ui_join();}
        if(pr&SCE_CTRL_CIRCLE){ui_cancel();return 0;}
        if(pr&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE)){
            if(sel==4){ui_cancel();return 0;}ui_ok();lf2_logf("NET","selected=%s",items[sel]);
            wait_message("Network Play","Transport-Menue vorbereitet. Vita Ad-Hoc und PC-LF2-2.00a-Protokoll sind noch nicht freigeschaltet; PC-Kompatibilitaet wird auf TCP 12345 aufgebaut.");prev=sync_buttons();
        }
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);vita2d_draw_rectangle(135,58,690,430,RGBA8(3,24,80,245));
        if(lf2_ui_font_ready()){lf2_ui_draw_text(170,78,1.0f,RGBA8(255,255,255,255),"Network Play");lf2_ui_draw_text(170,116,0.62f,RGBA8(180,215,255,255),"Vita Ad-Hoc + PC LF2 2.00a compatibility roadmap");
            for(int i=0;i<count;i++){float y=165+i*50;if(i==sel)vita2d_draw_rectangle(165,y-7,620,38,RGBA8(92,139,221,235));lf2_ui_draw_text(190,y,0.72f,RGBA8(255,255,255,255),items[i]);}
            lf2_ui_draw_text(170,424,0.56f,RGBA8(255,225,125,255),"PC mode target: original LF2 2.00a TCP port 12345");lf2_ui_draw_text(170,454,0.58f,RGBA8(220,235,255,255),"X/Square: select   Circle: back");}
        vita2d_end_drawing();vita2d_swap_buffers();
    }
}

static int confirm_quit(void){
    int yes=0;uint32_t prev=sync_buttons();
    for(;;){SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;
        if(pr&(SCE_CTRL_LEFT|SCE_CTRL_RIGHT)){yes=!yes;ui_join();}
        if(pr&SCE_CTRL_CIRCLE){ui_cancel();return 0;}
        if(pr&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE)){ui_ok();return yes;}
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);vita2d_draw_rectangle(240,185,480,165,RGBA8(8,31,92,245));
        if(g_font){vita2d_pgf_draw_text(g_font,338,230,RGBA8(255,255,255,255),1.0f,"Quit Little Fighter 2?");
            if(!yes)vita2d_draw_rectangle(330,270,110,38,RGBA8(110,150,220,240));else vita2d_draw_rectangle(520,270,110,38,RGBA8(110,150,220,240));
