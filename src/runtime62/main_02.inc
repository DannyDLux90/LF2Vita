        if(pr&(SCE_CTRL_LEFT|SCE_CTRL_LTRIGGER)){page=(page+total-1)%total;lf2_logf("UI","manual page=%d",page);}
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_manual_bg);vita2d_draw_rectangle(28,24,904,486,RGBA8(0,8,37,218));
        if(g_font){vita2d_pgf_draw_text(g_font,55,66,RGBA8(255,255,255,255),1.18f,"LITTLE FIGHTER 2 — ANLEITUNG");vita2d_pgf_draw_textf(g_font,790,64,RGBA8(155,205,255,255),0.70f,"%d / %d",page+1,total);
            if(page==0){
                vita2d_pgf_draw_text(g_font,60,116,RGBA8(90,195,255,255),1.0f,"Vita-Steuerung");
                const char *l[]={"Stick / D-Pad              Bewegen","Stick weit                  Rennen","□                           Angriff","×                           Sprung","○ oder L                    Abwehr / Kombotaste","△ oder R                    Primär-Spezial","START                       Pause","SELECT                      Anleitung (im Menü)"};
                for(int i=0;i<8;i++)vita2d_pgf_draw_text(g_font,80,158+i*36,RGBA8(235,242,255,255),0.78f,l[i]);
            }else if(page==1){
                vita2d_pgf_draw_text(g_font,60,116,RGBA8(90,195,255,255),1.0f,"Spielprinzip & Kombos");
                const char *l[]={"Besiege den Gegner, bevor deine HP auf 0 fallen.","Die blaue MP-Leiste bezahlt Spezialtechniken und regeneriert.","LF2 hat Tiefe: Oben/Unten bewegt dich auf der Z-Achse.","Original-Kombos: ○ oder L halten + Richtung + □/×.","'Vor' bedeutet immer in Blickrichtung deines Kämpfers.","× + □ löst bei passenden Kämpfern hit_ja aus.","LF2.NET-Start schaltet versteckte Kämpfer und CRAZY-CPU frei.","Log bei Fehlern: ux0:data/LF2V00001/lf2.log"};
                for(int i=0;i<8;i++)vita2d_pgf_draw_text(g_font,60,160+i*37,RGBA8(235,242,255,255),0.72f,l[i]);
            }else{
                int gi=page-2;const guide_char_t *g=&g_guides[gi];draw_portrait_index(gi,62,132,1.35f);
                vita2d_pgf_draw_textf(g_font,250,132,RGBA8(255,220,85,255),1.12f,"%s",g->name);vita2d_pgf_draw_text(g_font,250,166,RGBA8(150,205,255,255),0.69f,"Spezialkommandos aus den 2.00a-DAT-Dateien:");
                for(int i=0;i<g->count;i++)vita2d_pgf_draw_text(g_font,250,215+i*46,RGBA8(245,248,255,255),0.75f,g->moves[i]);
                vita2d_pgf_draw_text(g_font,250,460,RGBA8(175,195,225,255),0.62f,"Hinweis: Sonderwirkungen/Projektile werden schrittweise nativ portiert.");
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
    vita2d_draw_rectangle(x,y,w,2,border);vita2d_draw_rectangle(x,y+h-2,w,2,border);vita2d_draw_rectangle(x,y,2,h,border);vita2d_draw_rectangle(x+w-2,y,2,h,border);
    if(enabled&&roster>=0){
        draw_portrait_index(roster,x+58,y+8,0.82f);
    } else {
        vita2d_draw_rectangle(x+57,y+8,100,100,RGBA8(0,0,0,225));
        if(g_font){lf2_ui_draw_text(x+67,y+48,0.68f,RGBA8(255,255,255,255),"Press Square");lf2_ui_draw_text(x+75,y+73,0.68f,RGBA8(255,255,255,255),"to join!");}
    }
    vita2d_draw_rectangle(x+8,y+116,w-16,21,RGBA8(0,5,28,225));
    vita2d_draw_rectangle(x+8,y+139,w-16,21,RGBA8(0,5,28,225));
    vita2d_draw_rectangle(x+8,y+162,w-16,21,RGBA8(0,5,28,225));
    if(g_font){
        const char *role=slot==0?"Player 1":(enabled?"Computer":"Join?");
        const char *fighter=(enabled&&roster>=0)?lf2_roster[roster].name:"-";
        const char *tn=enabled?ui_team_name(team):"-";
        if(lf2_ui_font_ready()){
            lf2_ui_draw_textf(x+12,y+116,0.68f,RGBA8(235,242,255,255),"Player: %s",role);
            lf2_ui_draw_textf(x+12,y+139,0.68f,RGBA8(255,255,255,255),"Fighter: %s",fighter);
            lf2_ui_draw_textf(x+12,y+162,0.68f,RGBA8(255,255,255,255),"Team: %s",tn);
        } else {
            vita2d_pgf_draw_textf(g_font,(int)x+12,(int)y+132,RGBA8(245,247,255,255),0.72f,"Fighter: %s",fighter);
        }
    }
}

static void draw_character_selection_base(const vs_setup_t *setup,int active_slot,int active_roster,int active_team,bool cheat){
    vita2d_start_drawing();vita2d_clear_screen();

    /* The original LF2 selection screen is a very dark royal-blue panel with
       eight player cards. Keep the supplied LF2 artwork faintly visible behind it. */
    draw_full_texture(g_menu_bg);vita2d_draw_rectangle(0,0,960,544,RGBA8(7,29,87,224));
    if(lf2_ui_font_ready()){
        lf2_ui_draw_text(300,16,0.92f,RGBA8(255,255,255,255),"Character Selection");
        if(cheat)lf2_ui_draw_text(775,19,0.68f,RGBA8(255,225,90,255),"LF2.NET");
    } else if(g_font){vita2d_pgf_draw_text(g_font,318,44,RGBA8(255,255,255,255),1.16f,"Character Selection");}
    for(int i=0;i<8;i++)draw_lf2_slot(setup,i,active_slot,active_roster,active_team);
}

static int select_slot(bool cheat,vs_setup_t *setup,int slot,int *roster_out,int *team_out){
    int vis[32],n=build_visible_indices(cheat,vis,32),vpos=0,team=*team_out;
