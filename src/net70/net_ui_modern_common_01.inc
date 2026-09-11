/* Restored 0.70 big-type/touch network UI from the local workbench. */
static float net70_text_width(float scale,const char *text);
static void net70_text(float x,float baseline,float scale,unsigned color,const char *text);
static void net70_text_center(float x,float w,float baseline,float scale,unsigned color,const char *text);
static void net70_draw_outline(float x,float y,float w,float h,unsigned color);

static void net70_draw_avatar_badge(float x,float y,const char *name,uint32_t hash,bool selected){
    /* AdHoc must also work without PSN/Internet. The protocol carries a stable
       profile-avatar hash; until a remote thumbnail is cached we render a PS
       profile badge with the player's initial in the same fixed avatar cell. */
    unsigned base=hash?hash:0x6276A8u;unsigned r=70+((base>>16)&63),g=92+((base>>8)&63),b=145+(base&47);
    vita2d_draw_rectangle(x,y,38,38,RGBA8(r,g,b,255));
    vita2d_draw_rectangle(x+3,y+3,32,32,selected?RGBA8(18,41,95,245):RGBA8(10,26,65,245));
    if(g_font){
        char c[2]={(name&&name[0])?name[0]:'?',0};
        vita2d_pgf_draw_text(g_font,(int)x+11,(int)y+28,RGBA8(255,255,255,255),0.76f,c);
        vita2d_pgf_draw_text(g_font,(int)x+23,(int)y+36,RGBA8(160,205,255,255),0.46f,"PS");
    }
}

static void net70_draw_ping(float x,float y,uint16_t ping){
    int bars=lf2_net_ping_bars(ping);unsigned active=RGBA8(130,145,165,160);
    if(bars>=5)active=RGBA8(75,220,110,255);else if(bars==4)active=RGBA8(145,220,80,255);else if(bars==3)active=RGBA8(240,205,70,255);else if(bars==2)active=RGBA8(245,145,55,255);else if(bars==1)active=RGBA8(235,75,70,255);
    for(int i=0;i<5;i++){float h=6+i*4;vita2d_draw_rectangle(x+i*8,y+22-h,5,h,i<bars?active:RGBA8(75,91,120,135));}
    if(g_font){if(ping==0xffff)vita2d_pgf_draw_text(g_font,(int)x+45,(int)y+22,RGBA8(185,202,225,220),0.70f,"-- ms");else vita2d_pgf_draw_textf(g_font,(int)x+45,(int)y+22,RGBA8(220,232,248,245),0.70f,"%u ms",(unsigned)ping);}
}

static int net70_next_roster(bool cheat,int current,int dir){
    int vis[32],n=build_visible_indices(cheat,vis,32),pos=0;if(n<=0)return current;
    for(int i=0;i<n;i++)if(vis[i]==current){pos=i;break;}
    pos=(pos+(dir>0?1:n-1))%n;return vis[pos];
}

static void net70_draw_player_card(float x,float y,float w,float h,const lf2_net_player_t *p,int slot,bool local){
    unsigned bg=local?RGBA8(36,87,166,255):RGBA8(25,32,44,255);
    vita2d_draw_rectangle(x,y,w,h,bg);
    if(local)net70_draw_outline(x,y,w,h,RGBA8(120,190,255,255));
    if(!p||!p->present){char line[32];snprintf(line,sizeof(line),"Slot %d  frei",slot+1);net70_text(x+18,y+h*0.5f+8,0.72f,RGBA8(185,200,220,255),line);return;}
    draw_portrait_index(p->character,x+9,y+9,0.56f);
    char line[80];snprintf(line,sizeof(line),"%s",p->name);net70_text(x+76,y+32,0.78f,RGBA8(255,255,255,255),line);
    snprintf(line,sizeof(line),"%s",(p->character<lf2_roster_count)?lf2_roster[p->character].name:"?");net70_text(x+76,y+58,0.64f,RGBA8(203,219,239,255),line);
    net70_text(x+w-82,y+58,0.60f,p->ready?RGBA8(117,236,151,255):RGBA8(245,205,111,255),p->ready?"BEREIT":"WAHL");
}

static int adhoc_lobby_screen(bool cheat){
    int setting_sel=0;uint32_t prev=sync_buttons();uint64_t lobby_opened=sceKernelGetSystemTimeWide();
    for(;;){
        lf2_adhoc_lobby_update();const lf2_adhoc_lobby_t *l=lf2_adhoc_lobby();
        SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;
        if(pr&SCE_CTRL_CIRCLE){ui_cancel();lf2_adhoc_leave();return 0;}
        if(!l->connected&&!l->host&&sceKernelGetSystemTimeWide()-lobby_opened>8000000ULL){ui_cancel();wait_message("AdHoc","Verbindung zum Host verloren oder Zeitueberschreitung.");lf2_adhoc_leave();return 0;}
        if(pr&SCE_CTRL_LTRIGGER){int slot=l->host?0:l->local_slot;int c=(slot>=0&&slot<8&&l->players[slot].present)?l->players[slot].character:0;lf2_adhoc_lobby_set_character(net70_next_roster(cheat,c,-1));ui_join();}
        if(pr&SCE_CTRL_RTRIGGER){int slot=l->host?0:l->local_slot;int c=(slot>=0&&slot<8&&l->players[slot].present)?l->players[slot].character:0;lf2_adhoc_lobby_set_character(net70_next_roster(cheat,c,1));ui_join();}
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
        vita2d_start_drawing();vita2d_clear_screen();
        vita2d_draw_rectangle(0,0,960,544,RGBA8(10,14,21,255));
        vita2d_draw_rectangle(0,0,960,72,RGBA8(20,27,39,255));
        net70_text(24,49,1.06f,RGBA8(252,253,255,255),l->host?"AdHoc-Lobby  |  HOST":"AdHoc-Lobby");
        net70_text(655,47,0.72f,RGBA8(201,219,241,255),(!l->host&&!l->connected)?"Verbindung...":lf2_net_status_name(l->settings.status));
        if(!l->host)net70_draw_ping(820,27,l->host_ping_ms?l->host_ping_ms:0xffff);
        for(int i=0;i<8;i++){int c=i/4,r=i%4;float x=24+c*298,y=84+r*96;bool local=l->host?(i==0):(i==l->local_slot);net70_draw_player_card(x,y,286,88,&l->players[i],i,local);}
        vita2d_draw_rectangle(620,84,316,384,RGBA8(24,31,44,255));
        net70_text(642,116,0.86f,RGBA8(255,255,255,255),"Spieleinstellungen");
        const char *labels[]={"Modus","Spieler","Stage","Schwierigkeit"};
        char values[4][64];snprintf(values[0],64,"%s",lf2_net_mode_name(l->settings.mode));snprintf(values[1],64,"%d / %d",({int q=0;for(int i=0;i<8;i++)if(l->players[i].present)q++;q;}),l->settings.max_players);snprintf(values[2],64,"%s",g_stages[l->settings.stage%STAGE_COUNT].name);snprintf(values[3],64,"%s",ui_diff_name(l->settings.difficulty));
        for(int i=0;i<4;i++){float ry=136+i*66;if(l->host&&setting_sel==i){vita2d_draw_rectangle(634,ry,288,58,RGBA8(51,113,205,255));net70_draw_outline(634,ry,288,58,RGBA8(139,202,255,255));}net70_text(650,ry+22,0.58f,RGBA8(166,186,211,255),labels[i]);net70_text(650,ry+50,0.76f,RGBA8(250,252,255,255),values[i]);}
        net70_text(24,502,0.70f,RGBA8(224,234,247,255),"L/R Charakter    Quadrat Bereit    Kreis Zurueck");
        if(l->host){net70_text(642,430,0.70f,RGBA8(255,218,125,255),"Hoch/Runter: Option");net70_text(642,457,0.70f,RGBA8(205,222,242,255),"Links/Rechts: Wert");}
        vita2d_end_drawing();vita2d_swap_buffers();
    }
}
