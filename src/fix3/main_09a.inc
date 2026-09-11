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
