        if(pr&SCE_CTRL_UP){sel=(sel+5)%6;ui_join();}if(pr&SCE_CTRL_DOWN){sel=(sel+1)%6;ui_join();}
        int adjust=(pr&SCE_CTRL_RIGHT)?1:((pr&SCE_CTRL_LEFT)?-1:0);
        if(adjust&&sel==3){setup->stage+=adjust;if(setup->stage>=STAGE_COUNT)setup->stage=-1;if(setup->stage<-1)setup->stage=STAGE_COUNT-1;ui_join();}
        if(adjust&&sel==4){int max=cheat?LF2_DIFF_CRAZY:LF2_DIFF_DIFFICULT;setup->difficulty+=adjust;if(setup->difficulty>max)setup->difficulty=LF2_DIFF_EASY;if(setup->difficulty<LF2_DIFF_EASY)setup->difficulty=max;ui_join();}
        if(pr&SCE_CTRL_CIRCLE){ui_cancel();return 0;}
        if(pr&(SCE_CTRL_SQUARE|SCE_CTRL_CROSS)){
            if(sel==0){ui_ok();return 1;}
            if(sel==1){ui_cancel();return 2;}
            if(sel==2){int vis[32],n=build_visible_indices(cheat,vis,32);setup->p1=vis[rand()%n];setup->p1_team=0;for(int i=0;i<setup->cpu_count;i++){setup->cpus[i]=vis[rand()%n];setup->cpu_teams[i]=0;}ui_join();}
            if(sel==3){setup->stage++;if(setup->stage>=STAGE_COUNT)setup->stage=-1;ui_join();}
            if(sel==4){int max=cheat?LF2_DIFF_CRAZY:LF2_DIFF_DIFFICULT;setup->difficulty=(setup->difficulty+1)%(max+1);ui_join();}
            if(sel==5){ui_cancel();return 0;}
        }
        draw_character_selection_base(setup,-1,-1,0,cheat);
        vita2d_draw_rectangle(17,15,330,205,RGBA8(36,79,159,247));
        if(lf2_ui_font_ready()){
            for(int i=0;i<6;i++){
                float y=31.0f+i*29.0f;if(i==sel)vita2d_draw_rectangle(27,y-2,308,27,RGBA8(114,153,225,245));
                if(i==3)lf2_ui_draw_textf(38,y,0.68f,RGBA8(255,255,255,255),"Background: %s",stage_option_name(setup->stage));
                else if(i==4)lf2_ui_draw_textf(38,y,0.68f,RGBA8(255,255,255,255),"Difficulty: %s",ui_diff_name(setup->difficulty));
                else lf2_ui_draw_text(38,y,0.68f,RGBA8(255,255,255,255),labels[i]);
            }
            lf2_ui_draw_text(365,510,0.68f,RGBA8(255,255,255,255),"Up/Down Menu   Left/Right Value   X/Square OK   Circle Back");
        }
        vita2d_end_drawing();vita2d_swap_buffers();
    }
}

static int run_vs(bool cheat){
    vs_setup_t setup;setup_defaults(&setup);srand((unsigned)sceKernelGetSystemTimeWide());
    for(;;){
        lf2_log_stage("character_select:open","style=classic cheat=%d",cheat?1:0);
        if(!select_slot(cheat,&setup,0,&setup.p1,&setup.p1_team))return 0;
        if(!select_cpu_count_original(&setup,cheat))continue;
        int canceled=0;

        for(int i=0;i<setup.cpu_count;i++){
            if(!select_slot(cheat,&setup,i+1,&setup.cpus[i],&setup.cpu_teams[i])){canceled=1;break;}
        }
        if(canceled)continue;
        int opt=vs_options(&setup,cheat);if(opt==0)return 0;if(opt==2){setup_defaults(&setup);continue;}
        /* Guarantee the human has at least one hostile fighter. */
        int hostile=0;for(int i=0;i<setup.cpu_count;i++)if(setup.p1_team==0||setup.cpu_teams[i]==0||setup.p1_team!=setup.cpu_teams[i]){hostile=1;break;}
        if(!hostile)setup.cpu_teams[0]=0;
        int stage=setup.stage<0?(rand()%STAGE_COUNT):setup.stage;
        vita2d_texture *stage_tex=vita2d_load_PNG_file(g_stages[stage].asset);
        if(!stage_tex){wait_message("Background Error","Die gewählte Karte konnte nicht geladen werden.");continue;}
        lf2_log_stage("vs:start","p1=%s team=%s cpus=%d stage=%s difficulty=%s",lf2_roster[setup.p1].name,ui_team_name(setup.p1_team),setup.cpu_count,g_stages[stage].name,ui_diff_name(setup.difficulty));
        int rc=lf2_run_match(g_font,stage_tex,setup.p1,setup.p1_team,setup.cpus,setup.cpu_teams,setup.cpu_count,setup.difficulty,cheat);
        lf2_logf("INFO","lf2_run_match returned %d",rc);vita2d_wait_rendering_done();vita2d_free_texture(stage_tex);lf2_log_memory("stage texture freed");
        if(rc<0)wait_message("VS Mode Fehler","Der Kampf konnte nicht geladen werden. Bitte lf2.log und last_state.txt sichern.");
        return rc;
    }
}


/* -------------------------------------------------------------------------
 * Native game modes (v0.63)
 * ------------------------------------------------------------------------- */

static vita2d_texture *load_stage_texture(int stage_index){
    if(stage_index<0||stage_index>=STAGE_COUNT)return NULL;
    vita2d_texture *tex=vita2d_load_PNG_file(g_stages[stage_index].asset);
    if(!tex)lf2_logf("ERROR","background load failed idx=%d asset=%s",stage_index,g_stages[stage_index].asset);
    return tex;
}

static void free_stage_texture(vita2d_texture *tex){
    if(!tex)return;vita2d_wait_rendering_done();vita2d_free_texture(tex);
}

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

static int run_stage_continuous(vita2d_texture *stage_tex,int fighter,int difficulty,bool cheat,
                                const lf2_stage_t *st,const char *label,
                                int *carry_hp,int *carry_mp,unsigned *rng){
    if(!st||!carry_hp||!carry_mp||!rng)return -1;
    lf2_stage_runtime_t rt;memset(&rt,0,sizeof(rt));
    rt.phase_count=st->phase_count;
    if(rt.phase_count>LF2_STAGE_RUNTIME_MAX_PHASES)rt.phase_count=LF2_STAGE_RUNTIME_MAX_PHASES;
    rt.player_hp=*carry_hp;rt.player_mp=*carry_mp;rt.player_hp_out=carry_hp;rt.player_mp_out=carry_mp;rt.hud_prefix=label;
    for(int ph=0;ph<rt.phase_count;ph++){
        lf2_stage_runtime_phase_t *dst=&rt.phases[ph];dst->bound=st->phases[ph].bound;
        dst->enemy_count=lf2_stage_build_phase(&st->phases[ph],difficulty,dst->enemies,dst->hp,LF2_STAGE_RUNTIME_MAX_ENEMIES,rng);
        dst->item_count=lf2_stage_build_items(&st->phases[ph],difficulty,dst->item_oid,dst->item_x,LF2_STAGE_RUNTIME_MAX_ITEMS,rng);
        lf2_log_stage("stage:phase_plan","phase=%d/%d bound=%d enemies=%d items=%d",ph+1,rt.phase_count,dst->bound,dst->enemy_count,dst->item_count);
    }
    return lf2_run_stage_section(g_font,stage_tex,fighter,difficulty,cheat,&rt);
}

static int run_stage_major_continuous(vita2d_texture *stage_tex,int fighter,int difficulty,bool cheat,
                                      int major,int *carry_hp,int *carry_mp,unsigned *rng){
    if(!carry_hp||!carry_mp||!rng||major<0||major>=5)return -1;
    lf2_stage_runtime_t rt;memset(&rt,0,sizeof(rt));
    rt.player_hp=*carry_hp;rt.player_mp=*carry_mp;rt.player_hp_out=carry_hp;rt.player_mp_out=carry_mp;
    static char label[24];snprintf(label,sizeof(label),"Stage %d",major+1);rt.hud_prefix=label;
    int last_bound=0;
    for(int sub=0;sub<5;sub++){
        int stage_id=major*10+sub;lf2_stage_t st;
        if(lf2_stage_load(stage_id,&st)<0)return -2;
        if(st.phase_count<1)continue;
        int first=st.phases[0].bound;
        /* stage.dat restarts local bounds for each x-y section. Keep the
           authored distances inside a section, but place the next section a
           short screen-distance after the previous one so the camera/world
           never gets torn down between 1-1, 1-2, ... */
        int offset=(sub==0)?0:(last_bound+360-first);
        lf2_log_stage("stage:section_plan","stage=%d-%d phases=%d offset=%d first=%d",
                      major+1,sub+1,st.phase_count,offset,first+offset);
        for(int ph=0;ph<st.phase_count&&rt.phase_count<LF2_STAGE_RUNTIME_MAX_PHASES;ph++){
            lf2_stage_runtime_phase_t *dst=&rt.phases[rt.phase_count++];
            dst->bound=st.phases[ph].bound+offset;
            if(dst->bound<=last_bound)dst->bound=last_bound+220;
            dst->enemy_count=lf2_stage_build_phase(&st.phases[ph],difficulty,dst->enemies,dst->hp,
                                                    LF2_STAGE_RUNTIME_MAX_ENEMIES,rng);
            dst->item_count=lf2_stage_build_items(&st.phases[ph],difficulty,dst->item_oid,dst->item_x,
                                                  LF2_STAGE_RUNTIME_MAX_ITEMS,rng);
            last_bound=dst->bound;
            lf2_log_stage("stage:phase_plan","stage=%d-%d phase=%d bound=%d enemies=%d items=%d",
                          major+1,sub+1,ph+1,dst->bound,dst->enemy_count,dst->item_count);
        }
    }
    if(rt.phase_count<1)return -3;
    return lf2_run_stage_section(g_font,stage_tex,fighter,difficulty,cheat,&rt);
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
        int rc=run_stage_continuous(bg,fighter,difficulty,cheat,&st,"Survival Stage",&p_hp,&p_mp,&rng);
        free_stage_texture(bg);
        if(rc!=LF2_MATCH_WIN){mode_card("GAME OVER","Survival Stage ended",1200);return rc;}
        mode_card("SURVIVAL CLEAR","All authored survival phases cleared",1600);return LF2_MATCH_WIN;
    }

    for(int major=choice;major<5;major++){
        vita2d_texture *bg=load_stage_texture(stage_background_for_major(major));if(!bg)return -1;
        /* v0.69: all five x-y sections of one major stage are concatenated
           into a single persistent world. No Stage/KO splash is shown between
           waves or sub-sections; after a short delay the next authored group
           simply appears farther to the right, like the PC game. */
        int rc=run_stage_major_continuous(bg,fighter,difficulty,cheat,major,&p_hp,&p_mp,&rng);
        free_stage_texture(bg);
        if(rc!=LF2_MATCH_WIN){mode_card("GAME OVER","Stage Mode",1200);return rc;}
        int heal=(difficulty==LF2_DIFF_EASY)?200:(difficulty==LF2_DIFF_NORMAL?150:(difficulty==LF2_DIFF_DIFFICULT?100:50));
        p_hp+=heal;if(p_hp>500)p_hp=500;p_mp=500;
    }
    mode_card("CONGRATULATIONS!","Stage Mode cleared",1800);
    lf2_log_stage("stage_mode:complete","fighter=%s",lf2_roster[fighter].name);
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
