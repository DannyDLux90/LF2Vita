        for(int i=0;i<ally_count;i++){ally_hp[i]+=heal;if(ally_hp[i]>500)ally_hp[i]=500;ally_mp[i]=500;}
    }
    mode_card("CONGRATULATIONS!","Stage Mode cleared",1800);
    lf2_log_stage("stage_mode:complete","fighter=%s allies=%d",lf2_roster[fighter].name,ally_count);
    return LF2_MATCH_WIN;
}

static void draw_tournament_round(const char *title,const int *a,const int *b,int pairs,int round_no){
    uint32_t prev=sync_buttons();for(;;){SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;if(pr&SCE_CTRL_CIRCLE)return;if(pr&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE|SCE_CTRL_START)){ui_ok();return;}
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);vita2d_draw_rectangle(40,40,880,465,RGBA8(3,24,80,240));
        if(lf2_ui_font_ready()){lf2_ui_draw_text(70,60,1.0f,RGBA8(255,255,255,255),title);lf2_ui_draw_textf(695,64,0.70f,RGBA8(255,224,110,255),"Round %d",round_no);
            int columns=pairs>4?2:1;int percol=(pairs+columns-1)/columns;for(int i=0;i<pairs;i++){int c=i/percol,r=i%percol;float x=75+c*425,y=125+r*72;vita2d_draw_rectangle(x,y-8,365,55,RGBA8(7,39,111,220));lf2_ui_draw_textf(x+14,y,0.68f,RGBA8(255,255,255,255),"%s",lf2_roster[a[i]].name);lf2_ui_draw_text(x+170,y,0.68f,RGBA8(165,205,255,255),"VS");lf2_ui_draw_textf(x+220,y,0.68f,RGBA8(255,255,255,255),"%s",lf2_roster[b[i]].name);}
            lf2_ui_draw_text(305,472,0.68f,RGBA8(220,235,255,255),"X/Square/START: continue");}
        vita2d_end_drawing();vita2d_swap_buffers();}
}


static void championship_result_1v1(int winner,int defeated,int next_opponent,int round_no){
    uint32_t prev=sync_buttons();lf2_log_stage("championship1:result","round=%d winner=%s defeated=%s next=%s",round_no,lf2_roster[winner].name,defeated>=0?lf2_roster[defeated].name:"-",next_opponent>=0?lf2_roster[next_opponent].name:"CHAMPION");
    for(;;){SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;if(pr&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE|SCE_CTRL_START)){ui_ok();return;}
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);vita2d_draw_rectangle(45,38,870,468,RGBA8(3,24,80,242));
        if(lf2_ui_font_ready()){
            lf2_ui_draw_text(72,58,1.0f,RGBA8(255,255,255,255),"1 on 1 Championship");lf2_ui_draw_textf(738,63,0.70f,RGBA8(255,225,115,255),"Round %d",round_no);
            lf2_ui_draw_text(115,116,0.76f,RGBA8(255,225,115,255),"WINNER");draw_portrait_index(winner,105,155,1.35f);lf2_ui_draw_textf(112,332,0.90f,RGBA8(255,255,255,255),"%s",lf2_roster[winner].name);
            if(defeated>=0){lf2_ui_draw_text(398,164,0.72f,RGBA8(175,205,245,255),"defeated");lf2_ui_draw_textf(398,201,0.76f,RGBA8(225,235,255,255),"%s",lf2_roster[defeated].name);}
            if(next_opponent>=0){lf2_ui_draw_text(618,116,0.76f,RGBA8(125,205,255,255),"NEXT OPPONENT");draw_portrait_index(next_opponent,635,155,1.35f);lf2_ui_draw_textf(640,332,0.90f,RGBA8(255,255,255,255),"%s",lf2_roster[next_opponent].name);lf2_ui_draw_text(453,280,1.05f,RGBA8(255,255,255,255),"VS");}
            else lf2_ui_draw_text(610,220,1.05f,RGBA8(255,225,115,255),"FINAL WON!");
            lf2_ui_draw_text(312,463,0.68f,RGBA8(220,235,255,255),"X / Square / START: continue");
        }vita2d_end_drawing();vita2d_swap_buffers();
    }
}

static int tournament_1v1(bool cheat){
    (void)cheat;int player=0;if(!choose_single_fighter(false,"1 on 1 Championship",&player))return 0;int diff=choose_difficulty("1 on 1 Championship",false,LF2_DIFF_NORMAL);if(diff<0)return 0;
    int current[16],count=16;current[0]=player;for(int i=1;i<16;i++){int r=rand()%9;if(r>=player)r++;current[i]=r;}
    vita2d_texture *bg=load_stage_texture(3);if(!bg)return -1;lf2_log_stage("championship1:start","player=%s difficulty=%s",lf2_roster[player].name,ui_diff_name(diff));
    for(int round=1;count>1;round++){
        int pairs=count/2,aa[8],bb[8];for(int i=0;i<pairs;i++){aa[i]=current[i*2];bb[i]=current[i*2+1];}draw_tournament_round("1 on 1 Championship",aa,bb,pairs,round);
        int next[16]={0},player_alive=0,defeated=-1;
        for(int i=0;i<pairs;i++){
            int a=aa[i],b=bb[i];if(a==player||b==player){int opp=(a==player)?b:a;int cpu[1]={opp},teams[1]={0};char label[64];snprintf(label,sizeof(label),"Championship Round %d",round);lf2_match_options_t opt={0};opt.allow_rematch=false;opt.hud_label=label;
                int rc=lf2_run_match_ex(g_font,bg,player,0,cpu,teams,1,diff,false,&opt);if(rc!=LF2_MATCH_WIN){championship_result_1v1(opp,player,-1,round);free_stage_texture(bg);mode_card("ELIMINATED","1 on 1 Championship",900);return rc;}next[i]=player;defeated=opp;player_alive=1;lf2_logf("MODE","1on1 round=%d player beat %s",round,lf2_roster[opp].name);
            }else next[i]=(rand()&1)?a:b;
        }
        if(!player_alive){free_stage_texture(bg);return LF2_MATCH_LOSS;}
        int pi=-1;for(int i=0;i<pairs;i++)if(next[i]==player){pi=i;break;}int next_opp=-1;if(pi>=0&&pairs>1){int sib=(pi&1)?pi-1:pi+1;if(sib<pairs)next_opp=next[sib];}
        championship_result_1v1(player,defeated,next_opp,round);memcpy(current,next,(size_t)pairs*sizeof(int));count=pairs;
    }
    free_stage_texture(bg);mode_card("CHAMPION!",lf2_roster[player].name,1800);lf2_log_stage("championship1:complete","player=%s",lf2_roster[player].name);return LF2_MATCH_WIN;
}

typedef struct {int a,b;int human;} champ_team_t;
static void championship_result_2v2(champ_team_t winner,champ_team_t defeated,const champ_team_t *next_opponent,int round_no){
    uint32_t prev=sync_buttons();lf2_log_stage("championship2:result","round=%d winner=%s+%s next=%s",round_no,lf2_roster[winner.a].name,lf2_roster[winner.b].name,next_opponent?lf2_roster[next_opponent->a].name:"CHAMPIONS");
    for(;;){SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;if(pr&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE|SCE_CTRL_START)){ui_ok();return;}
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);vita2d_draw_rectangle(38,38,884,468,RGBA8(3,24,80,242));
        if(lf2_ui_font_ready()){
            lf2_ui_draw_text(66,58,1.0f,RGBA8(255,255,255,255),"2 on 2 Championship");lf2_ui_draw_textf(748,63,0.70f,RGBA8(255,225,115,255),"Round %d",round_no);
