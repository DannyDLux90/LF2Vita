#include <psp2/apputil.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/types.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vita2d.h>

#include "asset_pack.h"
#include "audio.h"
#include "filecheck.h"
#include "game.h"
#include "lf2_data.h"
#include "stage_data.h"
#include "log.h"
#include "ui_font.h"

#define SCREEN_W 960
#define SCREEN_H 544
const SceSize sceUserMainThreadStackSize = 512 * 1024;

static int lf2_pgf_draw_text_shadow(vita2d_pgf *font,int x,int y,unsigned color,float scale,const char *text){
    if(!text)return 0;
    /* Legacy menu call sites used PGF baseline coordinates. Route them through
       the same Vita system PVF as the rest of the UI, while preserving their
       approximate baseline and keeping PGF only as an emergency fallback. */
    if(lf2_ui_font_ready()){
        lf2_ui_draw_text((float)x,(float)y-24.0f*scale,scale,color,text);
        return (int)(lf2_ui_text_width(scale,text)+0.5f);
    }
    if(!font)return 0;
    vita2d_pgf_draw_text(font,x+2,y+2,RGBA8(0,0,0,220),scale,text);
    return vita2d_pgf_draw_text(font,x,y,color,scale,text);
}
static int lf2_pgf_draw_textf_shadow(vita2d_pgf *font,int x,int y,unsigned color,float scale,const char *fmt,...){
    char buf[512];va_list ap;va_start(ap,fmt);vsnprintf(buf,sizeof(buf),fmt,ap);va_end(ap);
    return lf2_pgf_draw_text_shadow(font,x,y,color,scale,buf);
}
#define vita2d_pgf_draw_text  lf2_pgf_draw_text_shadow
#define vita2d_pgf_draw_textf lf2_pgf_draw_textf_shadow

static vita2d_pgf *g_font;
static vita2d_texture *g_check_bg,*g_menu_bg,*g_manual_bg,*g_portraits;

typedef enum { LAUNCH_NORMAL=0, LAUNCH_CHEAT=1 } launch_mode_t;
typedef struct { const char *name; const char *moves[6]; int count; } guide_char_t;

static const guide_char_t g_guides[] = {
 {"Deep",{"○ + Vor + □  — Blast","○ + Unten + □ — Jump Sword","○ + Vor + ×    — Dash Sword","○ + Oben + ×   — Jump Sword"},4},
 {"John",{"○ + Vor + □  — Energy Disc","○ + Oben + □ — Biscuit","○ + Vor + ×    — Force Field","○ + Oben + ×   — Heal Other","○ + Unten + ×  — Heal Self"},5},
 {"Henry",{"○ + Vor + □  — Blast Push","○ + Vor + ×    — Super Arrow","○ + Oben + ×   — Flute","× + □      — Five Arrows"},4},
 {"Rudolf",{"○ + Vor + □  — Punch","○ + Vor + ×    — Jump Sword","○ + Oben + ×   — Disappear","○ + Unten + ×  — Clone","× + □      — Transform"},5},
 {"Louis",{"○ + Vor + □  — Blast Push","○ + Vor + ×    — 1000 Foot","○ + Oben + ×   — Catch","× + □      — Transform"},4},
 {"Firen",{"○ + Vor + □  — Fire Ball","○ + Vor + ×    — Burn Run","○ + Oben + ×   — Explosion","○ + Unten + ×  — Flame"},4},
 {"Freeze",{"○ + Vor + □  — Ice Ball","○ + Vor + ×    — Down Punch","○ + Oben + ×   — Whirlwind","○ + Unten + ×  — Ice Sword"},4},
 {"Dennis",{"○ + Vor + □  — Energy Ball","○ + Oben + □ — Chasing Ball","○ + Unten + □— Many Kicks","○ + Vor + ×    — Chasing Kick"},4},
 {"Woody",{"○ + Vor + □  — Energy Ball","○ + Oben + □ — Super Punch","○ + Unten + □— Flip Kick","○ + Vor + ×    — Fly Crash","○ + Oben/Unten + × — Teleport"},5},
 {"Davis",{"○ + Vor + □  — Energy Ball","○ + Oben + □ — Dragon Punch","○ + Unten + □— Many Punch","○ + Oben + ×   — Jump Hit"},4}
};

typedef struct { const char *name; const char *asset; } stage_entry_t;
static const stage_entry_t g_stages[] = {
 {"Stanley Prison",    "app0:/assets/stages/00_sp.png"},
 {"Brokeback Cliff",  "app0:/assets/stages/01_bc.png"},
 {"CUHK",              "app0:/assets/stages/02_cuhk.png"},
 {"HK Coliseum",       "app0:/assets/stages/03_hkc.png"},
 {"Tai Hom Village",   "app0:/assets/stages/04_thv.png"},
 {"Queen's Island",    "app0:/assets/stages/05_qi.png"},
 {"The Great Wall",    "app0:/assets/stages/06_gw.png"},
 {"Lion Forest",       "app0:/assets/stages/07_lf.png"},
 {"Forbidden Tower",   "app0:/assets/stages/08_ft.png"}
};
#define STAGE_COUNT ((int)(sizeof(g_stages)/sizeof(g_stages[0])))

static uint32_t sync_buttons(void){SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);return p.buttons;}

static void draw_full_texture(vita2d_texture *t){if(t)vita2d_draw_texture_scale(t,0,0,960.0f/(float)vita2d_texture_get_width(t),544.0f/(float)vita2d_texture_get_height(t));}

static void check_progress(int done,int total,const char *path,int ok,void *userdata){
    (void)userdata;float p=total?((float)done/(float)total):0.0f;
    vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_check_bg);
    vita2d_draw_rectangle(0,353,960,191,RGBA8(1,10,34,220));vita2d_draw_rectangle(115,422,730,30,RGBA8(4,12,35,245));
    vita2d_draw_rectangle(119,426,722*p,22,ok?RGBA8(46,184,255,255):RGBA8(240,65,75,255));
    if(g_font){vita2d_pgf_draw_text(g_font,116,395,RGBA8(245,250,255,255),1.03f,"Spieldateien werden geprüft...");vita2d_pgf_draw_textf(g_font,855,449,RGBA8(255,255,255,255),1.0f,"%d%%",(int)(p*100.0f+0.5f));vita2d_pgf_draw_textf(g_font,116,486,RGBA8(185,210,240,255),0.68f,"%d / %d   %s",done,total,path?path:"");}
    vita2d_end_drawing();vita2d_swap_buffers();
}

static int wait_message(const char *title,const char *message){
    uint32_t prev=0;for(;;){SceCtrlData pad={0};sceCtrlPeekBufferPositive(0,&pad,1);uint32_t pressed=pad.buttons&~prev;prev=pad.buttons;
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);vita2d_draw_rectangle(95,135,770,280,RGBA8(0,7,28,230));
        if(g_font){vita2d_pgf_draw_text(g_font,140,190,RGBA8(255,255,255,255),1.15f,title);vita2d_pgf_draw_text(g_font,140,250,RGBA8(205,220,245,255),0.76f,message);vita2d_pgf_draw_textf(g_font,140,320,RGBA8(150,195,245,255),0.64f,"Diagnose-Log: %s",lf2_log_path());vita2d_pgf_draw_text(g_font,140,375,RGBA8(130,190,255,255),0.72f,"KREUZ / KREIS: zurück");}
        vita2d_end_drawing();vita2d_swap_buffers();if(pressed&(SCE_CTRL_CROSS|SCE_CTRL_CIRCLE))return 0;}
}

static launch_mode_t detect_launch_mode(void){
    launch_mode_t mode=LAUNCH_NORMAL;SceAppUtilInitParam init={0};SceAppUtilBootParam boot={0};
    int rc=sceAppUtilInit(&init,&boot);lf2_logf("INFO","sceAppUtilInit rc=0x%08X",rc);
    if(rc>=0){SceAppUtilAppEventParam ev;memset(&ev,0,sizeof(ev));rc=sceAppUtilReceiveAppEvent(&ev);lf2_logf("INFO","ReceiveAppEvent rc=0x%08X type=0x%X",rc,ev.type);
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
                const char *l[]={"Stick / D-Pad              Bewegen","Stick weit                  Rennen","□                           Angriff / Item benutzen / werfen","L                           Item aufnehmen","□ halten mit Milch/Bier     Trinken","×                           Sprung","○                           Abwehr / Kombotaste","△ oder R                    Primär-Spezial","START                       Pause","SELECT                      Anleitung (im Menü)"};
                for(int i=0;i<10;i++)vita2d_pgf_draw_text(g_font,80,151+i*31,RGBA8(235,242,255,255),0.72f,l[i]);
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
    for(int i=0;i<n;i++)if(vis[i]==*roster_out){vpos=i;break;}
    uint32_t prev=sync_buttons();lf2_log_stage("character_select:slot_open","slot=%d role=%s",slot,slot==0?"Player1":"Computer");
    for(;;){
        SceCtrlData pad={0};sceCtrlPeekBufferPositive(0,&pad,1);uint32_t pr=pad.buttons&~prev;prev=pad.buttons;
        if(pr&SCE_CTRL_LEFT){vpos=(vpos+n-1)%n;ui_join();}
        if(pr&SCE_CTRL_RIGHT){vpos=(vpos+1)%n;ui_join();}
        if(pr&SCE_CTRL_UP){team=(team+4)%5;ui_join();}
        if(pr&SCE_CTRL_DOWN){team=(team+1)%5;ui_join();}
        if(pr&SCE_CTRL_CIRCLE){ui_cancel();return 0;}
        if(pr&(SCE_CTRL_SQUARE|SCE_CTRL_CROSS)){
            *roster_out=vis[vpos];*team_out=team;ui_ok();
            lf2_log_stage("character_select:slot_done","slot=%d fighter=%s team=%s",slot,lf2_roster[*roster_out].name,ui_team_name(team));return 1;
        }
        draw_character_selection_base(setup,slot,vis[vpos],team,cheat);
        vita2d_draw_rectangle(110,505,740,34,RGBA8(0,5,28,235));
        if(lf2_ui_font_ready())lf2_ui_draw_text(130,510,0.68f,RGBA8(255,255,255,255),"Left/Right Fighter   Up/Down Team   X/Square OK   Circle Back");
        vita2d_end_drawing();vita2d_swap_buffers();
    }
}

static int select_cpu_count_original(vs_setup_t *setup,bool cheat){
    int count=setup->cpu_count;uint32_t prev=sync_buttons();lf2_log_stage("character_select:com_count","open=1");
    for(;;){
        SceCtrlData pad={0};sceCtrlPeekBufferPositive(0,&pad,1);uint32_t pr=pad.buttons&~prev;prev=pad.buttons;
        if(pr&(SCE_CTRL_LEFT|SCE_CTRL_DOWN)){count=count>1?count-1:LF2_MAX_CPUS;ui_join();}
        if(pr&(SCE_CTRL_RIGHT|SCE_CTRL_UP)){count=count<LF2_MAX_CPUS?count+1:1;ui_join();}
        if(pr&SCE_CTRL_CIRCLE){ui_cancel();return 0;}
        if(pr&(SCE_CTRL_SQUARE|SCE_CTRL_CROSS)){setup->cpu_count=count;ui_ok();lf2_log_stage("character_select:com_count","count=%d",count);return 1;}
        vs_setup_t preview=*setup;preview.cpu_count=count;
        for(int i=0;i<count;i++)if(preview.cpus[i]<0)preview.cpus[i]=0;
        draw_character_selection_base(&preview,-1,-1,0,cheat);
        vita2d_draw_rectangle(18,17,275,165,RGBA8(25,67,145,242));
        if(g_font){
            vita2d_pgf_draw_text(g_font,36,48,RGBA8(255,255,255,255),0.80f,"COM Players");
            vita2d_pgf_draw_textf(g_font,43,105,RGBA8(255,225,90,255),1.35f,"%d",count);
            vita2d_pgf_draw_text(g_font,90,104,RGBA8(235,242,255,255),0.66f,"Computer");
            vita2d_pgf_draw_text(g_font,36,150,RGBA8(215,230,255,255),0.63f,"←/→ Anzahl    □/× OK");
        }
        vita2d_end_drawing();vita2d_swap_buffers();
    }
}

static const char *stage_option_name(int stage){return stage<0?"Random":g_stages[stage].name;}

static int vs_options(vs_setup_t *setup,bool cheat){
    static const char *labels[]={"Fight!","Reset All","Random","Background","Difficulty","Exit"};
    int sel=0;uint32_t prev=sync_buttons();
    for(;;){
        SceCtrlData pad={0};sceCtrlPeekBufferPositive(0,&pad,1);uint32_t pr=pad.buttons&~prev;prev=pad.buttons;
