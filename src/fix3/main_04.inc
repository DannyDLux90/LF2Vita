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
