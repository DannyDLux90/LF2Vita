static int net70_run_adhoc_connect_dialog(void){
    int state=-1,src=sceNetCtlAdhocGetState(&state);
    if(src>=0&&state==SCE_NETCTL_STATE_CONNECTED)return 0;

    SceNetCheckDialogParam param;sceNetCheckDialogParamInit(&param);
    SceNetAdhocctlGroupName group;memset(&group,0,sizeof(group));memcpy(group.data,"LF2VITA",7);
    param.mode=SCE_NETCHECK_DIALOG_MODE_ADHOC_CONN;param.groupName=&group;param.timeoutUs=20000000;
    memset(&param.npCommunicationId,0,sizeof(param.npCommunicationId));memcpy(param.npCommunicationId.data,"LF2V00001",9);param.npCommunicationId.term='\0';param.npCommunicationId.num=0;
    int rc=sceNetCheckDialogInit(&param);
    lf2_logf("NET","NetCheckDialog AdHoc init rc=0x%08X pre_state=%d pre_rc=0x%08X",(unsigned)rc,state,(unsigned)src);
    if(rc<0)return rc;

    for(;;){
        SceCommonDialogStatus st=sceNetCheckDialogGetStatus();
        vita2d_start_drawing();vita2d_clear_screen();
        vita2d_draw_rectangle(0,0,960,544,RGBA8(13,17,25,255));
        net70_text(54,82,1.05f,RGBA8(249,251,254,255),"AdHoc wird aktiviert");
        net70_text(54,124,0.66f,RGBA8(176,191,211,255),"Bitte die lokale Vita-Verbindung im Systemdialog bestaetigen.");
        vita2d_end_drawing();vita2d_common_dialog_update();vita2d_swap_buffers();
        if(st==SCE_COMMON_DIALOG_STATUS_FINISHED)break;
        if(st==SCE_COMMON_DIALOG_STATUS_NONE){rc=-1;break;}
        sceKernelDelayThread(1000);
    }
    if(sceNetCheckDialogGetStatus()==SCE_COMMON_DIALOG_STATUS_FINISHED){
        SceNetCheckDialogResult result;memset(&result,0,sizeof(result));int gr=sceNetCheckDialogGetResult(&result);
        lf2_logf("NET","NetCheckDialog result get=0x%08X result=0x%08X",(unsigned)gr,(unsigned)result.result);
        if(gr>=0&&result.result<0)rc=result.result;else if(gr<0)rc=gr;else rc=0;
    }
    sceNetCheckDialogTerm();
    state=-1;src=sceNetCtlAdhocGetState(&state);
    lf2_logf("NET","NetCheckDialog AdHoc done rc=0x%08X state=%d state_rc=0x%08X",(unsigned)rc,state,(unsigned)src);
    if(rc>=0&&src>=0&&state!=SCE_NETCTL_STATE_CONNECTED)return (int)0x80412108u;
    return rc;
}

static int net70_browser_backend_restart(int try_connect){
    lf2_adhoc_browser_stop();
    if(try_connect){
        int rc=lf2_adhoc_init();if(rc<0)return rc;
        int state=-1;if(sceNetCtlAdhocGetState(&state)>=0&&state!=SCE_NETCTL_STATE_CONNECTED){
            rc=net70_run_adhoc_connect_dialog();if(rc<0)return rc;
        }
    }
    return lf2_adhoc_browser_start();
}

/* Internet is always the initial view. Switching to Vita AdHoc is an action,
   not just a tab change: it brings the Vita AdHoc connection up automatically
   and starts discovery immediately, so a separate "connect" button is not needed. */
static int net70_browser_start_transport(int platform,int transport){
    lf2_adhoc_browser_stop();
    if(transport==NET70_TRANSPORT_INTERNET)return NET70_BROWSER_RC_INTERNET_PENDING;
    if(platform==NET70_PLATFORM_PC)return NET70_BROWSER_RC_PC_ADHOC;
    return net70_browser_backend_restart(1);
}

static int net70_browser_try_join(int session_index,bool cheat){
    const lf2_adhoc_session_t *ss=lf2_adhoc_browser_session(session_index);if(!ss)return 0;
    if(strcmp(ss->game_version,LF2_GAME_VERSION)!=0||strcmp(ss->vita_version,LF2_VITA_VERSION)!=0){wait_message("AdHoc","Versionskonflikt. Dieser Host ist nicht kompatibel.");return 0;}
    if(ss->players>=ss->max_players){wait_message("AdHoc","Dieses Spiel ist voll.");return 0;}
    if(ss->status!=LF2_NET_STATUS_WAITING){wait_message("AdHoc","Dieses Spiel nimmt gerade keine neuen Spieler an.");return 0;}
    ui_ok();if(lf2_adhoc_join(session_index,0)<0){wait_message("AdHoc Fehler",lf2_adhoc_error());return 0;}
    adhoc_lobby_screen(cheat);return 1;
}

static void net70_draw_filter_chip(float x,float y,float w,const char *label,const char *value,int selected){
    vita2d_draw_rectangle(x,y,w,54,selected?RGBA8(43,61,88,255):RGBA8(27,34,47,255));
    if(selected)net70_draw_outline(x,y,w,54,RGBA8(91,164,255,255));
    char line[64];snprintf(line,sizeof(line),"%s: %s",label,value);
    net70_text(x+13,y+35,0.69f,RGBA8(244,248,253,255),line);
}

static void net70_draw_server_card(float y,float h,const lf2_adhoc_session_t *ss,int selected,int expanded){
    unsigned bg=selected?RGBA8(38,52,74,255):RGBA8(23,30,42,255);
    vita2d_draw_rectangle(24,y,912,h,bg);if(selected)net70_draw_outline(24,y,912,h,RGBA8(83,158,250,255));
    net70_draw_avatar_badge(42,y+21,ss->host,ss->avatar_hash,selected);
    net70_text(100,y+42,0.88f,RGBA8(252,253,255,255),ss->host);
    char info[128];snprintf(info,sizeof(info),"%s  |  %s",lf2_net_mode_name(ss->mode),lf2_net_status_name(ss->status));net70_text(100,y+76,0.68f,RGBA8(181,199,222,255),info);
    snprintf(info,sizeof(info),"%u / %u Spieler",(unsigned)ss->players,(unsigned)ss->max_players);net70_text(545,y+48,0.76f,RGBA8(238,244,252,255),info);
    net70_draw_ping(758,y+26,ss->ping_ms);
    net70_text(905,y+58,0.78f,RGBA8(203,216,234,255),expanded?"v":">");
    if(expanded){
        vita2d_draw_rectangle(42,y+100,876,2,RGBA8(67,82,103,255));
        const char *st=(ss->stage<STAGE_COUNT)?g_stages[ss->stage].name:"Unbekannt";
        snprintf(info,sizeof(info),"Stage: %s",st);net70_text(52,y+139,0.74f,RGBA8(224,234,247,255),info);
        snprintf(info,sizeof(info),"Schwierigkeit: %s    Rev %u",ui_diff_name(ss->difficulty),(unsigned)ss->revision);net70_text(52,y+174,0.66f,RGBA8(181,199,221,255),info);
        int can_join=ss->players<ss->max_players&&ss->status==LF2_NET_STATUS_WAITING;
        net70_modern_button(710,y+119,196,58,can_join?"Beitreten":(ss->players>=ss->max_players?"Voll":"Gesperrt"),selected,can_join);
    }
}
