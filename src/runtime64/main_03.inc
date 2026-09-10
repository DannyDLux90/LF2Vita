        if(pr&SCE_CTRL_UP){team=(team+4)%5;ui_join();}
        if(pr&SCE_CTRL_DOWN){team=(team+1)%5;ui_join();}
        if(pr&SCE_CTRL_CIRCLE){ui_cancel();return 0;}
        if(pr&(SCE_CTRL_SQUARE|SCE_CTRL_CROSS)){
            *roster_out=vis[vpos];*team_out=team;ui_ok();
            lf2_log_stage("character_select:slot_done","slot=%d fighter=%s team=%s",slot,lf2_roster[*roster_out].name,ui_team_name(team));return 1;
        }
        draw_character_selection_base(setup,slot,vis[vpos],team,cheat);
        vita2d_draw_rectangle(110,505,740,34,RGBA8(0,5,28,235));
        if(lf2_ui_font_ready())lf2_ui_draw_text(130,510,0.68f,RGBA8(255,255,255,255),"Left/Right Fighter   Up/Down Team   X/Square OK   Circle Back");
        vita2d_end_drawing();vita2d_swap_buffers();
    }
}

static int select_cpu_count_original(vs_setup_t *setup,bool cheat){
    int count=setup->cpu_count;uint32_t prev=sync_buttons();lf2_log_stage("character_select:com_count","open=1");
    for(;;){
        SceCtrlData pad={0};sceCtrlPeekBufferPositive(0,&pad,1);uint32_t pr=pad.buttons&~prev;prev=pad.buttons;
        if(pr&(SCE_CTRL_LEFT|SCE_CTRL_DOWN)){count=count>1?count-1:LF2_MAX_CPUS;ui_join();}
        if(pr&(SCE_CTRL_RIGHT|SCE_CTRL_UP)){count=count<LF2_MAX_CPUS?count+1:1;ui_join();}
        if(pr&SCE_CTRL_CIRCLE){ui_cancel();return 0;}
        if(pr&(SCE_CTRL_SQUARE|SCE_CTRL_CROSS)){setup->cpu_count=count;ui_ok();lf2_log_stage("character_select:com_count","count=%d",count);return 1;}
        vs_setup_t preview=*setup;preview.cpu_count=count;
        for(int i=0;i<count;i++)if(preview.cpus[i]<0)preview.cpus[i]=0;
        draw_character_selection_base(&preview,-1,-1,0,cheat);
        vita2d_draw_rectangle(18,17,275,165,RGBA8(25,67,145,242));
        if(g_font){
            vita2d_pgf_draw_text(g_font,36,48,RGBA8(255,255,255,255),0.80f,"COM Players");
            vita2d_pgf_draw_textf(g_font,43,105,RGBA8(255,225,90,255),1.35f,"%d",count);
            vita2d_pgf_draw_text(g_font,90,104,RGBA8(235,242,255,255),0.66f,"Computer");
            vita2d_pgf_draw_text(g_font,36,150,RGBA8(215,230,255,255),0.63f,"←/→ Anzahl    □/× OK");
        }
        vita2d_end_drawing();vita2d_swap_buffers();
    }
}

static const char *stage_option_name(int stage){return stage<0?"Random":g_stages[stage].name;}

static int vs_options(vs_setup_t *setup,bool cheat){
    static const char *labels[]={"Fight!","Reset All","Random","Background","Difficulty","Exit"};
    int sel=0;uint32_t prev=sync_buttons();
    for(;;){
        SceCtrlData pad={0};sceCtrlPeekBufferPositive(0,&pad,1);uint32_t pr=pad.buttons&~prev;prev=pad.buttons;
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
