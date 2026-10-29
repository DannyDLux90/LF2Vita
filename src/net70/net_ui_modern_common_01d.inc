static int adhoc_lobby_screen(bool cheat){
    int setting_sel=0;uint32_t prev=sync_buttons();uint64_t lobby_opened=sceKernelGetSystemTimeWide();uint32_t last_started_revision=0;
    for(;;){
        lf2_adhoc_lobby_update();const lf2_adhoc_lobby_t *l=lf2_adhoc_lobby();
        SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;
        if(pr&SCE_CTRL_CIRCLE){ui_cancel();lf2_adhoc_leave();return 0;}
        if(!l->connected&&!l->host&&sceKernelGetSystemTimeWide()-lobby_opened>8000000ULL){ui_cancel();wait_message("AdHoc","Verbindung zum Host verloren oder Zeitueberschreitung.");lf2_adhoc_leave();return 0;}
        if(!l->host&&l->connected&&l->settings.status==LF2_NET_STATUS_STARTING&&l->revision!=last_started_revision){
            uint32_t match_id=l->revision;last_started_revision=match_id;
            int active_guest=net70_active_guest_slot(l);
            if(l->local_slot==active_guest){
                int mrc=net70_run_adhoc_mode(match_id);
                if(mrc<0){char e[128];snprintf(e,sizeof(e),"Netzwerkspiel konnte nicht gestartet werden (Fehler %d).",mrc);wait_message("AdHoc-Spiel",e);}
                prev=sync_buttons();continue;
            }
            lf2_logf("NET","reserve slot=%d observes match id=%u active_guest=%d",l->local_slot,(unsigned)match_id,active_guest);
        }
        bool modifier=(p.buttons&SCE_CTRL_TRIANGLE)!=0;
        if(pr&(SCE_CTRL_LTRIGGER|SCE_CTRL_RTRIGGER)){
            int slot=l->host?0:l->local_slot,dir=(pr&SCE_CTRL_RTRIGGER)?1:-1;
            if(modifier&&l->settings.mode==LF2_NET_MODE_2V2){int c=(slot>=0&&slot<8&&l->players[slot].present)?l->players[slot].partner_character:1;lf2_adhoc_lobby_set_partner(net70_next_roster(cheat,c,dir));}
            else if(modifier&&l->settings.mode==LF2_NET_MODE_BATTLE){int c=(slot>=0&&slot<8&&l->players[slot].present)?l->players[slot].battle_follower:10;c+=dir;if(c<10)c=17;if(c>17)c=10;lf2_adhoc_lobby_set_battle_follower(c);}
            else {int c=(slot>=0&&slot<8&&l->players[slot].present)?l->players[slot].character:0;lf2_adhoc_lobby_set_character(net70_next_roster(cheat,c,dir));}
            ui_join();
        }
        if(modifier&&l->settings.mode==LF2_NET_MODE_BATTLE&&(pr&(SCE_CTRL_UP|SCE_CTRL_DOWN))){int slot=l->host?0:l->local_slot;int count=(slot>=0&&slot<8&&l->players[slot].present)?l->players[slot].battle_count:2;count+=(pr&SCE_CTRL_UP)?1:-1;if(count<0)count=3;if(count>3)count=0;lf2_adhoc_lobby_set_battle_count(count);ui_join();}
        if(pr&SCE_CTRL_SQUARE){
            int slot=l->host?0:l->local_slot;
            if(net70_slot_is_active(l,slot)){bool r=(slot>=0&&slot<8&&l->players[slot].present)?!l->players[slot].ready:true;lf2_adhoc_lobby_set_ready(r);ui_ok();}
            else ui_cancel();
        }
        if(l->host){
            int setting_count=4;if(setting_sel>=setting_count)setting_sel=setting_count-1;
            if(!modifier&&pr&SCE_CTRL_UP){setting_sel=(setting_sel+setting_count-1)%setting_count;ui_join();}
            if(!modifier&&pr&SCE_CTRL_DOWN){setting_sel=(setting_sel+1)%setting_count;ui_join();}
            if(pr&(SCE_CTRL_LEFT|SCE_CTRL_RIGHT)){
                int d=(pr&SCE_CTRL_RIGHT)?1:-1;lf2_net_settings_t st=l->settings;
                if(setting_sel==0){st.mode=(uint8_t)((st.mode+(d>0?1:4))%5);if(st.mode==LF2_NET_MODE_STAGE&&st.stage>4)st.stage=0;}
                else if(setting_sel==1){int v=(int)st.max_players+d;if(v<2)v=LF2_ADHOC_MAX_PLAYERS;if(v>LF2_ADHOC_MAX_PLAYERS)v=2;st.max_players=(uint8_t)v;}
                else if(setting_sel==2){int limit=(st.mode==LF2_NET_MODE_STAGE)?5:STAGE_COUNT;int v=(int)st.stage+d;if(v<0)v=limit-1;if(v>=limit)v=0;st.stage=(uint8_t)v;}
                else if(setting_sel==3){int max=cheat?LF2_DIFF_CRAZY:LF2_DIFF_DIFFICULT;int v=(int)st.difficulty+d;if(v<0)v=max;if(v>max)v=0;st.difficulty=(uint8_t)v;}
                lf2_adhoc_lobby_host_settings(&st);ui_join();
            }
            if(pr&SCE_CTRL_START){
                int active_guest=net70_active_guest_slot(l);bool host_ready=l->players[0].present&&l->players[0].ready;
                bool guest_ready=active_guest>0&&l->players[active_guest].present&&l->players[active_guest].ready;
                if(active_guest>0&&host_ready&&guest_ready){
                    lf2_adhoc_lobby_host_status(LF2_NET_STATUS_STARTING);
                    l=lf2_adhoc_lobby();uint32_t match_id=l->revision;last_started_revision=match_id;ui_ok();
                    int mrc=net70_run_adhoc_mode(match_id);
                    /* Return to WAITING and clear every player's Ready bit in
                       one authoritative host snapshot. Clearing only slot 0
                       left the guest stale-ready after a completed match. */
                    lf2_net_settings_t post=lf2_adhoc_lobby()->settings;post.status=LF2_NET_STATUS_WAITING;
                    lf2_adhoc_lobby_host_settings(&post);
                    if(mrc<0){char e[128];snprintf(e,sizeof(e),"Netzwerkspiel wurde getrennt oder konnte nicht starten (Fehler %d).",mrc);wait_message("AdHoc-Spiel",e);}
                    prev=sync_buttons();
                }else ui_cancel();
            }
        }
        l=lf2_adhoc_lobby();
        vita2d_start_drawing();vita2d_clear_screen();
        vita2d_draw_rectangle(0,0,960,544,RGBA8(10,14,21,255));
        vita2d_draw_rectangle(0,0,960,72,RGBA8(20,27,39,255));
        net70_text(24,49,1.06f,RGBA8(252,253,255,255),l->host?"AdHoc-Lobby  |  HOST":"AdHoc-Lobby");
        net70_text(655,47,0.72f,RGBA8(201,219,241,255),(!l->host&&!l->connected)?"Verbindung...":lf2_net_status_name(l->settings.status));
        if(!l->host)net70_draw_ping(820,27,l->host_ping_ms?l->host_ping_ms:0xffff);
        for(int i=0;i<8;i++){int c=i/4,r=i%4;float x=24+c*298,y=84+r*96;bool local=l->host?(i==0):(i==l->local_slot);net70_draw_player_card(x,y,286,88,&l->players[i],i,local,l->settings.mode,net70_slot_is_active(l,i));}
        vita2d_draw_rectangle(620,84,316,384,RGBA8(24,31,44,255));
        net70_text(642,116,0.86f,RGBA8(255,255,255,255),"Spieleinstellungen");
        int connected_count=0;for(int i=0;i<LF2_ADHOC_MAX_PLAYERS;i++)if(l->players[i].present)connected_count++;
        char roleline[80];snprintf(roleline,sizeof(roleline),"2 aktiv / %d verbunden / max %u",connected_count,(unsigned)l->settings.max_players);net70_text(642,137,0.50f,RGBA8(150,174,204,255),roleline);
        const char *labels[]={"Modus","Spieler","Stage","Schwierigkeit"};
        char values[4][64];snprintf(values[0],64,"%s",lf2_net_mode_name(l->settings.mode));snprintf(values[1],64,"%u Vitas",(unsigned)l->settings.max_players);if(l->settings.mode==LF2_NET_MODE_STAGE)snprintf(values[2],64,"Stage %d",(l->settings.stage%5)+1);else snprintf(values[2],64,"%s",g_stages[l->settings.stage%STAGE_COUNT].name);snprintf(values[3],64,"%s",ui_diff_name(l->settings.difficulty));
        int rows=4;float step=64.0f;
        for(int i=0;i<rows;i++){float ry=146+i*step;float rh=58.0f;if(l->host&&setting_sel==i){vita2d_draw_rectangle(634,ry,288,rh,RGBA8(51,113,205,255));net70_draw_outline(634,ry,288,rh,RGBA8(139,202,255,255));}net70_text(650,ry+18,0.55f,RGBA8(166,186,211,255),labels[i]);net70_text(650,ry+39,0.68f,RGBA8(250,252,255,255),values[i]);}
        int local_slot=l->host?0:l->local_slot;bool local_active=net70_slot_is_active(l,local_slot);
        if(!local_active)net70_text(24,502,0.64f,RGBA8(165,190,220,255),"RESERVE/ZUSCHAUER   L/R Auswahl moeglich   Start blockiert nicht");
        else if(l->settings.mode==LF2_NET_MODE_2V2)net70_text(24,502,0.64f,RGBA8(224,234,247,255),"L/R Held   Dreieck+L/R Partner   Quadrat Bereit   Kreis Zurueck");
        else if(l->settings.mode==LF2_NET_MODE_BATTLE)net70_text(24,502,0.59f,RGBA8(224,234,247,255),"L/R Held   Dreieck+L/R Begleiter   Dreieck+Hoch/Runter Anzahl");
        else net70_text(24,502,0.68f,RGBA8(224,234,247,255),"L/R Charakter    Quadrat Bereit    Kreis Zurueck");
        if(l->host){net70_text(642,438,0.64f,RGBA8(255,218,125,255),"Hoch/Runter: Option");net70_text(642,462,0.64f,RGBA8(205,222,242,255),"Links/Rechts: Wert");}
        vita2d_end_drawing();vita2d_swap_buffers();
    }
}
