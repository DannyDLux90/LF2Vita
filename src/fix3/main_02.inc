        if(rc>=0&&ev.type==0x05){char buf[2048]={0};rc=sceAppUtilAppEventParseLiveArea(&ev,buf);lf2_logf("INFO","LiveArea args rc=0x%08X value=%s",rc,buf);if(rc>=0){if(strstr(buf,"-lf2net"))mode=LAUNCH_CHEAT;}}
        sceAppUtilShutdown();}
    return mode;
}

static void draw_portrait_index(int roster_idx,float x,float y,float scale){
    if(!g_portraits||roster_idx<0)return;float sx=(float)((roster_idx%6)*120),sy=(float)((roster_idx/6)*120);vita2d_draw_texture_part_scale(g_portraits,x,y,sx,sy,120,120,scale,scale);
}


static void manual_view(void){
    bool loaded_here=false;if(!g_manual_bg){g_manual_bg=vita2d_load_PNG_file("app0:/assets/manual_bg.png");loaded_here=true;lf2_logf("INFO","manual background lazy-load=%p",g_manual_bg);}
    int page=0,total=2+(int)(sizeof(g_guides)/sizeof(g_guides[0]));uint32_t prev=0;lf2_log_stage("manual:open","pages=%d",total);
    for(;;){SceCtrlData pad={0};sceCtrlPeekBufferPositive(0,&pad,1);uint32_t pr=pad.buttons&~prev;prev=pad.buttons;
        if(pr&(SCE_CTRL_CIRCLE|SCE_CTRL_SELECT)){lf2_logf("UI","manual close page=%d",page);break;}
        if(pr&(SCE_CTRL_RIGHT|SCE_CTRL_RTRIGGER)){page=(page+1)%total;lf2_logf("UI","manual page=%d",page);}
        if(pr&(SCE_CTRL_LEFT|SCE_CTRL_LTRIGGER)){page=(page+total-1)%total;lf2_logf("UI","manual page=%d",page);}
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_manual_bg);vita2d_draw_rectangle(28,24,904,486,RGBA8(0,8,37,218));
        if(g_font){vita2d_pgf_draw_text(g_font,55,66,RGBA8(255,255,255,255),1.18f,"LITTLE FIGHTER 2 — ANLEITUNG");vita2d_pgf_draw_textf(g_font,790,64,RGBA8(155,205,255,255),0.70f,"%d / %d",page+1,total);
            if(page==0){
                vita2d_pgf_draw_text(g_font,60,116,RGBA8(90,195,255,255),1.0f,"Vita-Steuerung");
                const char *l[]={"Stick / D-Pad              Bewegen","Stick weit                  Rennen","□                           Angriff / Item benutzen","L                           Item aufnehmen","L + □                       Gehaltenes Item werfen","□ halten mit Milch/Bier     Trinken","○                           Abwehr / Trinken abbrechen","×                           Sprung","△                           Angriff-Spezialshortcut","R                           Sprung-Spezialshortcut","START                       Pause"};
                for(int i=0;i<11;i++)vita2d_pgf_draw_text(g_font,80,146+i*29,RGBA8(235,242,255,255),0.69f,l[i]);
            }else if(page==1){
                vita2d_pgf_draw_text(g_font,60,116,RGBA8(90,195,255,255),1.0f,"Spielprinzip & Kombos");
                const char *l[]={"Besiege den Gegner, bevor deine HP auf 0 fallen.","Die blaue MP-Leiste bezahlt Spezialtechniken und regeneriert.","LF2 hat Tiefe: Oben/Unten bewegt dich auf der Z-Achse.","Original-Kombos: ○ halten + Richtung + □/×.","'Vor' bedeutet immer in Blickrichtung deines Kämpfers.","× + □ löst bei passenden Kämpfern hit_ja aus.","LF2.NET-Start schaltet versteckte Kämpfer und CRAZY-CPU frei.","Log bei Fehlern: ux0:data/LF2V00001/lf2.log"};
                for(int i=0;i<8;i++)vita2d_pgf_draw_text(g_font,60,160+i*37,RGBA8(235,242,255,255),0.72f,l[i]);
            }else{
                int gi=page-2;const guide_char_t *g=&g_guides[gi];draw_portrait_index(gi,62,132,1.35f);
                vita2d_pgf_draw_textf(g_font,250,132,RGBA8(255,220,85,255),1.12f,"%s",g->name);vita2d_pgf_draw_text(g_font,250,166,RGBA8(150,205,255,255),0.69f,"Spezialkommandos aus den 2.00a-DAT-Dateien:");
                for(int i=0;i<g->count;i++)vita2d_pgf_draw_text(g_font,250,215+i*46,RGBA8(245,248,255,255),0.75f,g->moves[i]);
                vita2d_pgf_draw_text(g_font,250,460,RGBA8(175,195,225,255),0.63f,"Projektile, Pfeile und Energiebaelle werden aus den 2.00a-DAT-Dateien erzeugt.");
            }
            vita2d_pgf_draw_text(g_font,54,500,RGBA8(145,190,245,255),0.65f,"L/R oder Links/Rechts: Seite     KREIS/SELECT: schließen");}
        vita2d_end_drawing();vita2d_swap_buffers();
    }
    lf2_log_stage("manual:closed","");if(loaded_here&&g_manual_bg){vita2d_wait_rendering_done();vita2d_free_texture(g_manual_bg);g_manual_bg=NULL;lf2_log_memory("manual bg freed");}
}

static int build_visible_indices(bool cheat,int *out,int cap){
    int n=0;for(int i=0;i<lf2_roster_count&&n<cap;i++)if(cheat||!lf2_roster[i].hidden)out[n++]=i;return n;
}

static void ui_ok(void){ if(lf2_audio_ready())lf2_audio_play("data/m_ok.wav"); }
static void ui_cancel(void){ if(lf2_audio_ready())lf2_audio_play("data/m_cancel.wav"); }
static void ui_join(void){ if(lf2_audio_ready())lf2_audio_play("data/m_join.wav"); }

static const char *ui_team_name(int t){
    static const char *n[]={"Independent","Team 1","Team 2","Team 3","Team 4"};
    return (t>=0&&t<=4)?n[t]:n[0];
}
static const char *ui_diff_name(int d){
    static const char *n[]={"Easy","Normal","Difficult","CRAZY!"};
    return (d>=0&&d<=3)?n[d]:n[1];
}

typedef struct {
    int p1,p1_team;
    int cpu_count;
    int cpus[LF2_MAX_CPUS];
    int cpu_teams[LF2_MAX_CPUS];
    int stage;      /* -1 = Random */
    int difficulty;
} vs_setup_t;

static void setup_defaults(vs_setup_t *s){
    memset(s,0,sizeof(*s));s->p1=0;s->p1_team=0;s->cpu_count=1;s->cpus[0]=1;s->stage=-1;s->difficulty=LF2_DIFF_NORMAL;
}

static int setup_roster_at(const vs_setup_t *s,int slot){
    if(slot==0)return s->p1;
    if(slot>=1&&slot<=s->cpu_count)return s->cpus[slot-1];
    return -1;
}
static int setup_team_at(const vs_setup_t *s,int slot){
    if(slot==0)return s->p1_team;
    if(slot>=1&&slot<=s->cpu_count)return s->cpu_teams[slot-1];
    return 0;
}

static void draw_lf2_slot(const vs_setup_t *setup,int slot,int active_slot,int active_roster,int active_team){
    int col=slot%4,row=slot/4;float x=28.0f+col*232.0f,y=72.0f+row*220.0f,w=214.0f,h=194.0f;
    int roster=slot==active_slot?active_roster:setup_roster_at(setup,slot);
    int team=slot==active_slot?active_team:setup_team_at(setup,slot);
    int enabled=(slot==0)||(slot<=setup->cpu_count);
    unsigned border=slot==active_slot?RGBA8(145,205,255,255):RGBA8(58,103,186,255);
    vita2d_draw_rectangle(x,y,w,h,RGBA8(10,38,101,235));
