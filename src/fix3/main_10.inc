            vita2d_pgf_draw_text(g_font,360,297,RGBA8(255,255,255,255),0.66f,"No");vita2d_pgf_draw_text(g_font,545,297,RGBA8(255,255,255,255),0.66f,"Yes");}
        vita2d_end_drawing();vita2d_swap_buffers();}
}

static void main_menu(bool cheat){
    /* Keep the order and naming of LF2 2.0a's Game Mode menu. */
    const char *items[]={"VS mode","Stage mode","1 on 1 Championship","2 on 2 Championship","Battle mode","Demo","Network Play","Playback Recording","Quit"};
    const int count=9;int sel=0;uint32_t prev=sync_buttons();char note[160]={0};int note_ticks=0;lf2_audio_music_start();lf2_log_stage("menu:open","cheat=%d style=classic music=%d",cheat?1:0,lf2_audio_music_ready());
    for(;;){
        SceCtrlData pad={0};sceCtrlPeekBufferPositive(0,&pad,1);uint32_t pr=pad.buttons&~prev;prev=pad.buttons;
        if(pr&SCE_CTRL_UP){sel=(sel+count-1)%count;ui_join();}if(pr&SCE_CTRL_DOWN){sel=(sel+1)%count;ui_join();}
        if(pr&SCE_CTRL_SELECT){manual_view();prev=sync_buttons();}
        if(pr&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE)){
            lf2_logf("UI","menu select index=%d item=%s",sel,items[sel]);ui_ok();
            if(sel==0){run_vs(cheat);lf2_audio_music_start();prev=sync_buttons();}
            else if(sel==1){run_stage(cheat);lf2_audio_music_start();prev=sync_buttons();}
            else if(sel==2){tournament_1v1(cheat);lf2_audio_music_start();prev=sync_buttons();}
            else if(sel==3){tournament_2v2(cheat);lf2_audio_music_start();prev=sync_buttons();}
            else if(sel==4){run_battle(cheat);lf2_audio_music_start();prev=sync_buttons();}
            else if(sel==5){run_demo();lf2_audio_music_start();prev=sync_buttons();}
            else if(sel==6){network_play_menu();prev=sync_buttons();}
            else if(sel==7){playback_recording();prev=sync_buttons();}
            else if(sel==8){if(confirm_quit())break;prev=sync_buttons();}
        }
        if(note_ticks>0)note_ticks--;
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);
        vita2d_draw_rectangle(0,0,960,544,RGBA8(10,38,105,135));
        vita2d_draw_rectangle(40,160,500,350,RGBA8(9,37,102,218));
        if(lf2_ui_font_ready()){
            /* v0.61: critical menu text uses a pre-rasterized, outlined bitmap
               atlas instead of tiny fractional PGF glyphs. This stays sharp on
               the Vita LCD/OLED and is readable from normal handheld distance. */
            lf2_ui_draw_text(48,72,1.35f,RGBA8(255,255,255,255),"Little Fighter 2");
            lf2_ui_draw_text(50,122,0.78f,RGBA8(220,235,255,255),"Version 2.00a   PS Vita 0.69 fix3");
            for(int i=0;i<count;i++){
                float y=174.0f+i*36.0f;
                if(i==sel)vita2d_draw_rectangle(70,y-5,455,33,RGBA8(126,166,235,245));
                else vita2d_draw_rectangle(70,y-5,455,33,RGBA8(7,29,82,145));
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
    lf2_audio_music_stop();lf2_log_stage("menu:closed","");
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
