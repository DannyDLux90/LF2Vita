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
                    lf2_log_stage("stage:phase","id=%d phase=%d/%d enemies=%d batch=%d",stage_id,ph+1,st.phase_count,n,batch);
                    int rc=run_stage_match_batch(bg,fighter,difficulty,cheat,enemy+off,hp+off,batch,label,&p_hp,&p_mp);
                    if(rc!=LF2_MATCH_WIN){free_stage_texture(bg);mode_card("GAME OVER",card,1200);return rc;}
                }
                if(ph+1<st.phase_count)mode_card("GO!",card,300);
            }
            int heal=(difficulty==LF2_DIFF_EASY)?200:(difficulty==LF2_DIFF_NORMAL?150:(difficulty==LF2_DIFF_DIFFICULT?100:50));p_hp+=heal;if(p_hp>500)p_hp=500;p_mp=500;
            mode_card("STAGE CLEAR",card,700);
        }
        free_stage_texture(bg);
    }
    mode_card("CONGRATULATIONS!","Stage Mode cleared",1800);lf2_log_stage("stage_mode:complete","fighter=%s",lf2_roster[fighter].name);return LF2_MATCH_WIN;
}

static void draw_tournament_round(const char *title,const int *a,const int *b,int pairs,int round_no){
    uint32_t prev=sync_buttons();for(;;){SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;if(pr&SCE_CTRL_CIRCLE)return;if(pr&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE|SCE_CTRL_START)){ui_ok();return;}
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);vita2d_draw_rectangle(40,40,880,465,RGBA8(3,24,80,240));
        if(lf2_ui_font_ready()){lf2_ui_draw_text(70,60,1.0f,RGBA8(255,255,255,255),title);lf2_ui_draw_textf(695,64,0.70f,RGBA8(255,224,110,255),"Round %d",round_no);
            int columns=pairs>4?2:1;int percol=(pairs+columns-1)/columns;for(int i=0;i<pairs;i++){int c=i/percol,r=i%percol;float x=75+c*425,y=125+r*72;vita2d_draw_rectangle(x,y-8,365,55,RGBA8(7,39,111,220));lf2_ui_draw_textf(x+14,y,0.68f,RGBA8(255,255,255,255),"%s",lf2_roster[a[i]].name);lf2_ui_draw_text(x+170,y,0.68f,RGBA8(165,205,255,255),"VS");lf2_ui_draw_textf(x+220,y,0.68f,RGBA8(255,255,255,255),"%s",lf2_roster[b[i]].name);}
            lf2_ui_draw_text(305,472,0.68f,RGBA8(220,235,255,255),"X/Square/START: continue");}
        vita2d_end_drawing();vita2d_swap_buffers();}
}

static int tournament_1v1(bool cheat){
    (void)cheat;int player=0;if(!choose_single_fighter(false,"1 on 1 Championship",&player))return 0;int diff=choose_difficulty("1 on 1 Championship",false,LF2_DIFF_NORMAL);if(diff<0)return 0;
    int current[16],count=16;current[0]=player;for(int i=1;i<16;i++){int r=rand()%9;if(r>=player)r++;current[i]=r;}
    vita2d_texture *bg=load_stage_texture(3);if(!bg)return -1;lf2_log_stage("championship1:start","player=%s difficulty=%s",lf2_roster[player].name,ui_diff_name(diff));
    for(int round=1;count>1;round++){
        int pairs=count/2,aa[8],bb[8];for(int i=0;i<pairs;i++){aa[i]=current[i*2];bb[i]=current[i*2+1];}draw_tournament_round("1 on 1 Championship",aa,bb,pairs,round);
        int next[16]={0},player_alive=0;
        for(int i=0;i<pairs;i++){
            int a=aa[i],b=bb[i];if(a==player||b==player){int opp=(a==player)?b:a;int cpu[1]={opp},teams[1]={0};char label[64];snprintf(label,sizeof(label),"Championship Round %d",round);lf2_match_options_t opt={0};opt.allow_rematch=false;opt.hud_label=label;
                int rc=lf2_run_match_ex(g_font,bg,player,0,cpu,teams,1,diff,false,&opt);if(rc!=LF2_MATCH_WIN){free_stage_texture(bg);mode_card("ELIMINATED","1 on 1 Championship",1200);return rc;}next[i]=player;player_alive=1;lf2_logf("MODE","1on1 round=%d player beat %s",round,lf2_roster[opp].name);
            }else next[i]=(rand()&1)?a:b;
        }
        if(!player_alive){free_stage_texture(bg);return LF2_MATCH_LOSS;}memcpy(current,next,(size_t)pairs*sizeof(int));count=pairs;
    }
    free_stage_texture(bg);mode_card("CHAMPION!",lf2_roster[player].name,1800);lf2_log_stage("championship1:complete","player=%s",lf2_roster[player].name);return LF2_MATCH_WIN;
}

typedef struct {int a,b;int human;} champ_team_t;
static void draw_team_tournament(const champ_team_t *teams,int count,int round){
    uint32_t prev=sync_buttons();for(;;){SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;if(pr&SCE_CTRL_CIRCLE)return;if(pr&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE|SCE_CTRL_START)){ui_ok();return;}
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);vita2d_draw_rectangle(55,45,850,450,RGBA8(3,24,80,240));if(lf2_ui_font_ready()){lf2_ui_draw_text(82,65,1.0f,RGBA8(255,255,255,255),"2 on 2 Championship");lf2_ui_draw_textf(735,69,0.70f,RGBA8(255,224,110,255),"Round %d",round);int cols=count>4?2:1,per=(count+cols-1)/cols;for(int i=0;i<count;i++){int c=i/per,r=i%per;float x=85+c*410,y=125+r*72;vita2d_draw_rectangle(x,y-7,380,52,teams[i].human?RGBA8(70,116,202,235):RGBA8(7,39,111,220));lf2_ui_draw_textf(x+12,y,0.64f,RGBA8(255,255,255,255),"%s + %s",lf2_roster[teams[i].a].name,lf2_roster[teams[i].b].name);}lf2_ui_draw_text(300,460,0.68f,RGBA8(220,235,255,255),"X/Square/START: continue");}vita2d_end_drawing();vita2d_swap_buffers();}
}

static int tournament_2v2(bool cheat){
    (void)cheat;int player=0,partner=1;if(!choose_single_fighter(false,"2 on 2 Championship - Player",&player))return 0;if(!choose_single_fighter(false,"2 on 2 Championship - Partner",&partner))return 0;int diff=choose_difficulty("2 on 2 Championship",false,LF2_DIFF_NORMAL);if(diff<0)return 0;
    champ_team_t cur[8];cur[0]=(champ_team_t){player,partner,1};for(int i=1;i<8;i++)cur[i]=(champ_team_t){rand()%10,rand()%10,0};int count=8;vita2d_texture *bg=load_stage_texture(3);if(!bg)return -1;lf2_log_stage("championship2:start","player=%s partner=%s difficulty=%s",lf2_roster[player].name,lf2_roster[partner].name,ui_diff_name(diff));
    for(int round=1;count>1;round++){
        draw_team_tournament(cur,count,round);champ_team_t next[8];int ni=0;
        for(int i=0;i<count;i+=2){champ_team_t a=cur[i],b=cur[i+1];if(a.human||b.human){champ_team_t human=a.human?a:b,opp=a.human?b:a;int cpu[3]={human.b,opp.a,opp.b};int teams[3]={1,2,2};char label[64];snprintf(label,sizeof(label),"2 on 2 Championship Round %d",round);lf2_match_options_t opt={0};opt.allow_rematch=false;opt.hud_label=label;int rc=lf2_run_match_ex(g_font,bg,human.a,1,cpu,teams,3,diff,false,&opt);if(rc!=LF2_MATCH_WIN){free_stage_texture(bg);mode_card("ELIMINATED","2 on 2 Championship",1200);return rc;}next[ni++]=human;}
            else next[ni++]=(rand()&1)?a:b;}
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
