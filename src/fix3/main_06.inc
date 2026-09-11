                                const lf2_stage_t *st,const char *label,
                                int ally_count,const int *allies,int *ally_hp,int *ally_mp,
                                int *carry_hp,int *carry_mp,unsigned *rng){
    if(!st||!carry_hp||!carry_mp||!rng)return -1;
    lf2_stage_runtime_t rt;memset(&rt,0,sizeof(rt));
    rt.phase_count=st->phase_count;
    if(rt.phase_count>LF2_STAGE_RUNTIME_MAX_PHASES)rt.phase_count=LF2_STAGE_RUNTIME_MAX_PHASES;
    rt.player_hp=*carry_hp;rt.player_mp=*carry_mp;rt.player_hp_out=carry_hp;rt.player_mp_out=carry_mp;rt.hud_prefix=label;
    rt.ally_count=ally_count;rt.ally_hp_out=ally_hp;rt.ally_mp_out=ally_mp;
    for(int i=0;i<ally_count&&i<LF2_STAGE_MAX_ALLIES;i++){rt.ally_roster[i]=allies[i];rt.ally_hp[i]=ally_hp[i];rt.ally_mp[i]=ally_mp[i];}
    for(int ph=0;ph<rt.phase_count;ph++){
        lf2_stage_runtime_phase_t *dst=&rt.phases[ph];dst->bound=st->phases[ph].bound;
        dst->enemy_count=lf2_stage_build_phase(&st->phases[ph],difficulty,dst->enemies,dst->hp,LF2_STAGE_RUNTIME_MAX_ENEMIES,rng);
        dst->item_count=lf2_stage_build_items(&st->phases[ph],difficulty,dst->item_oid,dst->item_x,LF2_STAGE_RUNTIME_MAX_ITEMS,rng);
        lf2_log_stage("stage:phase_plan","phase=%d/%d bound=%d enemies=%d items=%d",ph+1,rt.phase_count,dst->bound,dst->enemy_count,dst->item_count);
    }
    return lf2_run_stage_section(g_font,stage_tex,fighter,difficulty,cheat,&rt);
}

static int run_stage_major_continuous(vita2d_texture *stage_tex,int fighter,int difficulty,bool cheat,
                                      int major,int ally_count,const int *allies,int *ally_hp,int *ally_mp,
                                      int *carry_hp,int *carry_mp,unsigned *rng){
    if(!carry_hp||!carry_mp||!rng||major<0||major>=5)return -1;
    lf2_stage_runtime_t rt;memset(&rt,0,sizeof(rt));
    rt.player_hp=*carry_hp;rt.player_mp=*carry_mp;rt.player_hp_out=carry_hp;rt.player_mp_out=carry_mp;
    rt.ally_count=ally_count;rt.ally_hp_out=ally_hp;rt.ally_mp_out=ally_mp;
    for(int i=0;i<ally_count&&i<LF2_STAGE_MAX_ALLIES;i++){rt.ally_roster[i]=allies[i];rt.ally_hp[i]=ally_hp[i];rt.ally_mp[i]=ally_mp[i];}
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

static void stage_summary_screen(int major,const lf2_stage_summary_t *s,uint64_t elapsed_us){
    if(!s)return;
    unsigned total_sec=(unsigned)(elapsed_us/1000000ULL),mins=total_sec/60,secs=total_sec%60;
    lf2_log_stage("stage:summary","stage=%d players=%d time=%u:%02u",major+1,s->participant_count,mins,secs);
    for(int i=0;i<s->participant_count&&i<LF2_STAGE_SUMMARY_MAX_PLAYERS;i++){
        int r=s->roster[i];const char *name=(r>=0&&r<lf2_roster_count)?lf2_roster[r].name:"?";
        lf2_logf("STAGE","summary slot=%d player=%s kill=%d attack=%d hp_lost=%d mp_usage=%d picking=%d hp=%d mp=%d",
                 i,name,s->kills[i],s->attack[i],s->hp_lost[i],s->mp_usage[i],s->picking[i],s->hp[i],s->mp[i]);
    }
    uint32_t prev=sync_buttons();
    for(;;){
        SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;
        if(pr&(SCE_CTRL_SQUARE|SCE_CTRL_CROSS)){ui_ok();return;}
        vita2d_start_drawing();vita2d_clear_screen();
        vita2d_draw_rectangle(0,0,SCREEN_W,SCREEN_H,RGBA8(7,35,103,255));
        vita2d_draw_rectangle(24,28,912,462,RGBA8(8,46,126,245));
        vita2d_draw_rectangle(37,104,886,42,RGBA8(16,67,151,245));
        if(lf2_ui_font_ready()){
            lf2_ui_draw_textf(48,48,0.94f,RGBA8(255,255,255,255),"SUMMARY - STAGE %d",major+1);
            lf2_ui_draw_textf(760,54,0.58f,RGBA8(220,235,255,255),"Time %u:%02u",mins,secs);
            lf2_ui_draw_text(51,116,0.48f,RGBA8(210,228,255,255),"Player");
            lf2_ui_draw_text(327,116,0.44f,RGBA8(210,228,255,255),"Kill");
            lf2_ui_draw_text(389,116,0.44f,RGBA8(210,228,255,255),"Attack");
            lf2_ui_draw_text(475,116,0.44f,RGBA8(210,228,255,255),"HP Lost");
            lf2_ui_draw_text(574,116,0.44f,RGBA8(210,228,255,255),"MP Usage");
            lf2_ui_draw_text(688,116,0.44f,RGBA8(210,228,255,255),"Picking");
            lf2_ui_draw_text(786,116,0.44f,RGBA8(210,228,255,255),"Status");
            int n=s->participant_count;if(n>LF2_STAGE_SUMMARY_MAX_PLAYERS)n=LF2_STAGE_SUMMARY_MAX_PLAYERS;
            for(int i=0;i<n;i++){
                float y=158.0f+i*38.0f;int r=s->roster[i];const char *name=(r>=0&&r<lf2_roster_count)?lf2_roster[r].name:"?";
                vita2d_draw_rectangle(41,y-7,875,34,(i&1)?RGBA8(8,41,111,205):RGBA8(12,53,132,220));
                if(r>=0&&r<lf2_roster_count)draw_portrait_index(r,48,y-4,0.25f);
                lf2_ui_draw_textf(86,y,0.52f,RGBA8(255,255,255,255),"%s%s",i==0?"P1 ":"COM ",name);
                lf2_ui_draw_textf(334,y,0.50f,RGBA8(255,255,255,255),"%d",s->kills[i]);
                lf2_ui_draw_textf(398,y,0.50f,RGBA8(255,255,255,255),"%d",s->attack[i]);
                lf2_ui_draw_textf(493,y,0.50f,RGBA8(255,255,255,255),"%d",s->hp_lost[i]);
                lf2_ui_draw_textf(598,y,0.50f,RGBA8(255,255,255,255),"%d",s->mp_usage[i]);
                lf2_ui_draw_textf(711,y,0.50f,RGBA8(255,255,255,255),"%d",s->picking[i]);
                if(s->hp[i]>0)lf2_ui_draw_textf(786,y,0.47f,RGBA8(135,235,155,255),"OK %d HP",s->hp[i]);
                else lf2_ui_draw_text(786,y,0.47f,RGBA8(255,145,145,255),"KO");
            }
            lf2_ui_draw_text(277,505,0.54f,RGBA8(230,240,255,255),"Square (Attack) / X (Jump): continue");
        }
        vita2d_end_drawing();vita2d_swap_buffers();
    }
}

static int run_stage(bool cheat){
    int fighter=0,choice=0,difficulty=LF2_DIFF_NORMAL,ally_count=0;
    int allies[LF2_STAGE_MAX_ALLIES]={1,2,3,4,5,6,7};
    int ally_hp[LF2_STAGE_MAX_ALLIES]={500,500,500,500,500,500,500};
    int ally_mp[LF2_STAGE_MAX_ALLIES]={500,500,500,500,500,500,500};
    if(!choose_stage_party(cheat,&fighter,&ally_count,allies))return 0;
    if(!choose_stage_start(cheat,&choice,&difficulty))return 0;
    unsigned rng=(unsigned)sceKernelGetSystemTimeWide()^0x53544147u;int p_hp=500,p_mp=500;
    lf2_log_stage("stage_mode:start","fighter=%s allies=%d choice=%d difficulty=%s",lf2_roster[fighter].name,ally_count,choice,ui_diff_name(difficulty));

    if(choice==5){
        lf2_stage_t st;if(lf2_stage_load(50,&st)<0){wait_message("Stage Mode Error","data/stage.dat: Survival Stage konnte nicht gelesen werden.");return -1;}
        vita2d_texture *bg=load_stage_texture(1);if(!bg)return -1;
        int rc=run_stage_continuous(bg,fighter,difficulty,cheat,&st,"Survival Stage",ally_count,allies,ally_hp,ally_mp,&p_hp,&p_mp,&rng);
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
        uint64_t stage_started=sceKernelGetSystemTimeWide();
        int rc=run_stage_major_continuous(bg,fighter,difficulty,cheat,major,ally_count,allies,ally_hp,ally_mp,&p_hp,&p_mp,&rng);
        uint64_t stage_elapsed=sceKernelGetSystemTimeWide()-stage_started;
        free_stage_texture(bg);
        if(rc!=LF2_MATCH_WIN){mode_card("GAME OVER","Stage Mode",1200);return rc;}
        lf2_stage_summary_t summary;memset(&summary,0,sizeof(summary));lf2_get_last_stage_summary(&summary);
        stage_summary_screen(major,&summary,stage_elapsed);
        int heal=(difficulty==LF2_DIFF_EASY)?200:(difficulty==LF2_DIFF_NORMAL?150:(difficulty==LF2_DIFF_DIFFICULT?100:50));
        p_hp+=heal;if(p_hp>500)p_hp=500;p_mp=500;
