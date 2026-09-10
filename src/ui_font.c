#include "ui_font.h"
#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ui_font_bits.inc"

#define GLYPH_W 12
#define GLYPH_H 16
#define CELL_W 16
#define CELL_H 20
#define COLS 16
#define ROWS 6
#define FIRST_CHAR 32
#define LAST_CHAR 126
#define ADVANCE_X 12.0f
#define LINE_H 20.0f

static vita2d_texture *g_ui_font;

static void put_px(uint32_t *dst,unsigned stride,int w,int h,int x,int y,uint32_t c){
    if(x>=0&&y>=0&&x<w&&y<h)dst[(size_t)y*stride+(size_t)x]=c;
}

int lf2_ui_font_init(void) {
    if (g_ui_font) return 0;
    const int aw=COLS*CELL_W, ah=ROWS*CELL_H;
    g_ui_font=vita2d_create_empty_texture((unsigned)aw,(unsigned)ah);
    if(!g_ui_font){lf2_logf("ERROR","UI bitmap font texture allocation failed");return -1;}
    uint32_t *dst=(uint32_t*)vita2d_texture_get_datap(g_ui_font);
    unsigned stride=vita2d_texture_get_stride(g_ui_font)/4;
    memset(dst,0,(size_t)stride*(size_t)ah*sizeof(uint32_t));

    /* Build a crisp outlined atlas directly in GPU-visible texture memory.
       Only the raster masks are embedded; no TTF/PGF font file is shipped. */
    for(int gi=0;gi<95;gi++){
        int bx=(gi%COLS)*CELL_W+2, by=(gi/COLS)*CELL_H+2;
        for(int y=0;y<GLYPH_H;y++)for(int x=0;x<GLYPH_W;x++)if(g_ui_font_bits[gi][y]&(1u<<x)){
            for(int oy=-1;oy<=1;oy++)for(int ox=-1;ox<=1;ox++)
                put_px(dst,stride,aw,ah,bx+x+ox,by+y+oy,RGBA8(0,0,0,255));
        }
        for(int y=0;y<GLYPH_H;y++)for(int x=0;x<GLYPH_W;x++)if(g_ui_font_bits[gi][y]&(1u<<x))
            put_px(dst,stride,aw,ah,bx+x,by+y,RGBA8(255,255,255,255));
    }
    vita2d_texture_set_filters(g_ui_font,SCE_GXM_TEXTURE_FILTER_POINT,SCE_GXM_TEXTURE_FILTER_POINT);
    lf2_logf("INFO","UI bitmap font ready tex=%p %ux%u glyph=%dx%d",(void*)g_ui_font,
             vita2d_texture_get_width(g_ui_font),vita2d_texture_get_height(g_ui_font),GLYPH_W,GLYPH_H);
    return 0;
}

void lf2_ui_font_shutdown(void) {
    if (!g_ui_font) return;
    vita2d_wait_rendering_done();
    vita2d_free_texture(g_ui_font);
    g_ui_font = NULL;
}

int lf2_ui_font_ready(void) { return g_ui_font != NULL; }

float lf2_ui_text_width(float scale,const char *text){
    if(!text||scale<=0.0f)return 0.0f;
    int cur=0,longest=0;const unsigned char *p=(const unsigned char*)text;
    while(*p){
        if(*p=='\n'){if(cur>longest)longest=cur;cur=0;p++;continue;}
        if(*p<0x80){cur++;p++;}
        else{cur++;if((*p&0xE0)==0xC0)p+=2;else if((*p&0xF0)==0xE0)p+=3;else if((*p&0xF8)==0xF0)p+=4;else p++;}
    }
    if(cur>longest)longest=cur;return (float)longest*ADVANCE_X*scale;
}

static int next_ascii(const unsigned char **pp){
    const unsigned char *p=*pp;if(!*p)return 0;
    if(*p<0x80){*pp=p+1;return *p;}
    if((*p&0xE0)==0xC0)p+=2;else if((*p&0xF0)==0xE0)p+=3;else if((*p&0xF8)==0xF0)p+=4;else p++;
    *pp=p;return '?';
}

static void draw_glyph(float x,float y,float scale,unsigned color,int ch){
    if(!g_ui_font||scale<=0.0f)return;if(ch<FIRST_CHAR||ch>LAST_CHAR)ch='?';
    int idx=ch-FIRST_CHAR,col=idx%COLS,row=idx/COLS;
    float tw=(float)vita2d_texture_get_width(g_ui_font),th=(float)vita2d_texture_get_height(g_ui_font);
    float sx=(float)col*CELL_W,sy=(float)row*CELL_H;
    float u0=(sx+0.5f)/tw,v0=(sy+0.5f)/th,u1=(sx+CELL_W-0.5f)/tw,v1=(sy+CELL_H-0.5f)/th;
    float w=CELL_W*scale,h=CELL_H*scale;
    vita2d_texture_vertex *v=(vita2d_texture_vertex*)vita2d_pool_memalign(6*sizeof(*v),sizeof(*v));if(!v)return;
    v[0]=(vita2d_texture_vertex){x,y,0.5f,u0,v0};v[1]=(vita2d_texture_vertex){x,y+h,0.5f,u0,v1};v[2]=(vita2d_texture_vertex){x+w,y,0.5f,u1,v0};
    v[3]=(vita2d_texture_vertex){x,y+h,0.5f,u0,v1};v[4]=(vita2d_texture_vertex){x+w,y+h,0.5f,u1,v1};v[5]=(vita2d_texture_vertex){x+w,y,0.5f,u1,v0};
    vita2d_draw_array_textured(g_ui_font,SCE_GXM_PRIMITIVE_TRIANGLES,v,6,color);
}

void lf2_ui_draw_text(float x,float y,float scale,unsigned color,const char *text){
    if(!g_ui_font||!text)return;float start=x;const unsigned char *p=(const unsigned char*)text;
    while(*p){if(*p=='\n'){p++;x=start;y+=LINE_H*scale;continue;}int ch=next_ascii(&p);if(ch!=' ')draw_glyph(x,y,scale,color,ch);x+=ADVANCE_X*scale;}
}

void lf2_ui_draw_textf(float x,float y,float scale,unsigned color,const char *fmt,...){
    char buf[512];va_list ap;va_start(ap,fmt);vsnprintf(buf,sizeof(buf),fmt,ap);va_end(ap);lf2_ui_draw_text(x,y,scale,color,buf);
}
