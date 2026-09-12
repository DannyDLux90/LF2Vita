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

static int net70_two_player_chars(const lf2_adhoc_lobby_t *l,int *host_char,int *guest_char,int *guest_slot){
    if(!l||!l->connected)return -1;
    int guest=-1,present=0;for(int i=0;i<LF2_ADHOC_MAX_PLAYERS;i++)if(l->players[i].present){present++;if(i>0&&guest<0)guest=i;}
    if(present!=2||guest<1)return -2;
    int hc=l->players[0].character,gc=l->players[guest].character;
    if(hc<0||hc>=lf2_roster_count||gc<0||gc>=lf2_roster_count)return -3;
    if(host_char)*host_char=hc;if(guest_char)*guest_char=gc;if(guest_slot)*guest_slot=guest;return 0;
}

static void net70_native_match_options(lf2_match_options_t *opt,const char *label,int host_actor,int guest_actor){
    memset(opt,0,sizeof(*opt));opt->allow_rematch=false;opt->hud_label=label;opt->lockstep_actor_map_enabled=true;opt->lockstep_native_vita_input=true;
    if(host_actor>=0&&host_actor<=LF2_MAX_CPUS)opt->lockstep_actor_control[host_actor]=1;
    if(guest_actor>=0&&guest_actor<=LF2_MAX_CPUS)opt->lockstep_actor_control[guest_actor]=5;
    opt->lockstep_frame=lf2_adhoc_lockstep_frame;opt->lockstep_state=lf2_adhoc_match_report_state;opt->remote_userdata=NULL;
}

static int net70_run_adhoc_arena_match(uint32_t match_id,int mode,int host_char,int guest_char,int stage,int difficulty,bool net_cheat){
    int cpus[LF2_MAX_CPUS]={0},teams[LF2_MAX_CPUS]={0},n=0,guest_actor=1;
    const char *label="AdHoc VS";
    if(mode==LF2_NET_MODE_VS){cpus[n]=guest_char;teams[n++]=2;label="AdHoc VS";}
    else if(mode==LF2_NET_MODE_1V1){cpus[n]=guest_char;teams[n++]=2;label="AdHoc 1 vs 1 Championship";}
    else if(mode==LF2_NET_MODE_BATTLE){
        /* Native two-Vita Battle: each human leads two stock Bandit soldiers. */
        cpus[n]=guest_char;teams[n++]=2;guest_actor=1;
        cpus[n]=10;teams[n++]=1;cpus[n]=10;teams[n++]=1;
        cpus[n]=10;teams[n++]=2;cpus[n]=10;teams[n++]=2;label="AdHoc Battle";
    }else if(mode==LF2_NET_MODE_2V2){
        int hp=(host_char+(int)(match_id%7u)+1)%10;
        int gp=(guest_char+(int)((match_id/7u)%7u)+2)%10;
        if(hp==host_char)hp=(hp+1)%10;if(gp==guest_char)gp=(gp+1)%10;
        cpus[n]=hp;teams[n++]=1;
        guest_actor=n+1;cpus[n]=guest_char;teams[n++]=2;
        cpus[n]=gp;teams[n++]=2;label="AdHoc 2 vs 2 Championship";
    }else return -10;
    vita2d_texture *bg=load_stage_texture(stage);if(!bg)return -5;
    if(lf2_adhoc_match_begin(match_id)<0){free_stage_texture(bg);return -4;}
    lf2_match_options_t opt;net70_native_match_options(&opt,label,0,guest_actor);
    int rc=lf2_run_match_ex(g_font,bg,host_char,1,cpus,teams,n,difficulty,net_cheat,&opt);
    free_stage_texture(bg);lf2_adhoc_match_end();return rc;
}

static int net70_build_network_stage(lf2_stage_runtime_t *rt,int major,int host_char,int guest_char,int difficulty,uint32_t seed,int host_hp,int host_mp,int guest_hp,int guest_mp,int *host_hp_out,int *host_mp_out,int *guest_hp_out,int *guest_mp_out){
    if(!rt||major<0||major>=5)return -1;(void)host_char;memset(rt,0,sizeof(*rt));
    rt->player_hp=host_hp;rt->player_mp=host_mp;rt->player_hp_out=host_hp_out;rt->player_mp_out=host_mp_out;
    rt->ally_count=1;rt->ally_roster[0]=guest_char;rt->ally_hp[0]=guest_hp;rt->ally_mp[0]=guest_mp;rt->ally_hp_out=guest_hp_out;rt->ally_mp_out=guest_mp_out;
    rt->lockstep_actor_map_enabled=true;rt->lockstep_native_vita_input=true;rt->lockstep_actor_control[0]=1;rt->lockstep_actor_control[1]=5;rt->lockstep_frame=lf2_adhoc_lockstep_frame;rt->lockstep_state=lf2_adhoc_match_report_state;
    static char labels[5][24]={{0}};snprintf(labels[major],sizeof(labels[major]),"AdHoc Stage %d",major+1);rt->hud_prefix=labels[major];
    unsigned rng=seed^0x53544147u^(unsigned)(major*0x9E37u);int last_bound=0;
    for(int sub=0;sub<5;sub++){
        int stage_id=major*10+sub;lf2_stage_t st;if(lf2_stage_load(stage_id,&st)<0)return -2;if(st.phase_count<1)continue;
        int first=st.phases[0].bound,offset=(sub==0)?0:(last_bound+360-first);
        for(int ph=0;ph<st.phase_count&&rt->phase_count<LF2_STAGE_RUNTIME_MAX_PHASES;ph++){
            lf2_stage_runtime_phase_t *dst=&rt->phases[rt->phase_count++];dst->bound=st.phases[ph].bound+offset;if(dst->bound<=last_bound)dst->bound=last_bound+220;
            dst->enemy_count=lf2_stage_build_phase(&st.phases[ph],difficulty,dst->enemies,dst->hp,LF2_STAGE_RUNTIME_MAX_ENEMIES,&rng);
            dst->item_count=lf2_stage_build_items(&st.phases[ph],difficulty,dst->item_oid,dst->item_x,LF2_STAGE_RUNTIME_MAX_ITEMS,&rng);last_bound=dst->bound;
        }
    }
    return rt->phase_count>0?0:-3;
}

static int net70_run_adhoc_stage(uint32_t match_id,int host_char,int guest_char,int start_major,int difficulty,bool net_cheat){
    int host_hp=500,host_mp=500,guest_hp=500,guest_mp=500;
    if(start_major<0||start_major>4)start_major=0;
    for(int major=start_major;major<5;major++){
        lf2_stage_runtime_t rt;int brc=net70_build_network_stage(&rt,major,host_char,guest_char,difficulty,match_id,host_hp,host_mp,guest_hp,guest_mp,&host_hp,&host_mp,&guest_hp,&guest_mp);if(brc<0)return -20+brc;
        vita2d_texture *bg=load_stage_texture(stage_background_for_major(major));if(!bg)return -30;
        uint32_t sub_id=match_id*16u+(uint32_t)(major+1);if(!sub_id)sub_id=(uint32_t)(major+1);
        if(lf2_adhoc_match_begin(sub_id)<0){free_stage_texture(bg);return -31;}
        lf2_log_stage("network:adhoc_stage_launch","id=%u major=%d host=%s guest=%s phases=%d",(unsigned)sub_id,major+1,lf2_roster[host_char].name,lf2_roster[guest_char].name,rt.phase_count);
        int rc=lf2_run_stage_section(g_font,bg,host_char,difficulty,net_cheat,&rt);free_stage_texture(bg);lf2_adhoc_match_end();if(rc!=LF2_MATCH_WIN)return rc;
        int heal=(difficulty==LF2_DIFF_EASY)?200:(difficulty==LF2_DIFF_NORMAL?150:(difficulty==LF2_DIFF_DIFFICULT?100:50));
        host_hp+=heal;if(host_hp>500)host_hp=500;guest_hp+=heal;if(guest_hp>500)guest_hp=500;host_mp=guest_mp=500;
    }
    return LF2_MATCH_WIN;
}

static int net70_run_adhoc_mode(uint32_t match_id){
    const lf2_adhoc_lobby_t *l=lf2_adhoc_lobby();int host_char=0,guest_char=0,guest_slot=0;
    int pc=net70_two_player_chars(l,&host_char,&guest_char,&guest_slot);if(pc<0)return pc;(void)guest_slot;
    int mode=l->settings.mode;int stage=(l->settings.stage<STAGE_COUNT)?l->settings.stage:0;int difficulty=l->settings.difficulty;
    if(difficulty<LF2_DIFF_EASY||difficulty>LF2_DIFF_CRAZY)difficulty=LF2_DIFF_NORMAL;bool net_cheat=l->settings.allow_cheat!=0;
    lf2_log_stage("network:adhoc_match_launch","id=%u role=%s mode=%s host_char=%s client_char=%s stage=%d diff=%s cheat=%d",
                  (unsigned)match_id,l->host?"host":"client",lf2_net_mode_name(mode),lf2_roster[host_char].name,lf2_roster[guest_char].name,stage,ui_diff_name(difficulty),net_cheat?1:0);
    int rc=(mode==LF2_NET_MODE_STAGE)?net70_run_adhoc_stage(match_id,host_char,guest_char,stage%5,difficulty,net_cheat):net70_run_adhoc_arena_match(match_id,mode,host_char,guest_char,stage,difficulty,net_cheat);
    lf2_log_stage("network:adhoc_match_return","id=%u role=%s mode=%s rc=%d",(unsigned)match_id,l->host?"host":"client",lf2_net_mode_name(mode),rc);return rc;
}

static int adhoc_lobby_screen(bool cheat){
    int setting_sel=0;uint32_t prev=sync_buttons();uint64_t lobby_opened=sceKernelGetSystemTimeWide();uint32_t last_started_revision=0;
    for(;;){
        lf2_adhoc_lobby_update();const lf2_adhoc_lobby_t *l=lf2_adhoc_lobby();
        SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;
        if(pr&SCE_CTRL_CIRCLE){ui_cancel();lf2_adhoc_leave();return 0;}
        if(!l->connected&&!l->host&&sceKernelGetSystemTimeWide()-lobby_opened>8000000ULL){ui_cancel();wait_message("AdHoc","Verbindung zum Host verloren oder Zeitueberschreitung.");lf2_adhoc_leave();return 0;}
        if(!l->host&&l->connected&&l->settings.status==LF2_NET_STATUS_STARTING&&l->revision!=last_started_revision){
            uint32_t match_id=l->revision;last_started_revision=match_id;
            int mrc=net70_run_adhoc_mode(match_id);
            if(mrc<0){char e[128];snprintf(e,sizeof(e),"Netzwerkspiel konnte nicht gestartet werden (Fehler %d).",mrc);wait_message("AdHoc-Spiel",e);}
            prev=sync_buttons();continue;
        }
        if(pr&SCE_CTRL_LTRIGGER){int slot=l->host?0:l->local_slot;int c=(slot>=0&&slot<8&&l->players[slot].present)?l->players[slot].character:0;lf2_adhoc_lobby_set_character(net70_next_roster(cheat,c,-1));ui_join();}
        if(pr&SCE_CTRL_RTRIGGER){int slot=l->host?0:l->local_slot;int c=(slot>=0&&slot<8&&l->players[slot].present)?l->players[slot].character:0;lf2_adhoc_lobby_set_character(net70_next_roster(cheat,c,1));ui_join();}
        if(pr&SCE_CTRL_SQUARE){int slot=l->host?0:l->local_slot;bool r=(slot>=0&&slot<8&&l->players[slot].present)?!l->players[slot].ready:true;lf2_adhoc_lobby_set_ready(r);ui_ok();}
        if(l->host){
            if(pr&SCE_CTRL_UP){setting_sel=(setting_sel+3)%4;ui_join();}
            if(pr&SCE_CTRL_DOWN){setting_sel=(setting_sel+1)%4;ui_join();}
            if(pr&(SCE_CTRL_LEFT|SCE_CTRL_RIGHT)){
                int d=(pr&SCE_CTRL_RIGHT)?1:-1;lf2_net_settings_t st=l->settings;
                if(setting_sel==0){st.mode=(uint8_t)((st.mode+(d>0?1:4))%5);if(st.mode==LF2_NET_MODE_STAGE&&st.stage>4)st.stage=0;}
                else if(setting_sel==1){st.max_players=2;}
                else if(setting_sel==2){int limit=(st.mode==LF2_NET_MODE_STAGE)?5:STAGE_COUNT;int v=(int)st.stage+d;if(v<0)v=limit-1;if(v>=limit)v=0;st.stage=(uint8_t)v;}
                else {int max=cheat?LF2_DIFF_CRAZY:LF2_DIFF_DIFFICULT;int v=(int)st.difficulty+d;if(v<0)v=max;if(v>max)v=0;st.difficulty=(uint8_t)v;}
                lf2_adhoc_lobby_host_settings(&st);ui_join();
            }
            if(pr&SCE_CTRL_START){
                int present=0,ready=0;for(int i=0;i<8;i++)if(l->players[i].present){present++;if(l->players[i].ready)ready++;}
                if(present==2&&ready==present){
                    lf2_adhoc_lobby_host_status(LF2_NET_STATUS_STARTING);
                    l=lf2_adhoc_lobby();uint32_t match_id=l->revision;last_started_revision=match_id;ui_ok();
                    int mrc=net70_run_adhoc_mode(match_id);
                    lf2_adhoc_lobby_host_status(LF2_NET_STATUS_WAITING);
                    lf2_adhoc_lobby_set_ready(false);
                    if(mrc<0){char e[128];snprintf(e,sizeof(e),"Netzwerkspiel wurde getrennt oder konnte nicht starten (Fehler %d).",mrc);wait_message("AdHoc-Spiel",e);}
                    prev=sync_buttons();
                } else if(present!=2){wait_message("AdHoc-Spiel","Native AdHoc ist derzeit fuer genau zwei Vitas ausgelegt.");prev=sync_buttons();}
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
        char values[4][64];snprintf(values[0],64,"%s",lf2_net_mode_name(l->settings.mode));snprintf(values[1],64,"2 Vitas");if(l->settings.mode==LF2_NET_MODE_STAGE)snprintf(values[2],64,"Stage %d",(l->settings.stage%5)+1);else snprintf(values[2],64,"%s",g_stages[l->settings.stage%STAGE_COUNT].name);snprintf(values[3],64,"%s",ui_diff_name(l->settings.difficulty));
        for(int i=0;i<4;i++){float ry=136+i*66;if(l->host&&setting_sel==i){vita2d_draw_rectangle(634,ry,288,58,RGBA8(51,113,205,255));net70_draw_outline(634,ry,288,58,RGBA8(139,202,255,255));}net70_text(650,ry+22,0.58f,RGBA8(166,186,211,255),labels[i]);net70_text(650,ry+50,0.76f,RGBA8(250,252,255,255),values[i]);}
        net70_text(24,502,0.70f,RGBA8(224,234,247,255),"L/R Charakter    Quadrat Bereit    Kreis Zurueck");
        if(l->host){net70_text(642,430,0.70f,RGBA8(255,218,125,255),"Hoch/Runter: Option");net70_text(642,457,0.70f,RGBA8(205,222,242,255),"Links/Rechts: Wert");}
        vita2d_end_drawing();vita2d_swap_buffers();
    }
}
