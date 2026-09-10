typedef struct {int a,b;int human;} champ_team_t;
static void championship_result_2v2(champ_team_t winner,champ_team_t defeated,const champ_team_t *next_opponent,int round_no){
    uint32_t prev=sync_buttons();lf2_log_stage("championship2:result","round=%d winner=%s+%s next=%s",round_no,lf2_roster[winner.a].name,lf2_roster[winner.b].name,next_opponent?lf2_roster[next_opponent->a].name:"CHAMPIONS");
    for(;;){SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;if(pr&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE|SCE_CTRL_START)){ui_ok();return;}
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);vita2d_draw_rectangle(38,38,884,468,RGBA8(3,24,80,242));
        if(lf2_ui_font_ready()){
            lf2_ui_draw_text(66,58,1.0f,RGBA8(255,255,255,255),"2 on 2 Championship");lf2_ui_draw_textf(748,63,0.70f,RGBA8(255,225,115,255),"Round %d",round_no);
            lf2_ui_draw_text(88,116,0.76f,RGBA8(255,225,115,255),"WINNING TEAM");draw_portrait_index(winner.a,80,155,1.0f);draw_portrait_index(winner.b,195,155,1.0f);lf2_ui_draw_textf(78,286,0.72f,RGBA8(255,255,255,255),"%s + %s",lf2_roster[winner.a].name,lf2_roster[winner.b].name);
            lf2_ui_draw_text(390,157,0.66f,RGBA8(175,205,245,255),"defeated");lf2_ui_draw_textf(385,192,0.64f,RGBA8(225,235,255,255),"%s + %s",lf2_roster[defeated.a].name,lf2_roster[defeated.b].name);
            if(next_opponent){lf2_ui_draw_text(635,116,0.76f,RGBA8(125,205,255,255),"NEXT TEAM");draw_portrait_index(next_opponent->a,620,155,1.0f);draw_portrait_index(next_opponent->b,735,155,1.0f);lf2_ui_draw_textf(615,286,0.68f,RGBA8(255,255,255,255),"%s + %s",lf2_roster[next_opponent->a].name,lf2_roster[next_opponent->b].name);lf2_ui_draw_text(454,274,1.0f,RGBA8(255,255,255,255),"VS");}
            else lf2_ui_draw_text(630,220,1.0f,RGBA8(255,225,115,255),"CHAMPIONS!");
            lf2_ui_draw_text(312,463,0.68f,RGBA8(220,235,255,255),"X / Square / START: continue");
        }vita2d_end_drawing();vita2d_swap_buffers();
    }
}

static void draw_team_tournament(const champ_team_t *teams,int count,int round){
    uint32_t prev=sync_buttons();for(;;){SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;if(pr&SCE_CTRL_CIRCLE)return;if(pr&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE|SCE_CTRL_START)){ui_ok();return;}
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);vita2d_draw_rectangle(55,45,850,450,RGBA8(3,24,80,240));if(lf2_ui_font_ready()){lf2_ui_draw_text(82,65,1.0f,RGBA8(255,255,255,255),"2 on 2 Championship");lf2_ui_draw_textf(735,69,0.70f,RGBA8(255,224,110,255),"Round %d",round);int cols=count>4?2:1,per=(count+cols-1)/cols;for(int i=0;i<count;i++){int c=i/per,r=i%per;float x=85+c*410,y=125+r*72;vita2d_draw_rectangle(x,y-7,380,52,teams[i].human?RGBA8(70,116,202,235):RGBA8(7,39,111,220));lf2_ui_draw_textf(x+12,y,0.64f,RGBA8(255,255,255,255),"%s + %s",lf2_roster[teams[i].a].name,lf2_roster[teams[i].b].name);}lf2_ui_draw_text(300,460,0.68f,RGBA8(220,235,255,255),"X/Square/START: continue");}vita2d_end_drawing();vita2d_swap_buffers();}
}

static int tournament_2v2(bool cheat){
    (void)cheat;int player=0,partner=1;if(!choose_single_fighter(false,"2 on 2 Championship - Player",&player))return 0;if(!choose_single_fighter(false,"2 on 2 Championship - Partner",&partner))return 0;int diff=choose_difficulty("2 on 2 Championship",false,LF2_DIFF_NORMAL);if(diff<0)return 0;
    champ_team_t cur[8];cur[0]=(champ_team_t){player,partner,1};for(int i=1;i<8;i++)cur[i]=(champ_team_t){rand()%10,rand()%10,0};int count=8;vita2d_texture *bg=load_stage_texture(3);if(!bg)return -1;lf2_log_stage("championship2:start","player=%s partner=%s difficulty=%s",lf2_roster[player].name,lf2_roster[partner].name,ui_diff_name(diff));
    for(int round=1;count>1;round++){
        draw_team_tournament(cur,count,round);champ_team_t next[8];int ni=0;champ_team_t defeated={0};int human_won=0;
        for(int i=0;i<count;i+=2){champ_team_t a=cur[i],b=cur[i+1];if(a.human||b.human){champ_team_t human=a.human?a:b,opp=a.human?b:a;int cpu[3]={human.b,opp.a,opp.b};int teams[3]={1,2,2};char label[64];snprintf(label,sizeof(label),"2 on 2 Championship Round %d",round);lf2_match_options_t opt={0};opt.allow_rematch=false;opt.hud_label=label;int rc=lf2_run_match_ex(g_font,bg,human.a,1,cpu,teams,3,diff,false,&opt);if(rc!=LF2_MATCH_WIN){championship_result_2v2(opp,human,NULL,round);free_stage_texture(bg);mode_card("ELIMINATED","2 on 2 Championship",900);return rc;}defeated=opp;human_won=1;next[ni++]=human;}
            else next[ni++]=(rand()&1)?a:b;}
        if(human_won){int hi=-1;for(int i=0;i<ni;i++)if(next[i].human){hi=i;break;}const champ_team_t *nopp=NULL;if(hi>=0&&ni>1){int sib=(hi&1)?hi-1:hi+1;if(sib<ni)nopp=&next[sib];}championship_result_2v2(next[hi],defeated,nopp,round);}
        memcpy(cur,next,(size_t)ni*sizeof(*cur));count=ni;
    }
    free_stage_texture(bg);mode_card("CHAMPIONS!","2 on 2 Championship cleared",1800);lf2_log_stage("championship2:complete","player=%s partner=%s",lf2_roster[player].name,lf2_roster[partner].name);return LF2_MATCH_WIN;
}

static int battle_config(bool cheat,int *p1,int *ally_type,int *ally_count,int *enemy_hero,int *enemy_type,int *enemy_count,int *stage,int *diff){
    int sel=0,maxdiff=cheat?LF2_DIFF_CRAZY:LF2_DIFF_DIFFICULT;uint32_t prev=sync_buttons();
    for(;;){SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;if(pr&SCE_CTRL_UP){sel=(sel+7)%8;ui_join();}if(pr&SCE_CTRL_DOWN){sel=(sel+1)%8;ui_join();}
        int delta=(pr&SCE_CTRL_RIGHT)?1:((pr&SCE_CTRL_LEFT)?-1:0);if(delta){
            if(sel==0||sel==3){int *v=sel==0?p1:enemy_hero;*v+=delta;if(*v>9)*v=0;if(*v<0)*v=9;}
            else if(sel==1||sel==4){int *v=sel==1?ally_type:enemy_type;*v+=delta;if(*v>17)*v=10;if(*v<10)*v=17;}
            else if(sel==2||sel==5){int *v=sel==2?ally_count:enemy_count;*v+=delta;if(*v>3)*v=0;if(*v<0)*v=3;}
            else if(sel==6){*stage+=delta;if(*stage>=STAGE_COUNT)*stage=0;if(*stage<0)*stage=STAGE_COUNT-1;}
            else if(sel==7){*diff+=delta;if(*diff>maxdiff)*diff=LF2_DIFF_EASY;if(*diff<LF2_DIFF_EASY)*diff=maxdiff;}ui_join();}
        if(pr&SCE_CTRL_CIRCLE){ui_cancel();return 0;}if(pr&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE|SCE_CTRL_START)){ui_ok();return 1;}
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);vita2d_draw_rectangle(70,50,820,445,RGBA8(3,24,80,242));if(lf2_ui_font_ready()){lf2_ui_draw_text(92,68,1.0f,RGBA8(255,255,255,255),"Battle Mode");lf2_ui_draw_text(95,112,0.80f,RGBA8(125,196,255,255),"TEAM 1");lf2_ui_draw_text(520,112,0.80f,RGBA8(255,145,145,255),"TEAM 2");
            const char *left_labels[]={"Hero","Follower","In Screen"};const char *right_labels[]={"Hero","Follower","In Screen"};for(int i=0;i<3;i++){float y=160+i*54;if(sel==i)vita2d_draw_rectangle(90,y-7,360,42,RGBA8(83,132,215,235));if(sel==i+3)vita2d_draw_rectangle(510,y-7,350,42,RGBA8(83,132,215,235));lf2_ui_draw_textf(110,y,0.66f,RGBA8(255,255,255,255),"%s: %s",left_labels[i],i==0?lf2_roster[*p1].name:(i==1?lf2_roster[*ally_type].name:""));if(i==2)lf2_ui_draw_textf(290,y,0.66f,RGBA8(255,232,125,255),"%d",*ally_count);lf2_ui_draw_textf(530,y,0.66f,RGBA8(255,255,255,255),"%s: %s",right_labels[i],i==0?lf2_roster[*enemy_hero].name:(i==1?lf2_roster[*enemy_type].name:""));if(i==2)lf2_ui_draw_textf(720,y,0.66f,RGBA8(255,232,125,255),"%d",*enemy_count);}
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

static int confirm_quit(void){
    int yes=0;uint32_t prev=sync_buttons();
    for(;;){SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;
        if(pr&(SCE_CTRL_LEFT|SCE_CTRL_RIGHT)){yes=!yes;ui_join();}
        if(pr&SCE_CTRL_CIRCLE){ui_cancel();return 0;}
        if(pr&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE)){ui_ok();return yes;}
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);vita2d_draw_rectangle(240,185,480,165,RGBA8(8,31,92,245));
        if(g_font){vita2d_pgf_draw_text(g_font,338,230,RGBA8(255,255,255,255),1.0f,"Quit Little Fighter 2?");
            if(!yes)vita2d_draw_rectangle(330,270,110,38,RGBA8(110,150,220,240));else vita2d_draw_rectangle(520,270,110,38,RGBA8(110,150,220,240));
            vita2d_pgf_draw_text(g_font,360,297,RGBA8(255,255,255,255),0.66f,"No");vita2d_pgf_draw_text(g_font,545,297,RGBA8(255,255,255,255),0.66f,"Yes");}
        vita2d_end_drawing();vita2d_swap_buffers();}
}

static void main_menu(bool cheat){
    /* Keep the order and naming of LF2 2.0a's Game Mode menu. */
    const char *items[]={"VS mode","Stage mode","1 on 1 Championship","2 on 2 Championship","Battle mode","Demo","Playback Recording","Quit"};
    const int count=8;int sel=0;uint32_t prev=sync_buttons();char note[160]={0};int note_ticks=0;lf2_log_stage("menu:open","cheat=%d style=classic",cheat?1:0);
    for(;;){
        SceCtrlData pad={0};sceCtrlPeekBufferPositive(0,&pad,1);uint32_t pr=pad.buttons&~prev;prev=pad.buttons;
        if(pr&SCE_CTRL_UP){sel=(sel+count-1)%count;ui_join();}if(pr&SCE_CTRL_DOWN){sel=(sel+1)%count;ui_join();}
        if(pr&SCE_CTRL_SELECT){manual_view();prev=sync_buttons();}
        if(pr&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE)){
            lf2_logf("UI","menu select index=%d item=%s",sel,items[sel]);ui_ok();
            if(sel==0){run_vs(cheat);prev=sync_buttons();}
            else if(sel==1){run_stage(cheat);prev=sync_buttons();}
            else if(sel==2){tournament_1v1(cheat);prev=sync_buttons();}
            else if(sel==3){tournament_2v2(cheat);prev=sync_buttons();}
            else if(sel==4){run_battle(cheat);prev=sync_buttons();}
            else if(sel==5){run_demo();prev=sync_buttons();}
            else if(sel==6){playback_recording();prev=sync_buttons();}
            else if(sel==7){if(confirm_quit())break;prev=sync_buttons();}
        }
        if(note_ticks>0)note_ticks--;
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);
        vita2d_draw_rectangle(0,0,960,544,RGBA8(10,38,105,135));
        vita2d_draw_rectangle(40,175,500,325,RGBA8(9,37,102,218));
        if(lf2_ui_font_ready()){
            /* v0.61: critical menu text uses a pre-rasterized, outlined bitmap
               atlas instead of tiny fractional PGF glyphs. This stays sharp on
               the Vita LCD/OLED and is readable from normal handheld distance. */
            lf2_ui_draw_text(48,72,1.35f,RGBA8(255,255,255,255),"Little Fighter 2");
            lf2_ui_draw_text(50,122,0.78f,RGBA8(220,235,255,255),"Version 2.00a   PS Vita 0.63");
            for(int i=0;i<count;i++){
                float y=190.0f+i*39.0f;
                if(i==sel)vita2d_draw_rectangle(70,y-5,455,36,RGBA8(126,166,235,245));
                else vita2d_draw_rectangle(70,y-5,455,36,RGBA8(7,29,82,145));
                lf2_ui_draw_text(92,y,0.94f,RGBA8(255,255,255,255),items[i]);
            }
            if(cheat)lf2_ui_draw_text(592,438,0.82f,RGBA8(255,225,80,255),"LF2.NET / CRAZY unlocked");
            lf2_ui_draw_text(592,482,0.72f,RGBA8(235,242,255,255),"D-Pad: select");
            lf2_ui_draw_text(592,509,0.72f,RGBA8(235,242,255,255),"X or Square: OK   SELECT: Help");
            if(note_ticks)lf2_ui_draw_text(530,148,0.68f,RGBA8(255,220,120,255),note);
        } else if(g_font){
            vita2d_pgf_draw_text(g_font,48,112,RGBA8(255,255,255,255),1.48f,"Little Fighter 2");
        }
        vita2d_end_drawing();vita2d_swap_buffers();
    }
    lf2_log_stage("menu:closed","");
}

static void load_ui_assets(void){
    lf2_log_stage("graphics:init","");vita2d_init();vita2d_set_clear_color(RGBA8(5,12,28,255));vita2d_set_vblank_wait(1);sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
    g_font=vita2d_load_default_pgf();lf2_logf("INFO","font=%p",g_font);lf2_ui_font_init();
    g_menu_bg=vita2d_load_PNG_file("app0:/assets/menu_bg.png");g_portraits=vita2d_load_PNG_file("app0:/assets/portraits.png");
    lf2_logf("INFO","UI textures menu=%p portraits=%p",g_menu_bg,g_portraits);lf2_log_memory("base ui loaded");
}
static void free_ui_assets(void){vita2d_wait_rendering_done();if(g_portraits)vita2d_free_texture(g_portraits);if(g_manual_bg)vita2d_free_texture(g_manual_bg);if(g_menu_bg)vita2d_free_texture(g_menu_bg);if(g_check_bg)vita2d_free_texture(g_check_bg);if(g_font)vita2d_free_pgf(g_font);lf2_ui_font_shutdown();vita2d_fini();}

int main(void){
    lf2_log_init();lf2_log_install_signal_handlers();lf2_log_stage("boot","main stack=%u",(unsigned)sceUserMainThreadStackSize);launch_mode_t mode=detect_launch_mode();lf2_logf("INFO","launch mode=%d",mode);load_ui_assets();
    {
        g_check_bg=vita2d_load_PNG_file("app0:/assets/check_bg.png");lf2_logf("INFO","check background=%p",g_check_bg);
        char error[640]={0};int rc=lf2_verify_bundle("app0:/game.lf2pak",check_progress,NULL,error,sizeof(error));
        if(g_check_bg){vita2d_wait_rendering_done();vita2d_free_texture(g_check_bg);g_check_bg=NULL;lf2_log_memory("check bg freed");}
        if(rc<0){lf2_logf("ERROR","filecheck failed rc=%d error=%s",rc,error);wait_message("Dateicheck fehlgeschlagen",error[0]?error:"Unbekannter Fehler beim Prüfen der Spieldateien.");}
        else {int arc=lf2_audio_init();lf2_logf("INFO","audio init rc=%d ready=%d",arc,lf2_audio_ready());main_menu(mode==LAUNCH_CHEAT);}
    }
    lf2_log_stage("shutdown:resources","");lf2_audio_shutdown();lf2_pak_shutdown();free_ui_assets();lf2_log_shutdown(1);sceKernelExitProcess(0);return 0;
}
