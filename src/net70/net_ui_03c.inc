                    else{ui_ok();if(lf2_adhoc_join(si,0)<0){wait_message("AdHoc Fehler",lf2_adhoc_error());prev=sync_buttons();}else{adhoc_lobby_screen(cheat);lf2_adhoc_browser_start();sel=0;scroll=0;prev=sync_buttons();}}
                }else if(sel==vn){ui_ok();lf2_adhoc_browser_stop();adhoc_create_game(cheat);lf2_adhoc_browser_start();sel=0;scroll=0;prev=sync_buttons();}
                else{ui_cancel();lf2_adhoc_browser_stop();return 0;}
            }
        }

        if(sel<scroll)scroll=sel;if(sel>=scroll+5)scroll=sel-4;if(scroll>vn)scroll=vn;
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);vita2d_draw_rectangle(20,24,920,496,RGBA8(2,22,69,247));
        if(lf2_ui_font_ready()){
            lf2_ui_draw_text(42,37,0.91f,RGBA8(255,255,255,255),"AdHoc Serverbrowser");
            lf2_ui_draw_textf(390,42,0.48f,RGBA8(140,235,165,255),"LF2 %s",lf2_adhoc_game_version());
            lf2_ui_draw_textf(500,42,0.48f,RGBA8(140,235,165,255),"LF2Vita %s",lf2_adhoc_vita_version());
            lf2_ui_draw_textf(680,42,0.48f,RGBA8(180,215,255,255),"Profil: %s",lf2_adhoc_local_name());
            lf2_ui_draw_text(42,65,0.42f,RGBA8(120,225,150,255),"Kompatibilitaet ist Pflicht: abweichende Spiel-/Vita-Versionen werden ausgeblendet.");

            const char *flab[]={"Modus","Status","Freie Plaetze","Max. Ping"};
            char fv[4][48];snprintf(fv[0],48,"%s",browser_filter_mode_name(filter.mode));snprintf(fv[1],48,"%s",browser_filter_status_name(filter.status));snprintf(fv[2],48,"%s",filter.free_only?"Ja":"Alle");if(g_browser_ping_limits[filter.ping_index])snprintf(fv[3],48,"<= %d ms",g_browser_ping_limits[filter.ping_index]);else snprintf(fv[3],48,"Alle");
            for(int i=0;i<4;i++){float x=42+i*222;bool hi=filter.editing&&filter.selected==i;vita2d_draw_rectangle(x,82,210,42,hi?RGBA8(77,126,207,245):RGBA8(8,39,99,225));lf2_ui_draw_textf(x+8,87,0.40f,RGBA8(155,190,235,255),"%s",flab[i]);lf2_ui_draw_textf(x+8,104,0.48f,RGBA8(255,255,255,255),"%s",fv[i]);}

            float hy=145;lf2_ui_draw_text(48,hy,0.48f,RGBA8(155,195,240,255),"Host");lf2_ui_draw_text(416,hy,0.48f,RGBA8(155,195,240,255),"Spieler");lf2_ui_draw_text(492,hy,0.48f,RGBA8(155,195,240,255),"Status");lf2_ui_draw_text(653,hy,0.48f,RGBA8(155,195,240,255),"Modus");lf2_ui_draw_text(750,hy,0.48f,RGBA8(155,195,240,255),"Verbindung");
            lf2_ui_draw_textf(845,145,0.38f,RGBA8(135,165,205,255),"%d/%d",vn,lf2_adhoc_browser_count());
        }
        int shown=0;for(int v=scroll;v<vn&&shown<5;v++,shown++){int i=visible[v];const lf2_adhoc_session_t *ss=lf2_adhoc_browser_session(i);float y=169+shown*48;bool hi=!filter.editing&&sel==v;if(hi)vita2d_draw_rectangle(38,y-7,870,42,RGBA8(70,116,202,235));else vita2d_draw_rectangle(38,y-7,870,42,RGBA8(8,37,96,210));net_draw_avatar_badge(48,y-4,ss->host,ss->avatar_hash,hi);if(lf2_ui_font_ready()){lf2_ui_draw_textf(96,y,0.57f,RGBA8(255,255,255,255),"%s",ss->host);lf2_ui_draw_textf(433,y,0.55f,RGBA8(240,245,255,255),"%d/%d",ss->players,ss->max_players);lf2_ui_draw_textf(492,y,0.46f,RGBA8(230,238,255,255),"%s",lf2_net_status_name(ss->status));lf2_ui_draw_textf(655,y,0.51f,RGBA8(240,245,255,255),"%s",lf2_net_mode_name(ss->mode));}net_draw_ping(755,y-2,ss->ping_ms);}
        float cy=425;if(!filter.editing&&sel==vn)vita2d_draw_rectangle(40,cy-8,555,38,RGBA8(88,143,226,240));if(lf2_ui_font_ready()){if(vn==0)lf2_ui_draw_text(57,177,0.58f,RGBA8(180,205,235,255),lf2_adhoc_browser_count()?"Keine Server entsprechen den aktiven Filtern.":"Keine kompatiblen AdHoc-Spiele gefunden - Suche laeuft...");lf2_ui_draw_text(58,cy,0.62f,RGBA8(255,255,255,255),"+ Neues AdHoc-Spiel erstellen");if(!filter.editing&&sel==vn+1)vita2d_draw_rectangle(650,cy-8,225,38,RGBA8(88,143,226,240));lf2_ui_draw_text(682,cy,0.62f,RGBA8(255,255,255,255),"Zurueck");lf2_ui_draw_text(47,470,0.43f,RGBA8(190,215,245,255),filter.editing?"FILTER: Up/Down Auswahl   Left/Right aendern   SELECT/Kreis schliessen":"X: Beitreten/Erstellen   Dreieck: Neu suchen   SELECT: Filter   Kreis: Zurueck");}
        vita2d_end_drawing();vita2d_swap_buffers();
    }
}
