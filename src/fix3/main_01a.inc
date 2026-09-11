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
#include "replay_ref.h"
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
