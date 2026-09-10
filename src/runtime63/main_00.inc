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
    if(!font||!text)return 0;
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
 {"Deep",{"○/L + Vor + □  — Blast","○/L + Unten + □ — Jump Sword","○/L + Vor + ×    — Dash Sword","○/L + Oben + ×   — Jump Sword"},4},
 {"John",{"○/L + Vor + □  — Energy Disc","○/L + Oben + □ — Biscuit","○/L + Vor + ×    — Force Field","○/L + Oben + ×   — Heal Other","○/L + Unten + ×  — Heal Self"},5},
 {"Henry",{"○/L + Vor + □  — Blast Push","○/L + Vor + ×    — Super Arrow","○/L + Oben + ×   — Flute","× + □      — Five Arrows"},4},
 {"Rudolf",{"○/L + Vor + □  — Punch","○/L + Vor + ×    — Jump Sword","○/L + Oben + ×   — Disappear","○/L + Unten + ×  — Clone","× + □      — Transform"},5},
 {"Louis",{"○/L + Vor + □  — Blast Push","○/L + Vor + ×    — 1000 Foot","○/L + Oben + ×   — Catch","× + □      — Transform"},4},
 {"Firen",{"○/L + Vor + □  — Fire Ball","○/L + Vor + ×    — Burn Run","○/L + Oben + ×   — Explosion","○/L + Unten + ×  — Flame"},4},
 {"Freeze",{"○/L + Vor + □  — Ice Ball","○/L + Vor + ×    — Down Punch","○/L + Oben + ×   — Whirlwind","○/L + Unten + ×  — Ice Sword"},4},
 {"Dennis",{"○/L + Vor + □  — Energy Ball","○/L + Oben + □ — Chasing Ball","○/L + Unten + □— Many Kicks","○/L + Vor + ×    — Chasing Kick"},4},
 {"Woody",{"○/L + Vor + □  — Energy Ball","○/L + Oben + □ — Super Punch","○/L + Unten + □— Flip Kick","○/L + Vor + ×    — Fly Crash","○/L + Oben/Unten + × — Teleport"},5},
 {"Davis",{"○/L + Vor + □  — Energy Ball","○/L + Oben + □ — Dragon Punch","○/L + Unten + □— Many Punch","○/L + Oben + ×   — Jump Hit"},4}
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
