/* Modern PC network frontend restored from the local 0.70 workbench,
   wired to the stock LF2 2.00a TCP/12345 transport. */
static char g_pc_target_ip[16]="";

static void net70_ascii_to_wchar(const char *src,SceWChar16 *dst,int max_chars){
    int i=0;if(!dst||max_chars<=0)return;
    if(src)for(;src[i]&&i<max_chars-1;i++)dst[i]=(SceWChar16)(unsigned char)src[i];
    dst[i]=0;
}

static void net70_wchar_to_ascii(const SceWChar16 *src,char *dst,int max_chars){
    int i=0;if(!dst||max_chars<=0)return;
    if(src)for(;src[i]&&i<max_chars-1;i++){unsigned v=(unsigned)src[i];dst[i]=(v<128)?(char)v:'?';}
    dst[i]=0;
}

static int net70_pc_prompt_ipv4(char ip[16]){
    int mod=sceSysmoduleLoadModule(SCE_SYSMODULE_IME);if(mod<0)return mod;
    SceWChar16 title[64],initial[32],out[32];
    net70_ascii_to_wchar("LF2 2.00a PC IPv4",title,64);net70_ascii_to_wchar(ip,initial,32);memset(out,0,sizeof(out));
    SceImeDialogParam prm;sceImeDialogParamInit(&prm);
    prm.supportedLanguages=SCE_IME_LANGUAGE_GERMAN|SCE_IME_LANGUAGE_ENGLISH;
    prm.languagesForced=0;prm.type=SCE_IME_TYPE_URL;prm.option=SCE_IME_OPTION_NO_AUTO_CAPITALIZATION|SCE_IME_OPTION_NO_ASSISTANCE;
    prm.dialogMode=SCE_IME_DIALOG_DIALOG_MODE_WITH_CANCEL;prm.textBoxMode=SCE_IME_DIALOG_TEXTBOX_MODE_WITH_CLEAR;
    prm.title=title;prm.maxTextLength=15;prm.initialText=initial;prm.inputTextBuffer=out;prm.enterLabel=SCE_IME_ENTER_LABEL_GO;
    int rc=sceImeDialogInit(&prm);if(rc<0){sceSysmoduleUnloadModule(SCE_SYSMODULE_IME);return rc;}
    for(;;){
        SceCommonDialogStatus st=sceImeDialogGetStatus();
        vita2d_start_drawing();vita2d_clear_screen();vita2d_draw_rectangle(0,0,960,544,RGBA8(11,15,22,255));
        net70_text(48,74,1.02f,RGBA8(250,252,255,255),"PC-Adresse eingeben");
        net70_text(48,116,0.72f,RGBA8(181,199,221,255),"IPv4 des PCs mit unveraendertem Little Fighter 2 v2.00a");
        net70_text(48,151,0.66f,RGBA8(146,171,204,255),"Originalprotokoll: TCP Port 12345");
        vita2d_end_drawing();vita2d_common_dialog_update();vita2d_swap_buffers();
        if(st==SCE_COMMON_DIALOG_STATUS_FINISHED)break;
        if(st==SCE_COMMON_DIALOG_STATUS_NONE){rc=-1;break;}
        sceKernelDelayThread(1000);
    }
    if(sceImeDialogGetStatus()==SCE_COMMON_DIALOG_STATUS_FINISHED){
        SceImeDialogResult result;memset(&result,0,sizeof(result));int gr=sceImeDialogGetResult(&result);
        if(gr>=0&&result.result>=0&&result.button==SCE_IME_DIALOG_BUTTON_ENTER){
            char tmp[32];net70_wchar_to_ascii(out,tmp,sizeof(tmp));SceNetInAddr a;
            if(sceNetInetPton(SCE_NET_AF_INET,tmp,&a)>0){strncpy(ip,tmp,15);ip[15]=0;rc=1;}else rc=-2;
        }else rc=0;
    }
    sceImeDialogTerm();sceSysmoduleUnloadModule(SCE_SYSMODULE_IME);return rc;
}

static const char *net70_pc_stock_phase_name(int p){
    switch(p){
        case LF2_PC_STOCK_IDLE:return "Bereit";
        case LF2_PC_STOCK_LISTENING:return "Warte auf LF2-PC";
        case LF2_PC_STOCK_CONNECTING:return "Verbinden";
        case LF2_PC_STOCK_HANDSHAKE:return "LF2 2.00a Handshake";
        case LF2_PC_STOCK_LOCKSTEP:return "Lockstep bereit";
        case LF2_PC_STOCK_ERROR:return "Fehler";
        case LF2_PC_STOCK_CLOSED:return "Getrennt";
        default:return "Unbekannt";
    }
}

static void net70_pc_stock_status_box(float x,float y,float w,float h,const lf2_pc_stock_state_t *st){
    unsigned bg=RGBA8(22,29,40,255),accent=RGBA8(112,99,189,255);
    if(st->connected){bg=RGBA8(22,63,47,255);accent=RGBA8(104,223,151,255);}
    else if(st->phase==LF2_PC_STOCK_ERROR){bg=RGBA8(78,37,43,255);accent=RGBA8(240,116,126,255);}
    vita2d_draw_rectangle(x,y,w,h,bg);vita2d_draw_rectangle(x,y,7,h,accent);
    char line[192];snprintf(line,sizeof(line),"Status: %s",net70_pc_stock_phase_name(st->phase));net70_text(x+24,y+29,0.80f,RGBA8(249,251,254,255),line);
    if(st->message[0])net70_text(x+24,y+58,0.66f,RGBA8(211,224,240,255),st->message);
    if(st->connected){
        snprintf(line,sizeof(line),"Rolle: %s   Peer-Slots: %s   Seed %08X",st->role==LF2_PC_STOCK_ROLE_HOST?"Host 1-4":"Client 5-8",st->peer_slots[0]?st->peer_slots:"--------",(unsigned)st->seed_hash);
        net70_text(x+24,y+87,0.60f,RGBA8(190,235,207,255),line);
    }
}

/* Modern PC frontend backed exclusively by the original, unmodified LF2 2.00a
   TCP/12345 protocol.  The old LF2Vita companion bridge on TCP/24500 is kept
   out of this UI intentionally.  Handshake and the 22-byte input clock are
   available here; exact Windows match-state/menu parity is still WIP, so this
   screen does not start an invented local match configuration. */
