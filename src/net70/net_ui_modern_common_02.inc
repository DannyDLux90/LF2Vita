static int adhoc_create_game(bool cheat){
    lf2_net_settings_t st={0};st.mode=LF2_NET_MODE_VS;st.max_players=2;st.status=LF2_NET_STATUS_WAITING;st.difficulty=LF2_DIFF_NORMAL;st.stage=0;st.allow_cheat=cheat?1:0;st.battle_follower=10;st.battle_count=2;int sel=0;uint32_t prev=sync_buttons();
    for(;;){
        SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;
        if(pr&SCE_CTRL_UP){sel=(sel+3)%4;ui_join();}
        if(pr&SCE_CTRL_DOWN){sel=(sel+1)%4;ui_join();}
        if(pr&SCE_CTRL_CIRCLE){ui_cancel();return 0;}
        if(pr&(SCE_CTRL_LEFT|SCE_CTRL_RIGHT)){
            int d=(pr&SCE_CTRL_RIGHT)?1:-1;
            if(sel==0)st.mode=(uint8_t)((st.mode+(d>0?1:4))%5);
            else if(sel==1){int v=(int)st.max_players+d;if(v<2)v=LF2_ADHOC_MAX_PLAYERS;if(v>LF2_ADHOC_MAX_PLAYERS)v=2;st.max_players=(uint8_t)v;}
            else if(sel==2){int v=st.stage+d;if(v<0)v=STAGE_COUNT-1;if(v>=STAGE_COUNT)v=0;st.stage=v;}
            else{int max=cheat?LF2_DIFF_CRAZY:LF2_DIFF_DIFFICULT;int v=(int)st.difficulty+d;if(v<0)v=max;if(v>max)v=0;st.difficulty=v;}
            ui_join();
        }
        if(pr&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE)){
            ui_ok();int rc=lf2_adhoc_host_start(&st,0);if(rc<0){char e[220];snprintf(e,sizeof(e),"AdHoc konnte nicht gestartet werden. %s",lf2_adhoc_error());wait_message("AdHoc Fehler",e);prev=sync_buttons();continue;}return adhoc_lobby_screen(cheat);
        }
        vita2d_start_drawing();vita2d_clear_screen();
        vita2d_draw_rectangle(0,0,960,544,RGBA8(10,14,21,255));
        vita2d_draw_rectangle(0,0,960,80,RGBA8(20,27,39,255));
        net70_text(48,51,1.12f,RGBA8(252,253,255,255),"Neues AdHoc-Spiel");
        char hostline[96];snprintf(hostline,sizeof(hostline),"Host: %s",lf2_adhoc_local_name());net70_text(50,76,0.66f,RGBA8(177,196,219,255),hostline);
        const char *lab[]={"Spielmodus","Max. Spieler","Stage","Schwierigkeit"};char v[4][64];snprintf(v[0],64,"%s",lf2_net_mode_name(st.mode));snprintf(v[1],64,"%u Vitas",(unsigned)st.max_players);snprintf(v[2],64,"%s",g_stages[st.stage].name);snprintf(v[3],64,"%s",ui_diff_name(st.difficulty));
        for(int i=0;i<4;i++){float y=104+i*78;unsigned bg=sel==i?RGBA8(48,106,196,255):RGBA8(25,33,46,255);vita2d_draw_rectangle(72,y,816,66,bg);if(sel==i)net70_draw_outline(72,y,816,66,RGBA8(135,201,255,255));net70_text(100,y+41,0.78f,RGBA8(207,220,238,255),lab[i]);net70_text(520,y+42,0.90f,RGBA8(255,255,255,255),v[i]);net70_text(846,y+41,0.72f,RGBA8(191,213,240,255),"< >");}
        vita2d_draw_rectangle(72,430,816,66,RGBA8(19,25,35,255));
        net70_text(98,470,0.68f,RGBA8(224,234,247,255),"Hoch/Runter waehlen     Links/Rechts aendern");
        net70_text(570,470,0.68f,RGBA8(131,201,255,255),"X / Quadrat  ERSTELLEN");
        net70_text(72,526,0.62f,RGBA8(159,177,200,255),"Kreis  Zurueck");
        vita2d_end_drawing();vita2d_swap_buffers();
    }
}

typedef struct {
    int pressed;
    float x;
    float y;
} net70_touch_press_t;

static int net70_point_in(float px,float py,float x,float y,float w,float h){return px>=x&&px<=x+w&&py>=y&&py<=y+h;}

static void net70_touch_poll(net70_touch_press_t *out,int *was_down){
    if(!out||!was_down)return;
    memset(out,0,sizeof(*out));
    SceTouchData td;memset(&td,0,sizeof(td));
    int rc=sceTouchPeek(SCE_TOUCH_PORT_FRONT,&td,1);
    int down=(rc>0&&td.reportNum>0)?1:0;
    if(down){
        static int panel_ready=0;static SceTouchPanelInfo pi;
        if(!panel_ready){memset(&pi,0,sizeof(pi));if(sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT,&pi)>=0)panel_ready=1;}
        float minx=panel_ready?(float)pi.minDispX:0.0f,miny=panel_ready?(float)pi.minDispY:0.0f;
        float maxx=panel_ready?(float)pi.maxDispX:1919.0f,maxy=panel_ready?(float)pi.maxDispY:1087.0f;
        float dx=maxx-minx,dy=maxy-miny;if(dx<1.0f)dx=1919.0f;if(dy<1.0f)dy=1087.0f;
        out->x=((float)td.report[0].x-minx)*959.0f/dx;
        out->y=((float)td.report[0].y-miny)*543.0f/dy;
        if(out->x<0)out->x=0;if(out->x>959)out->x=959;if(out->y<0)out->y=0;if(out->y>543)out->y=543;
        out->pressed=!(*was_down);
    }
    *was_down=down;
}

/* Network screens now use the same readable PVF helper as the rest of LF2Vita
   whenever it is available, with PGF as a safe fallback. */
static vita2d_texture *g_net70_vita_button_tex,*g_net70_pc_button_tex;

static vita2d_texture *net70_platform_texture(int platform){
    vita2d_texture **slot=platform==1?&g_net70_pc_button_tex:&g_net70_vita_button_tex;
    const char *path=platform==1?"app0:/assets/net70_pc_button.png":"app0:/assets/net70_vita_button.png";
    if(!*slot)*slot=vita2d_load_PNG_file(path);
    return *slot;
}

static float net70_text_width(float scale,const char *text){
    if(scale<0.70f)scale=0.70f;
    if(lf2_ui_font_ready())return lf2_ui_text_width(scale,text);
    return (g_font&&text)?(float)vita2d_pgf_text_width(g_font,scale,text):0.0f;
}
static void net70_text(float x,float baseline,float scale,unsigned color,const char *text){
    if(scale<0.70f)scale=0.70f;
    if(lf2_ui_font_ready()&&text){lf2_ui_draw_text(x,baseline-24.0f*scale,scale,color,text);return;}
    if(g_font&&text){vita2d_pgf_draw_text(g_font,(int)x+1,(int)baseline+1,RGBA8(0,0,0,215),scale,text);vita2d_pgf_draw_text(g_font,(int)x,(int)baseline,color,scale,text);}
}
static void net70_text_center(float x,float w,float baseline,float scale,unsigned color,const char *text){float tw=net70_text_width(scale,text);net70_text(x+(w-tw)*0.5f,baseline,scale,color,text);}

static void net70_draw_outline(float x,float y,float w,float h,unsigned color){
    vita2d_draw_rectangle(x,y,w,3,color);vita2d_draw_rectangle(x,y+h-3,w,3,color);
    vita2d_draw_rectangle(x,y,3,h,color);vita2d_draw_rectangle(x+w-3,y,3,h,color);
}

static void net70_modern_button(float x,float y,float w,float h,const char *label,int selected,int enabled){
    unsigned bg=!enabled?RGBA8(43,49,59,255):(selected?RGBA8(52,119,220,255):RGBA8(30,38,52,255));
    unsigned fg=!enabled?RGBA8(125,134,148,255):RGBA8(249,251,254,255);
    vita2d_draw_rectangle(x,y,w,h,bg);
    if(selected&&enabled)net70_draw_outline(x,y,w,h,RGBA8(132,195,255,255));
    net70_text_center(x,w,y+h*0.5f+10.0f,0.80f,fg,label);
}

enum { NET70_PLATFORM_VITA=0, NET70_PLATFORM_PC=1 };
enum { NET70_TRANSPORT_INTERNET=0, NET70_TRANSPORT_ADHOC=1 };
#define NET70_BROWSER_RC_INTERNET_PENDING (-7001)
#define NET70_BROWSER_RC_PC_ADHOC (-7002)

static const char *net_platform_name(int platform){return platform==NET70_PLATFORM_PC?"PC":"PS Vita";}

static void net70_draw_segment(float x,float y,float w,float h,const char *label,int active,int enabled){
    unsigned bg=active?RGBA8(56,124,224,255):RGBA8(29,37,51,255);
    unsigned fg=enabled?RGBA8(249,251,254,255):RGBA8(130,140,154,255);
    if(!enabled)bg=RGBA8(39,44,54,255);
    vita2d_draw_rectangle(x,y,w,h,bg);
    if(active&&enabled)net70_draw_outline(x,y,w,h,RGBA8(140,201,255,255));
    net70_text_center(x,w,y+h*0.5f+10.0f,0.80f,fg,label);
}

static const char *net70_backend_short(int rc){
    if(rc>=0)return "AdHoc aktiv - automatische Suche laeuft";
    if((uint32_t)rc==0x80412108u)return "AdHoc konnte nicht verbunden werden";
    if((uint32_t)rc==0x80412101u)return "Netzwerkdienst nicht initialisiert";
    return "AdHoc Discovery nicht verfuegbar";
}

static const char *net70_backend_short_ex(int platform,int transport,int rc){
    if(transport==NET70_TRANSPORT_INTERNET)return platform==NET70_PLATFORM_PC?"PC Internet - Backend in Vorbereitung":"PS Vita Internet - Backend in Vorbereitung";
    if(platform==NET70_PLATFORM_PC)return "AdHoc ist nur fuer PS Vita verfuegbar";
    return net70_backend_short(rc);
}
