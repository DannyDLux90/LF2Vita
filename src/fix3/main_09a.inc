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

static int run_reference_recording(const char *recording_name){
    if(!recording_name||!recording_name[0])return -1;
    char stem[80];snprintf(stem,sizeof(stem),"%s",recording_name);
    char *dot=strrchr(stem,'.');if(dot)*dot=0;
    char app_path[160],data_path[180];
    snprintf(app_path,sizeof(app_path),"app0:/ref/recording_%s.l2rf",stem);
    snprintf(data_path,sizeof(data_path),"ux0:data/LF2V00001/ref/recording_%s.l2rf",stem);

    lf2_ref_replay_t replay;int rc=lf2_ref_replay_load(app_path,&replay);
    const char *used_path=app_path;
    if(rc<0){rc=lf2_ref_replay_load(data_path,&replay);used_path=data_path;}
    if(rc<0){
        lf2_logf("REF","fixture missing recording=%s app_rc=%d data=%s",recording_name,rc,data_path);
        wait_message("Reference Replay","Keine .l2rf-Referenzdatei gefunden. Siehe lf2.log.");
        return rc;
    }
    if(replay.mode==1){
        lf2_logf("REF","stage fixture deferred path=%s stage_raw=%d",used_path,replay.stage_raw);
        lf2_ref_replay_free(&replay);
        wait_message("Reference Replay","Stage-Replays folgen nach dem VS/Championship-Determinismus.");
        return -20;
    }

    lf2_ref_match_plan_t plan;rc=lf2_ref_replay_build_match(&replay,&plan);
    if(rc<0){lf2_logf("REF","match plan failed path=%s rc=%d",used_path,rc);lf2_ref_replay_free(&replay);wait_message("Reference Replay","Roster/Match-Konfiguration konnte nicht rekonstruiert werden.");return rc;}
    vita2d_texture *bg=load_stage_texture(plan.stage_index);
    if(!bg){lf2_ref_replay_free(&replay);wait_message("Reference Replay","Original-Hintergrund konnte nicht geladen werden.");return -21;}

    lf2_stock_stat_t actual[LF2_REF_PLAYER_COUNT];memset(actual,0,sizeof(actual));int actual_count=0;
    lf2_match_options_t opt;memset(&opt,0,sizeof(opt));
    opt.hud_label="LF2 2.00a Reference Replay";
    opt.lockstep_actor_map_enabled=true;
    for(int i=0;i<plan.actor_count&&i<(LF2_MAX_CPUS+1);i++)opt.lockstep_actor_control[i]=plan.actor_control[i];
    opt.lockstep_frame=lf2_ref_replay_clock;opt.remote_userdata=&replay;
    opt.reference_tu_limit=replay.movie_tus;opt.reference_fast_forward=true;
    opt.stock_stats_out=actual;opt.stock_stats_capacity=LF2_REF_PLAYER_COUNT;opt.stock_stats_count_out=&actual_count;

    lf2_ref_replay_reset(&replay);rc=lf2_ref_replay_enable_rng(&replay);
    if(rc<0){free_stage_texture(bg);lf2_ref_replay_free(&replay);wait_message("Reference Replay","Original-RNG konnte nicht initialisiert werden.");return rc;}
    lf2_log_stage("reference:start","file=%s mode=%d actors=%d tus=%u bg=%d diff=%d",used_path,replay.mode,plan.actor_count,(unsigned)replay.movie_tus,plan.stage_index,plan.difficulty);
    int match_rc=lf2_run_match_ex(g_font,bg,plan.player_index,plan.player_team,plan.cpu_indices,plan.cpu_teams,plan.cpu_count,plan.difficulty,false,&opt);
    int mismatches=lf2_ref_replay_compare_stats(&replay,&plan,actual,actual_count);
    lf2_log_stage("reference:done","file=%s match_rc=%d mismatches=%d replay_tu=%u/%u rng_calls=%u",used_path,match_rc,mismatches,(unsigned)replay.tu,(unsigned)replay.movie_tus,(unsigned)lf2_rng_calls());
    lf2_rng_stock_disable();free_stage_texture(bg);lf2_ref_replay_free(&replay);

    char message[180];
    if(mismatches==0)snprintf(message,sizeof(message),"Referenzlauf stimmt in allen erfassten Endstatistiken ueberein.");
    else snprintf(message,sizeof(message),"Referenzlauf beendet: %d Abweichungen. Details stehen als REFCMP in lf2.log.",mismatches);
    wait_message("LF2 2.00a Reference",message);
    return mismatches;
}

static int playback_recording(void){
