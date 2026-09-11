static int adhoc_lobby_screen(bool cheat){
    int setting_sel=0;uint32_t prev=sync_buttons();uint64_t lobby_opened=sceKernelGetSystemTimeWide();
    for(;;){
        lf2_adhoc_lobby_update();const lf2_adhoc_lobby_t *l=lf2_adhoc_lobby();
        SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;
        if(pr&SCE_CTRL_CIRCLE){ui_cancel();lf2_adhoc_leave();return 0;}
        if(!l->connected&&!l->host&&sceKernelGetSystemTimeWide()-lobby_opened>8000000ULL){ui_cancel();wait_message("AdHoc","Verbindung zum Host verloren oder Zeitueberschreitung.");lf2_adhoc_leave();return 0;}
        if(pr&SCE_CTRL_LTRIGGER){int slot=l->host?0:l->local_slot;int c=(slot>=0&&slot<8&&l->players[slot].present)?l->players[slot].character:0;lf2_adhoc_lobby_set_character(net_next_roster(cheat,c,-1));ui_join();}
        if(pr&SCE_CTRL_RTRIGGER){int slot=l->host?0:l->local_slot;int c=(slot>=0&&slot<8&&l->players[slot].present)?l->players[slot].character:0;lf2_adhoc_lobby_set_character(net_next_roster(cheat,c,1));ui_join();}
        if(pr&SCE_CTRL_SQUARE){int slot=l->host?0:l->local_slot;bool r=(slot>=0&&slot<8&&l->players[slot].present)?!l->players[slot].ready:true;lf2_adhoc_lobby_set_ready(r);ui_ok();}
        if(l->host){
            if(pr&SCE_CTRL_UP){setting_sel=(setting_sel+3)%4;ui_join();}
            if(pr&SCE_CTRL_DOWN){setting_sel=(setting_sel+1)%4;ui_join();}
            if(pr&(SCE_CTRL_LEFT|SCE_CTRL_RIGHT)){
                int d=(pr&SCE_CTRL_RIGHT)?1:-1;lf2_net_settings_t st=l->settings;
                if(setting_sel==0)st.mode=(uint8_t)((st.mode+(d>0?1:4))%5);
                else if(setting_sel==1){int v=st.max_players+d;if(v<2)v=8;if(v>8)v=2;st.max_players=(uint8_t)v;}
                else if(setting_sel==2){int v=(int)st.stage+d;if(v<0)v=STAGE_COUNT-1;if(v>=STAGE_COUNT)v=0;st.stage=(uint8_t)v;}
                else {int max=cheat?LF2_DIFF_CRAZY:LF2_DIFF_DIFFICULT;int v=(int)st.difficulty+d;if(v<0)v=max;if(v>max)v=0;st.difficulty=(uint8_t)v;}
                lf2_adhoc_lobby_host_settings(&st);ui_join();
            }
            if(pr&SCE_CTRL_START){
                int present=0,ready=0;for(int i=0;i<8;i++)if(l->players[i].present){present++;if(l->players[i].ready)ready++;}
                if(present>=2&&ready==present){lf2_adhoc_lobby_host_status(LF2_NET_STATUS_STARTING);ui_ok();wait_message("AdHoc-Lobby","Lobby-Synchronisation ist bereit. Der deterministische 30-Hz-Kampf-Lockstep wird als naechster 0.7X-Schritt angeschlossen.");lf2_adhoc_lobby_host_status(LF2_NET_STATUS_WAITING);prev=sync_buttons();}
                else {ui_cancel();}
            }
        }
        l=lf2_adhoc_lobby();
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);vita2d_draw_rectangle(35,34,890,475,RGBA8(3,22,70,244));
        if(lf2_ui_font_ready()){
            lf2_ui_draw_text(58,51,0.96f,RGBA8(255,255,255,255),l->host?"AdHoc Lobby - HOST":"AdHoc Lobby");
            lf2_ui_draw_textf(625,57,0.54f,RGBA8(180,215,255,255),"%s",(!l->host&&!l->connected)?"Verbinde...":lf2_net_status_name(l->settings.status));
            if(!l->host)net_draw_ping(815,48,l->host_ping_ms?l->host_ping_ms:0xffff);
        }
        for(int i=0;i<8;i++){int c=i/4,r=i%4;float x=55+c*290,y=96+r*92;bool local=l->host?(i==0):(i==l->local_slot);net_draw_player_card(x,y,&l->players[i],i,local);}
        vita2d_draw_rectangle(625,92,275,320,RGBA8(7,38,100,225));
        if(lf2_ui_font_ready()){
            lf2_ui_draw_text(648,108,0.70f,RGBA8(255,255,255,255),"Spieleinstellungen");
            const char *labels[]={"Modus","Spieler","Stage","Schwierigkeit"};
            char values[4][48];snprintf(values[0],48,"%s",lf2_net_mode_name(l->settings.mode));snprintf(values[1],48,"%d / %d",({int q=0;for(int i=0;i<8;i++)if(l->players[i].present)q++;q;}),l->settings.max_players);snprintf(values[2],48,"%s",g_stages[l->settings.stage%STAGE_COUNT].name);snprintf(values[3],48,"%s",ui_diff_name(l->settings.difficulty));
            for(int i=0;i<4;i++){float y=158+i*55;if(l->host&&setting_sel==i)vita2d_draw_rectangle(640,y-6,245,39,RGBA8(86,135,215,235));lf2_ui_draw_textf(650,y,0.52f,RGBA8(170,205,245,255),"%s",labels[i]);lf2_ui_draw_textf(742,y,0.56f,RGBA8(255,255,255,255),"%s",values[i]);}
            lf2_ui_draw_text(59,474,0.51f,RGBA8(220,235,255,255),"L/R: Charakter   Quadrat: Bereit   Kreis: Verlassen");
            if(l->host)lf2_ui_draw_text(625,440,0.48f,RGBA8(255,224,125,255),"Host: Up/Down Option, Left/Right aendern");
            if(l->host)lf2_ui_draw_text(625,465,0.48f,RGBA8(205,225,255,255),"START: Lockstep-Status testen");
        }
        vita2d_end_drawing();vita2d_swap_buffers();
    }
}

