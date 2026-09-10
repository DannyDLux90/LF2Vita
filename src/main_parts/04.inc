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
            else if(sel==1){lf2_log_stage("stage_mode:compat","classic UI; native stage progression pending");run_vs(cheat);prev=sync_buttons();}
            else if(sel==7){if(confirm_quit())break;prev=sync_buttons();}
            else{snprintf(note,sizeof(note),"%s: Gameplay-Regeln werden weiter nativ portiert.",items[sel]);note_ticks=180;}
        }
        if(note_ticks>0)note_ticks--;
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);
        vita2d_draw_rectangle(0,0,960,544,RGBA8(10,38,105,135));
        vita2d_draw_rectangle(40,175,500,325,RGBA8(9,37,102,218));
        if(g_font){
            vita2d_pgf_draw_text(g_font,48,112,RGBA8(255,255,255,255),1.48f,"Little Fighter 2");
            vita2d_pgf_draw_text(g_font,327,139,RGBA8(215,229,250,255),0.66f,"Ver 2.0a • PS Vita v0.60");
            for(int i=0;i<count;i++){
                float y=214.0f+i*34.0f;if(i==sel)vita2d_draw_rectangle(92,y-23,405,28,RGBA8(116,151,218,235));
                unsigned c=i==sel?RGBA8(255,255,255,255):RGBA8(153,180,230,255);
                vita2d_pgf_draw_text(g_font,112,(int)y,c,0.82f,items[i]);
            }
            if(cheat)vita2d_pgf_draw_text(g_font,610,455,RGBA8(255,225,80,255),0.72f,"LF2.NET • CRAZY! unlocked");
            vita2d_pgf_draw_text(g_font,610,500,RGBA8(190,215,245,255),0.65f,"↑/↓ wählen   □/× bestätigen");
            vita2d_pgf_draw_text(g_font,610,522,RGBA8(150,195,240,255),0.62f,"SELECT: Anleitung");
            if(note_ticks)vita2d_pgf_draw_text(g_font,520,170,RGBA8(255,207,110,255),0.60f,note);
        }
        vita2d_end_drawing();vita2d_swap_buffers();
    }
    lf2_log_stage("menu:closed","");
}

static void load_ui_assets(void){
    lf2_log_stage("graphics:init","");vita2d_init();vita2d_set_clear_color(RGBA8(5,12,28,255));vita2d_set_vblank_wait(1);sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
    g_font=vita2d_load_default_pgf();lf2_logf("INFO","font=%p",g_font);
    g_menu_bg=vita2d_load_PNG_file("app0:/assets/menu_bg.png");g_portraits=vita2d_load_PNG_file("app0:/assets/portraits.png");
    lf2_logf("INFO","UI textures menu=%p portraits=%p",g_menu_bg,g_portraits);lf2_log_memory("base ui loaded");
}
static void free_ui_assets(void){vita2d_wait_rendering_done();if(g_portraits)vita2d_free_texture(g_portraits);if(g_manual_bg)vita2d_free_texture(g_manual_bg);if(g_menu_bg)vita2d_free_texture(g_menu_bg);if(g_check_bg)vita2d_free_texture(g_check_bg);if(g_font)vita2d_free_pgf(g_font);vita2d_fini();}

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
