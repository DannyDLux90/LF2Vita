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
            else if(sel==6){network_play_menu(cheat);prev=sync_buttons();}
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
            lf2_ui_draw_text(50,122,0.78f,RGBA8(220,235,255,255),"Version 2.00a   PS Vita 0.70");
            for(int i=0;i<count;i++){
