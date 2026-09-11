#include "ui_font.h"
#include "log.h"

#include <stdarg.h>
#include <stdio.h>

/* v0.65 switches the gameplay UI from the hand-made 12x16 bitmap atlas to
   the Vita's own scalable system PVF.  The previous font was crisp in a PC
   screenshot but too thin/small on the Vita's 5-inch panel. */
static vita2d_pvf *g_ui_font;

int lf2_ui_font_init(void) {
    if(g_ui_font)return 0;
    g_ui_font=vita2d_load_default_pvf();
    if(!g_ui_font){lf2_logf("ERROR","UI system PVF load failed");return -1;}
    lf2_logf("INFO","UI font=Vita default PVF with 1px outline");
    return 0;
}

void lf2_ui_font_shutdown(void) {
    if(!g_ui_font)return;
    vita2d_wait_rendering_done();
    vita2d_free_pvf(g_ui_font);g_ui_font=NULL;
}

int lf2_ui_font_ready(void){return g_ui_font!=NULL;}

/* Existing UI call sites use scales tuned for the old 12x16 font.  A 0.90
   factor keeps the same layouts while producing a substantially taller,
   anti-aliased system glyph. */
static float pvf_scale(float scale){return scale*1.05f;}

float lf2_ui_text_width(float scale,const char *text){
    if(!g_ui_font||!text)return 0.0f;
    return (float)vita2d_pvf_text_width(g_ui_font,pvf_scale(scale),text);
}

void lf2_ui_draw_text(float x,float y,float scale,unsigned color,const char *text){
    if(!g_ui_font||!text)return;
    float sc=pvf_scale(scale);
    /* PVF y is a baseline. Preserve the old top-left API and add a strong
       outline that survives the bright LF2 backgrounds. */
    int bx=(int)(x+0.5f), by=(int)(y+24.0f*scale+0.5f);
    unsigned edge=RGBA8(0,0,0,225);
    static const int off[4][2]={{-1,0},{1,0},{0,-1},{0,1}};
    for(int i=0;i<4;i++)vita2d_pvf_draw_text(g_ui_font,bx+off[i][0],by+off[i][1],edge,sc,text);
    vita2d_pvf_draw_text(g_ui_font,bx,by,color,sc,text);
}

void lf2_ui_draw_textf(float x,float y,float scale,unsigned color,const char *fmt,...){
    char buf[512];va_list ap;va_start(ap,fmt);vsnprintf(buf,sizeof(buf),fmt,ap);va_end(ap);
    lf2_ui_draw_text(x,y,scale,color,buf);
}
