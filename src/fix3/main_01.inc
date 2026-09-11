#include <psp2/apputil.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/net/netctl.h>
#include <psp2/netcheck_dialog.h>
#include <psp2/ime_dialog.h>
#include <psp2/sysmodule.h>
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
#include "lf2_rng.h"
#include "stage_data.h"
#include "log.h"
#include "ui_font.h"
#include "net70/adhoc_net.h"
#include "net70/pc_net.h"

#define SCREEN_W 960
#define SCREEN_H 544
const SceSize sceUserMainThreadStackSize = 512 * 1024;

static int lf2_pgf_draw_text_shadow(vita2d_pgf *font,int x,int y,unsigned color,float scale,const char *text){
    if(!text)return 0;
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

static const char *check_progress_kind(const char *path){
    if(!path||!path[0])return "Spieldaten";
    if(!strncmp(path,"bg/",3))return "Hintergrund";
    if(!strncmp(path,"data/",5)){
        size_t n=strlen(path);if(n>=4&&!strcmp(path+n-4,".wav"))return "Sound";
        if(n>=4&&!strcmp(path+n-4,".dat"))return "Charakter-/Objektdaten";
        return "Spieldaten";
    }
    if(!strncmp(path,"recording/",10))return "Aufzeichnung";
    if(!strncmp(path,"sprite/",7))return "Grafik";
    return "Spieldatei";
}

static void check_progress(int done,int total,const char *path,int ok,void *userdata){
    (void)userdata;float p=total?((float)done/(float)total):0.0f;if(p<0)p=0;if(p>1)p=1;
    vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_check_bg);
    const float bx=105,by=430,bw=750,bh=24;
    vita2d_draw_rectangle(bx-2,by-2,bw+4,bh+4,RGBA8(225,235,250,235));
    vita2d_draw_rectangle(bx,by,bw,bh,RGBA8(13,29,55,245));
    vita2d_draw_rectangle(bx,by,bw*p,bh,ok?RGBA8(72,176,238,255):RGBA8(225,75,92,255));
    if(g_font){
        char short_path[96];snprintf(short_path,sizeof(short_path),"%s",path&&path[0]?path:"Initialisiere Spieldaten...");
        vita2d_pgf_draw_text(g_font,105,375,RGBA8(250,252,255,255),0.86f,"Little Fighter 2 wird geladen");
        vita2d_pgf_draw_textf(g_font,105,405,RGBA8(225,238,252,255),0.66f,"Lade / pruefe: %s",check_progress_kind(path));
        vita2d_pgf_draw_textf(g_font,105,425,RGBA8(205,222,244,245),0.54f,"%s",short_path);
        vita2d_pgf_draw_textf(g_font,760,477,RGBA8(245,249,255,255),0.60f,"%d/%d  %d%%",done,total,(int)(p*100.0f+0.5f));
    }
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
