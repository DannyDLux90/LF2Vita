static void net_draw_avatar_badge(float x,float y,const char *name,uint32_t hash,bool selected){
    /* AdHoc must also work without PSN/Internet. The protocol carries a stable
       profile-avatar hash; until a remote thumbnail is cached we render a PS
       profile badge with the player's initial in the same fixed avatar cell. */
    unsigned base=hash?hash:0x6276A8u;unsigned r=70+((base>>16)&63),g=92+((base>>8)&63),b=145+(base&47);
    vita2d_draw_rectangle(x,y,38,38,RGBA8(r,g,b,255));
    vita2d_draw_rectangle(x+3,y+3,32,32,selected?RGBA8(18,41,95,245):RGBA8(10,26,65,245));
    if(lf2_ui_font_ready()){
        char c[2]={(name&&name[0])?name[0]:'?',0};
        lf2_ui_draw_text(x+13,y+6,0.72f,RGBA8(255,255,255,255),c);
        lf2_ui_draw_text(x+25,y+24,0.36f,RGBA8(160,205,255,255),"PS");
    }
}

static void net_draw_ping(float x,float y,uint16_t ping){
    int bars=lf2_net_ping_bars(ping);unsigned active=RGBA8(130,145,165,160);
    if(bars>=5)active=RGBA8(75,220,110,255);else if(bars==4)active=RGBA8(145,220,80,255);else if(bars==3)active=RGBA8(240,205,70,255);else if(bars==2)active=RGBA8(245,145,55,255);else if(bars==1)active=RGBA8(235,75,70,255);
    for(int i=0;i<5;i++){float h=6+i*4;vita2d_draw_rectangle(x+i*8,y+22-h,5,h,i<bars?active:RGBA8(75,91,120,135));}
    if(lf2_ui_font_ready()){if(ping==0xffff)lf2_ui_draw_text(x+45,y+3,0.44f,RGBA8(185,202,225,220),"-- ms");else lf2_ui_draw_textf(x+45,y+3,0.44f,RGBA8(220,232,248,245),"%u ms",(unsigned)ping);}
}

static int net_next_roster(bool cheat,int current,int dir){
    int vis[32],n=build_visible_indices(cheat,vis,32),pos=0;if(n<=0)return current;
    for(int i=0;i<n;i++)if(vis[i]==current){pos=i;break;}
    pos=(pos+(dir>0?1:n-1))%n;return vis[pos];
}

static void net_draw_player_card(float x,float y,const lf2_net_player_t *p,int slot,bool local){
    vita2d_draw_rectangle(x,y,270,82,local?RGBA8(35,77,151,238):RGBA8(6,31,86,224));
    if(!p||!p->present){if(lf2_ui_font_ready())lf2_ui_draw_textf(x+18,y+27,0.60f,RGBA8(135,160,195,220),"Slot %d - frei",slot+1);return;}
    draw_portrait_index(p->character,x+8,y+7,0.56f);net_draw_avatar_badge(x+79,y+8,p->name,p->avatar_hash,local);
    if(lf2_ui_font_ready()){
        lf2_ui_draw_textf(x+123,y+8,0.62f,RGBA8(255,255,255,255),"%s",p->name);
        lf2_ui_draw_textf(x+123,y+33,0.54f,RGBA8(195,220,255,255),"%s",(p->character<lf2_roster_count)?lf2_roster[p->character].name:"?");
        lf2_ui_draw_text(x+123,y+56,0.48f,p->ready?RGBA8(105,235,135,255):RGBA8(230,205,120,255),p->ready?"BEREIT":"waehlt...");
    }
}

