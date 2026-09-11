#include <psp2/common_dialog.h>
#include <psp2/net/netctl.h>
#include <psp2/netcheck_dialog.h>
#include <psp2/pspnet_adhocctl.h>

/*
 * The PSP-compatible matching API does not establish the Vita's AdHoc link by
 * itself.  MatchingStart returns SCE_NET_CTL_ERROR_NOT_CONNECTED (0x80412108)
 * until the system network layer has joined an AdHoc group.  Run the Vita
 * NetCheck dialog before starting the browser/host matching object, then keep
 * SceNetCtl alive for the lifetime of the app's network session.
 */
static bool g_adhoc_netctl_ui_ready=false;

static void adhoc_draw_connecting(void){
    vita2d_start_drawing();
    vita2d_clear_screen();
    draw_full_texture(g_menu_bg);
    vita2d_draw_rectangle(175,125,610,285,RGBA8(3,24,80,242));
    if(lf2_ui_font_ready()){
        lf2_ui_draw_text(225,158,0.96f,RGBA8(255,255,255,255),"AdHoc wird verbunden");
        lf2_ui_draw_text(225,215,0.62f,RGBA8(205,225,255,255),"PS Vita AdHoc-Gruppe: LF2VITA");
        lf2_ui_draw_text(225,255,0.56f,RGBA8(175,205,245,255),"Bitte den Systemdialog abschliessen.");
        lf2_ui_draw_text(225,330,0.49f,RGBA8(145,185,235,255),"Danach startet der LF2-Serverbrowser automatisch.");
    }
    vita2d_end_drawing();
}

static int adhoc_ensure_system_link(void){
    int rc=lf2_adhoc_init();
    if(rc<0){
        char msg[240];snprintf(msg,sizeof(msg),"Netzwerk-Initialisierung fehlgeschlagen. %s",lf2_adhoc_error());
        wait_message("AdHoc Fehler",msg);return rc;
    }

    if(!g_adhoc_netctl_ui_ready){
        rc=sceNetCtlInit();
        if(rc<0){
            lf2_logf("NET","sceNetCtlInit: 0x%08X",(unsigned)rc);
            wait_message("AdHoc Fehler","SceNetCtl konnte nicht initialisiert werden. Bitte lf2.log sichern.");
            return rc;
        }
        g_adhoc_netctl_ui_ready=true;
    }

    int state=SCE_NETCTL_STATE_DISCONNECTED;
    rc=sceNetCtlAdhocGetState(&state);
    lf2_logf("NET","adhoc link state before dialog rc=0x%08X state=%d",(unsigned)rc,state);
    if(rc>=0&&state==SCE_NETCTL_STATE_CONNECTED)return 0;

    SceNetAdhocctlGroupName group;memset(&group,0,sizeof(group));
    memcpy(group.data,"LF2VITA",7);
    SceNetCheckDialogParam param;sceNetCheckDialogParamInit(&param);
    param.mode=SCE_NETCHECK_DIALOG_MODE_PSP_ADHOC_CONN;
    param.groupName=&group;
    param.timeoutUs=30000000;

    rc=sceNetCheckDialogInit(&param);
    if(rc<0){
        lf2_logf("NET","sceNetCheckDialogInit: 0x%08X",(unsigned)rc);
        wait_message("AdHoc Fehler","Der PS-Vita-AdHoc-Systemdialog konnte nicht gestartet werden.");
        return rc;
    }
    lf2_log_stage("network:adhoc_link","dialog=started group=LF2VITA mode=psp-adhoc-connect");

    for(;;){
        SceCommonDialogStatus st=sceNetCheckDialogGetStatus();
        if(st!=SCE_COMMON_DIALOG_STATUS_RUNNING)break;
        adhoc_draw_connecting();
        vita2d_common_dialog_update();
        vita2d_swap_buffers();
    }

    SceNetCheckDialogResult result;memset(&result,0,sizeof(result));
    int gr=sceNetCheckDialogGetResult(&result);
    int tr=sceNetCheckDialogTerm();
    lf2_logf("NET","adhoc dialog result get=0x%08X result=0x%08X term=0x%08X",(unsigned)gr,(unsigned)result.result,(unsigned)tr);

    state=SCE_NETCTL_STATE_DISCONNECTED;
    rc=sceNetCtlAdhocGetState(&state);
    lf2_logf("NET","adhoc link state after dialog rc=0x%08X state=%d",(unsigned)rc,state);
    if(gr<0||result.result!=0||rc<0||state!=SCE_NETCTL_STATE_CONNECTED){
        wait_message("AdHoc","Keine AdHoc-Verbindung hergestellt. Bitte WLAN einschalten und den Systemdialog erneut versuchen.");
        return rc<0?rc:-1;
    }

    lf2_log_stage("network:adhoc_link","connected=1 group=LF2VITA");
    return 0;
}

static void adhoc_system_link_shutdown(void){
    if(!g_adhoc_netctl_ui_ready)return;
    int state=SCE_NETCTL_STATE_DISCONNECTED;
    if(sceNetCtlAdhocGetState(&state)>=0&&state==SCE_NETCTL_STATE_CONNECTED){
        int rc=sceNetCtlAdhocDisconnect();
        lf2_logf("NET","adhoc disconnect rc=0x%08X",(unsigned)rc);
    }
    sceNetCtlTerm();
    g_adhoc_netctl_ui_ready=false;
}
