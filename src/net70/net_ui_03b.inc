static int adhoc_browser(bool cheat){
    if(adhoc_ensure_system_link()<0)return 0;
    int rc=lf2_adhoc_browser_start();if(rc<0){char e[220];snprintf(e,sizeof(e),"AdHoc-Initialisierung fehlgeschlagen. %s",lf2_adhoc_error());wait_message("AdHoc Fehler",e);return 0;}
    int sel=0,scroll=0;uint32_t prev=sync_buttons();uint64_t last_probe=0;
    net_browser_filter_t filter={5,4,0,0,0,0};
    for(;;){
        lf2_adhoc_browser_update();
        int visible[LF2_ADHOC_MAX_SESSIONS];int vn=browser_visible_sessions(visible,LF2_ADHOC_MAX_SESSIONS,&filter);int total=vn+2;
        if(sel>=total)sel=total-1;if(sel<0)sel=0;
        uint64_t nu=sceKernelGetSystemTimeWide();
        if(vn>0&&nu-last_probe>700000ULL){for(int v=0;v<vn;v++){int i=visible[v];const lf2_adhoc_session_t *ss=lf2_adhoc_browser_session(i);if(ss&&ss->ping_ms==0xffff){if(lf2_adhoc_browser_probe(i)>=0){last_probe=nu;break;}}}}

        SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;
        if(pr&SCE_CTRL_SELECT){filter.editing=!filter.editing;ui_join();}
        if(filter.editing){
            if(pr&SCE_CTRL_UP){filter.selected=(filter.selected+3)%4;ui_join();}
            if(pr&SCE_CTRL_DOWN){filter.selected=(filter.selected+1)%4;ui_join();}
            if(pr&SCE_CTRL_LEFT){browser_filter_change(&filter,-1);sel=0;scroll=0;ui_join();}
            if(pr&SCE_CTRL_RIGHT){browser_filter_change(&filter,1);sel=0;scroll=0;ui_join();}
            if(pr&SCE_CTRL_CIRCLE){filter.editing=0;ui_cancel();}
        }else{
            if(pr&SCE_CTRL_UP){sel=(sel+total-1)%total;ui_join();}
            if(pr&SCE_CTRL_DOWN){sel=(sel+1)%total;ui_join();}
            if(pr&SCE_CTRL_CIRCLE){ui_cancel();lf2_adhoc_browser_stop();return 0;}
            if(pr&SCE_CTRL_TRIANGLE){lf2_adhoc_browser_stop();lf2_adhoc_browser_start();sel=0;scroll=0;ui_join();}
            if(pr&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE)){
                if(sel<vn){int si=visible[sel];const lf2_adhoc_session_t *ss=lf2_adhoc_browser_session(si);if(!ss)continue;
                    if(strcmp(ss->game_version,LF2_GAME_VERSION)!=0||strcmp(ss->vita_version,LF2_VITA_VERSION)!=0){wait_message("AdHoc","Versionskonflikt. Dieser Host ist nicht kompatibel.");prev=sync_buttons();}
                    else if(ss->players>=ss->max_players){wait_message("AdHoc","Dieses Spiel ist voll.");prev=sync_buttons();}
                    else if(ss->status!=LF2_NET_STATUS_WAITING){wait_message("AdHoc","Dieses Spiel nimmt gerade keine neuen Spieler an.");prev=sync_buttons();}
                    else {lf2_adhoc_browser_probe(si);sceKernelDelayThread(150000);lf2_adhoc_browser_update();if(lf2_adhoc_join(si,0)>=0){ui_ok();int lr=adhoc_lobby_screen(cheat);(void)lr;prev=sync_buttons();}else{ui_cancel();}}
                }else if(sel==vn){lf2_adhoc_browser_stop();ui_ok();adhoc_create_game(cheat);prev=sync_buttons();lf2_adhoc_browser_start();sel=0;scroll=0;}
                else {ui_cancel();lf2_adhoc_browser_stop();return 0;}
            }
        }
        if(!filter.editing){if(sel<scroll)scroll=sel;if(sel>=scroll+5)scroll=sel-4;if(scroll<0)scroll=0;if(scroll>vn-1)scroll=vn>0?vn-1:0;}
