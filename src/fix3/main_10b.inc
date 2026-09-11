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
    lf2_log_stage("shutdown:resources","");adhoc_system_link_shutdown();lf2_adhoc_shutdown();lf2_audio_shutdown();lf2_pak_shutdown();free_ui_assets();lf2_log_shutdown(1);sceKernelExitProcess(0);return 0;
}
