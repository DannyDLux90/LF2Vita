            if(pr&SCE_CTRL_UP){sel=(sel+total-1)%total;ui_join();}
            if(pr&SCE_CTRL_DOWN){sel=(sel+1)%total;ui_join();}
            if(pr&SCE_CTRL_CIRCLE){if(expanded>=0){expanded=-1;ui_cancel();}else{ui_cancel();lf2_adhoc_browser_stop();sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,SCE_TOUCH_SAMPLING_STATE_STOP);return 0;}}
            if(pr&SCE_CTRL_TRIANGLE&&supported){backend_rc=net70_browser_backend_restart(1);sel=0;scroll=0;expanded=-1;last_probe=0;prev=sync_buttons();ui_join();}
            if(pr&SCE_CTRL_SQUARE&&sel<vn){int si=visible[sel];if(net70_browser_try_join(si,cheat)){backend_rc=net70_browser_start_transport(platform,transport);sel=0;scroll=0;expanded=-1;last_probe=0;prev=sync_buttons();}}
            if(pr&SCE_CTRL_CROSS){
                if(sel<vn){if(expanded==sel){int si=visible[sel];if(net70_browser_try_join(si,cheat)){backend_rc=net70_browser_start_transport(platform,transport);sel=0;scroll=0;expanded=-1;last_probe=0;prev=sync_buttons();}}else{expanded=sel;ui_ok();}}
                else if(sel==vn&&supported&&backend_rc>=0){ui_ok();lf2_adhoc_browser_stop();adhoc_create_game(cheat);backend_rc=net70_browser_start_transport(platform,transport);sel=0;scroll=0;expanded=-1;last_probe=0;prev=sync_buttons();}
                else if(sel==vn+1){ui_cancel();lf2_adhoc_browser_stop();sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,SCE_TOUCH_SAMPLING_STATE_STOP);return 0;}
                else ui_cancel();
            }
        }

        vita2d_start_drawing();vita2d_clear_screen();vita2d_draw_rectangle(0,0,960,544,RGBA8(12,16,23,255));
        vita2d_draw_rectangle(0,0,960,78,RGBA8(20,27,39,255));
        net70_text(24,51,1.08f,RGBA8(252,253,255,255),"Serverbrowser");
        vita2d_draw_rectangle(244,18,132,42,platform==NET70_PLATFORM_VITA?RGBA8(42,88,161,255):RGBA8(95,68,151,255));
        net70_text_center(244,132,48,0.72f,RGBA8(252,253,255,255),net_platform_name(platform));
        net70_draw_segment(552,11,184,56,"Internet",transport==NET70_TRANSPORT_INTERNET,1);
        net70_draw_segment(744,11,184,56,"AdHoc",transport==NET70_TRANSPORT_ADHOC,platform==NET70_PLATFORM_VITA);

        unsigned sbg=supported?(backend_rc>=0?RGBA8(25,73,53,255):RGBA8(89,43,49,255)):RGBA8(53,50,73,255);vita2d_draw_rectangle(24,86,912,50,sbg);
        const char *status=net70_backend_short_ex(platform,transport,backend_rc);
        net70_text(38,119,0.72f,RGBA8(252,253,255,255),status);
        if(supported&&backend_rc>=0)net70_text(780,119,0.64f,RGBA8(182,235,202,255),"AUTO 0.5 s");
        else if(supported&&backend_rc<0)net70_text(748,119,0.62f,RGBA8(255,205,209,255),"Dreieck: erneut");

        char fv[4][48];snprintf(fv[0],48,"%s",browser_filter_mode_name(filter.mode));snprintf(fv[1],48,"%s",browser_filter_status_name(filter.status));snprintf(fv[2],48,"%s",filter.free_only?"Nur frei":"Alle");if(g_browser_ping_limits[filter.ping_index])snprintf(fv[3],48,"bis %d ms",g_browser_ping_limits[filter.ping_index]);else snprintf(fv[3],48,"Alle");
        const char *flab[4]={"MODUS","STATUS","PLAETZE","PING"};const float chipx[4]={24,252,480,708};
        for(int i=0;i<4;i++)net70_draw_filter_chip(chipx[i],146,216,flab[i],fv[i],filter.editing&&filter.selected==i);

        if(vn==0){
            vita2d_draw_rectangle(24,208,912,258,RGBA8(22,28,38,255));
            if(!supported){
                net70_text(62,282,1.02f,RGBA8(244,248,253,255),transport==NET70_TRANSPORT_INTERNET?"Internet-Server":"Diese Verbindung ist nicht verfuegbar");
                net70_text(62,326,0.76f,RGBA8(170,188,211,255),transport==NET70_TRANSPORT_INTERNET?"Der Browser startet immer hier. Das Internet-Backend folgt.":"AdHoc verbindet PS Vita direkt mit PS Vita.");
                if(g_font)vita2d_pgf_draw_textf(g_font,62,370,RGBA8(214,199,244,255),0.72f,"Zielgeraet: %s",net_platform_name(platform));
                if(platform==NET70_PLATFORM_VITA&&transport==NET70_TRANSPORT_INTERNET)net70_text(62,414,0.70f,RGBA8(131,198,255,255),"Tippe oben auf AdHoc - die Vita-Verbindung startet automatisch.");
            }else{
                net70_text(62,282,1.02f,RGBA8(244,248,253,255),backend_rc<0?"AdHoc konnte nicht gestartet werden":"Keine Server gefunden");
                net70_text(62,326,0.76f,RGBA8(170,188,211,255),backend_rc<0?"Dreieck versucht die Verbindung erneut.":"Die Liste aktualisiert sich automatisch zweimal pro Sekunde.");
                if(backend_rc<0)net70_text(62,372,0.64f,RGBA8(232,166,172,255),lf2_adhoc_error());
            }
        }else{
            for(int k=0;k<card_n;k++){int v=card_v[k],si=visible[v];const lf2_adhoc_session_t *ss=lf2_adhoc_browser_session(si);if(ss)net70_draw_server_card(card_y[k],card_h[k],ss,(!filter.editing&&sel==v),expanded==v);}
            if(scroll>0)net70_text(910,226,0.72f,RGBA8(125,144,168,255),"^");
            if(card_n>0&&card_v[card_n-1]<vn-1)net70_text(910,456,0.72f,RGBA8(125,144,168,255),"v");
        }

        net70_modern_button(24,476,356,58,"+ Neues Spiel",!filter.editing&&sel==vn,supported&&backend_rc>=0);
        net70_text(410,511,0.64f,RGBA8(164,182,205,255),supported&&backend_rc>=0?"Liste aktualisiert automatisch":"L/R: Internet / AdHoc");
        net70_modern_button(736,476,200,58,"Zurueck",!filter.editing&&sel==vn+1,1);
        vita2d_end_drawing();vita2d_swap_buffers();
    }
}
