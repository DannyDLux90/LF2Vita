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

static void net70_draw_player_card(float x,float y,float w,float h,const lf2_net_player_t *p,int slot,bool local,int mode){
    unsigned bg=local?RGBA8(36,87,166,255):RGBA8(25,32,44,255);
    vita2d_draw_rectangle(x,y,w,h,bg);
    if(local)net70_draw_outline(x,y,w,h,RGBA8(120,190,255,255));
    if(!p||!p->present){char line[32];snprintf(line,sizeof(line),"Slot %d  frei",slot+1);net70_text(x+18,y+h*0.5f+8,0.72f,RGBA8(185,200,220,255),line);return;}
    draw_portrait_index(p->character,x+9,y+9,0.56f);
    char line[96];snprintf(line,sizeof(line),"%s",p->name);net70_text(x+76,y+31,0.75f,RGBA8(255,255,255,255),line);
    const char *hero=(p->character<lf2_roster_count)?lf2_roster[p->character].name:"?";
    if(mode==LF2_NET_MODE_2V2){const char *partner=(p->partner_character<lf2_roster_count)?lf2_roster[p->partner_character].name:"?";snprintf(line,sizeof(line),"%s + %s",hero,partner);}
    else if(mode==LF2_NET_MODE_BATTLE){int bf=(p->battle_follower>=10&&p->battle_follower<lf2_roster_count)?p->battle_follower:10;snprintf(line,sizeof(line),"%s + %ux %s",hero,(unsigned)p->battle_count,lf2_roster[bf].name);}
    else snprintf(line,sizeof(line),"%s",hero);
    net70_text(x+76,y+58,0.58f,RGBA8(203,219,239,255),line);
    net70_text(x+w-78,y+58,0.56f,p->ready?RGBA8(117,236,151,255):RGBA8(245,205,111,255),p->ready?"BEREIT":"WAHL");
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

static int net70_run_adhoc_arena_match(uint32_t match_id,int mode,int host_char,int guest_char,int host_partner,int guest_partner,int host_follower,int host_count,int guest_follower,int guest_count,int stage,int difficulty,bool net_cheat){
    int cpus[LF2_MAX_CPUS]={0},teams[LF2_MAX_CPUS]={0},n=0,guest_actor=1;
    const char *label="AdHoc VS";
    if(mode==LF2_NET_MODE_VS){cpus[n]=guest_char;teams[n++]=2;label="AdHoc VS";}
    else if(mode==LF2_NET_MODE_1V1){cpus[n]=guest_char;teams[n++]=2;label="AdHoc 1 vs 1 Championship Final";}
    else if(mode==LF2_NET_MODE_BATTLE){
        if(host_follower<10||host_follower>17)host_follower=10;if(guest_follower<10||guest_follower>17)guest_follower=10;
        if(host_count<0)host_count=0;if(host_count>3)host_count=3;if(guest_count<0)guest_count=0;if(guest_count>3)guest_count=3;
        cpus[n]=guest_char;teams[n++]=2;guest_actor=1;
        for(int i=0;i<host_count&&n<LF2_MAX_CPUS;i++){cpus[n]=host_follower;teams[n++]=1;}
        for(int i=0;i<guest_count&&n<LF2_MAX_CPUS;i++){cpus[n]=guest_follower;teams[n++]=2;}
        label="AdHoc Battle";
    }else if(mode==LF2_NET_MODE_2V2){
        int hp=(host_partner>=0&&host_partner<lf2_roster_count)?host_partner:((host_char+1)%10);
        int gp=(guest_partner>=0&&guest_partner<lf2_roster_count)?guest_partner:((guest_char+1)%10);
        cpus[n]=hp;teams[n++]=1;
        guest_actor=n+1;cpus[n]=guest_char;teams[n++]=2;
        cpus[n]=gp;teams[n++]=2;label="AdHoc 2 vs 2 Championship Final";
    }else return -10;
    vita2d_texture *bg=load_stage_texture(stage);if(!bg)return -5;
    if(lf2_adhoc_match_begin(match_id)<0){free_stage_texture(bg);return -4;}
    lf2_match_options_t opt;net70_native_match_options(&opt,label,0,guest_actor);
    int rc=lf2_run_match_ex(g_font,bg,host_char,1,cpus,teams,n,difficulty,net_cheat,&opt);
    free_stage_texture(bg);lf2_adhoc_match_end();return rc;
}


typedef struct { int fighter; uint8_t owner; } net70_champ_1v1_entry_t; /* owner: 0 CPU, 1 host, 2 guest */
typedef struct { int a,b; uint8_t owner; } net70_champ_2v2_team_t;
