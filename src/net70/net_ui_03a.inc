static int adhoc_create_game(bool cheat){
    lf2_net_settings_t st={0};st.mode=LF2_NET_MODE_VS;st.max_players=2;st.status=LF2_NET_STATUS_WAITING;st.difficulty=LF2_DIFF_NORMAL;st.stage=0;st.allow_cheat=cheat?1:0;int sel=0;uint32_t prev=sync_buttons();
    for(;;){SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;if(pr&SCE_CTRL_UP){sel=(sel+3)%4;ui_join();}if(pr&SCE_CTRL_DOWN){sel=(sel+1)%4;ui_join();}if(pr&SCE_CTRL_CIRCLE){ui_cancel();return 0;}if(pr&(SCE_CTRL_LEFT|SCE_CTRL_RIGHT)){int d=(pr&SCE_CTRL_RIGHT)?1:-1;if(sel==0)st.mode=(uint8_t)((st.mode+(d>0?1:4))%5);else if(sel==1){int v=st.max_players+d;if(v<2)v=8;if(v>8)v=2;st.max_players=v;}else if(sel==2){int v=st.stage+d;if(v<0)v=STAGE_COUNT-1;if(v>=STAGE_COUNT)v=0;st.stage=v;}else{int max=cheat?LF2_DIFF_CRAZY:LF2_DIFF_DIFFICULT;int v=st.difficulty+d;if(v<0)v=max;if(v>max)v=0;st.difficulty=v;}ui_join();}if(pr&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE)){ui_ok();int rc=lf2_adhoc_host_start(&st,0);if(rc<0){char e[220];snprintf(e,sizeof(e),"AdHoc konnte nicht gestartet werden. %s",lf2_adhoc_error());wait_message("AdHoc Fehler",e);prev=sync_buttons();continue;}return adhoc_lobby_screen(cheat);}
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);vita2d_draw_rectangle(160,62,640,420,RGBA8(3,25,79,244));if(lf2_ui_font_ready()){lf2_ui_draw_text(195,82,0.98f,RGBA8(255,255,255,255),"Neues AdHoc-Spiel");lf2_ui_draw_textf(195,122,0.56f,RGBA8(180,215,255,255),"Host: %s",lf2_adhoc_local_name());const char *lab[]={"Spielmodus","Max. Spieler","Stage","Schwierigkeit"};char v[4][64];snprintf(v[0],64,"%s",lf2_net_mode_name(st.mode));snprintf(v[1],64,"%d",st.max_players);snprintf(v[2],64,"%s",g_stages[st.stage].name);snprintf(v[3],64,"%s",ui_diff_name(st.difficulty));for(int i=0;i<4;i++){float y=175+i*58;if(sel==i)vita2d_draw_rectangle(190,y-8,550,42,RGBA8(91,141,225,235));lf2_ui_draw_textf(210,y,0.64f,RGBA8(190,215,250,255),"%s",lab[i]);lf2_ui_draw_textf(425,y,0.68f,RGBA8(255,255,255,255),"%s",v[i]);}lf2_ui_draw_text(210,430,0.55f,RGBA8(220,235,255,255),"Left/Right aendern   X/Square erstellen   Kreis zurueck");}vita2d_end_drawing();vita2d_swap_buffers();}
}

typedef struct {
    int mode;          /* 5 = Alle */
    int status;        /* 4 = Alle */
    int free_only;
    int ping_index;    /* 0=Alle, then thresholds below */
    int editing;
    int selected;
} net_browser_filter_t;

static const int g_browser_ping_limits[]={0,50,100,150,250};

static bool browser_filter_match(const lf2_adhoc_session_t *s,const net_browser_filter_t *f){
    if(!s||!f)return false;
    if(strcmp(s->game_version,LF2_GAME_VERSION)!=0||strcmp(s->vita_version,LF2_VITA_VERSION)!=0)return false;
    if(f->mode<5&&s->mode!=f->mode)return false;
    if(f->status<4&&s->status!=f->status)return false;
    if(f->free_only&&s->players>=s->max_players)return false;
    int lim=g_browser_ping_limits[f->ping_index];
    if(lim>0&&s->ping_ms!=0xffff&&s->ping_ms>(uint16_t)lim)return false;
    return true;
}

static int browser_visible_sessions(int *out,int cap,const net_browser_filter_t *f){
    int n=lf2_adhoc_browser_count(),q=0;
    for(int i=0;i<n&&q<cap;i++){const lf2_adhoc_session_t *s=lf2_adhoc_browser_session(i);if(browser_filter_match(s,f))out[q++]=i;}
    return q;
}

static const char *browser_filter_mode_name(int m){return m>=5?"Alle":lf2_net_mode_name(m);}
static const char *browser_filter_status_name(int s){return s>=4?"Alle":lf2_net_status_name(s);}

static void browser_filter_change(net_browser_filter_t *f,int dir){
    if(!f)return;
    if(f->selected==0){int v=f->mode+dir;if(v<0)v=5;if(v>5)v=0;f->mode=v;}
    else if(f->selected==1){int v=f->status+dir;if(v<0)v=4;if(v>4)v=0;f->status=v;}
    else if(f->selected==2)f->free_only=!f->free_only;
    else {int n=(int)(sizeof(g_browser_ping_limits)/sizeof(g_browser_ping_limits[0]));int v=f->ping_index+dir;if(v<0)v=n-1;if(v>=n)v=0;f->ping_index=v;}
}

