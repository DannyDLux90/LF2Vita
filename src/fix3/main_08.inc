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
