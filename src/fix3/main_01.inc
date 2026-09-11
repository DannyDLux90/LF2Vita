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
    /* fix3: deliberately quiet startup indicator. No translucent panel or
       frame; only a two-pixel progress line and small status text. */
    vita2d_draw_rectangle(210,451,540,2,RGBA8(8,18,38,105));
    vita2d_draw_rectangle(210,451,540*p,2,ok?RGBA8(105,185,225,185):RGBA8(220,90,100,205));
    if(g_font){
        vita2d_pgf_draw_text(g_font,210,438,RGBA8(220,232,244,190),0.58f,"Spieldateien werden geprüft");
        vita2d_pgf_draw_textf(g_font,758,454,RGBA8(205,220,235,180),0.52f,"%d%%",(int)(p*100.0f+0.5f));
    }
    (void)path;
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
