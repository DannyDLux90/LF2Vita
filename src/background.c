#include "background.h"
#include "asset_pack.h"
#include "lf2_data.h"
#include "log.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vita2d.h>

#define BG_MAX_LAYERS 40
#define BG_MAX_TEXTURES 32
#define BG_PC_VIEW_W 794.0f
#define BG_VITA_VIEW_W 960.0f
#define BG_X_SCALE (BG_VITA_VIEW_W / BG_PC_VIEW_W)

static const unsigned char k_bg_cipher[] = "odBearBecauseHeIsVeryGoodSiuHungIsAGo";

typedef struct {
    char path[128];
    int opaque;
    vita2d_texture *texture;
} bg_texture_t;

typedef struct {
    char path[128];
    int transparency;
    int width;
    int x,y,height;
    int loop,tile;
    int cc,c1,c2;
    int rect_present;
    int rect_value;
    int texture_index;
    int source_order;
} bg_layer_t;

typedef struct {
    int loaded;
    int stage_index;
    int width;
    char name[64];
    uint32_t timer;
    bg_layer_t layers[BG_MAX_LAYERS];
    int layer_count;
    int draw_order[BG_MAX_LAYERS];
    bg_texture_t textures[BG_MAX_TEXTURES];
    int texture_count;
} bg_state_t;

static bg_state_t g_bg={0};

static const char *const g_bg_dat_paths[] = {
    "bg/sys/sp/bg.dat",
    "bg/sys/bc/bg.dat",
    "bg/sys/cuhk/bg.dat",
    "bg/sys/hkc/bg.dat",
    "bg/sys/thv/bg.dat",
    "bg/sys/qi/bg.dat",
    "bg/sys/gw/bg.dat",
    "bg/sys/lf/bg.dat",
    "bg/sys/ft/bg.dat"
};

static int bg_find_int(const char *text,const char *key,int fallback){
    if(!text||!key)return fallback;
    const char *p=strstr(text,key);if(!p)return fallback;p+=strlen(key);
    while(*p==' '||*p=='\t')p++;
    return (int)strtol(p,NULL,10);
}

static void bg_copy_name(const char *text,char *out,size_t cap){
    if(!out||cap<1){return;}out[0]=0;if(!text)return;
    const char *p=strstr(text,"name:");if(!p)return;p+=5;while(*p==' '||*p=='\t')p++;
    size_t n=strcspn(p,"\r\n");if(n>=cap)n=cap-1;memcpy(out,p,n);out[n]=0;
    for(size_t i=0;i<n;i++)if(out[i]=='_')out[i]=' ';
}

static int bg_decode_dat(const char *path,char **text_out){
    if(!path||!text_out)return -1;*text_out=NULL;
    unsigned char *src=NULL;size_t sz=0;int rc=lf2_pak_read(path,(void**)&src,&sz);
    if(rc<0||!src)return -2;if(sz<123){free(src);return -3;}
    size_t outsz=sz-123;char *dst=(char*)malloc(outsz+1);if(!dst){free(src);return -4;}
    const size_t keylen=sizeof(k_bg_cipher)-1;
    for(size_t i=0;i<outsz;i++)dst[i]=(char)((unsigned char)src[i+123]-k_bg_cipher[i%keylen]);
    dst[outsz]=0;free(src);*text_out=dst;return 0;
}

static void bg_force_opaque(vita2d_texture *tex){
    if(!tex)return;uint32_t *p=(uint32_t*)vita2d_texture_get_datap(tex);if(!p)return;
    unsigned w=vita2d_texture_get_width(tex),h=vita2d_texture_get_height(tex),stride=vita2d_texture_get_stride(tex)/4;
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)p[(size_t)y*stride+x]|=0xff000000u;
}

static int bg_texture_get(const char *path,int transparency){
    if(!path||!path[0])return -1;char norm[128];lf2_normalize_relpath(path,norm,sizeof(norm));
    int opaque=transparency?0:1;
    for(int i=0;i<g_bg.texture_count;i++)if(g_bg.textures[i].opaque==opaque&&!strcmp(g_bg.textures[i].path,norm))return i;
    if(g_bg.texture_count>=BG_MAX_TEXTURES)return -2;
    vita2d_texture *tex=lf2_load_bmp_colorkey(norm);if(!tex)return -3;
    if(opaque)bg_force_opaque(tex);
    vita2d_texture_set_filters(tex,SCE_GXM_TEXTURE_FILTER_POINT,SCE_GXM_TEXTURE_FILTER_POINT);
    int idx=g_bg.texture_count++;bg_texture_t *t=&g_bg.textures[idx];memset(t,0,sizeof(*t));
    snprintf(t->path,sizeof(t->path),"%s",norm);t->opaque=opaque;t->texture=tex;return idx;
}

static int bg_extract_layer_path(const char *block,char *out,size_t cap){
    if(!block||!out||cap<2)return -1;out[0]=0;const char *p=block;
    while(*p){
        while(*p=='\r'||*p=='\n')p++;
        const char *e=strpbrk(p,"\r\n");size_t n=e?(size_t)(e-p):strlen(p);
        while(n&&(*p==' '||*p=='\t')){p++;n--;}
        while(n&&(p[n-1]==' '||p[n-1]=='\t'))n--;
        if(n>=3&&(!strncmp(p,"bg\\",3)||!strncmp(p,"bg/",3))){if(n>=cap)n=cap-1;memcpy(out,p,n);out[n]=0;return 0;}
        if(!e)break;p=e;
    }
    return -2;
}

static uint32_t bg_rect_color(int rect){
    switch(rect){
        case 4706:return RGBA8(16,79,16,255);
        case 40179:return RGBA8(159,163,159,255);
        case 29582:return RGBA8(119,119,119,255);
        case 37773:return RGBA8(151,119,111,255);
        case 33580:return RGBA8(135,107,103,255);
        case 25356:return RGBA8(103,103,103,255);
        case 21096:return RGBA8(90,78,75,255);
        case 37770:return RGBA8(154,110,90,255);
        case 16835:return RGBA8(66,56,24,255);
        case 34816:return RGBA8(143,7,7,255);
        default:break;
    }
    int r=(rect>>11)<<3,g=((rect>>6)&31)<<3,b=(rect&31)<<3;
    r+=((r>64||r==0)?7:0);
    g+=((g>64||g==0)?7:0)+((((rect>>5)&1)&&g>80)?4:0);
    b+=((b>64||b==0)?7:0);
    if(r>255)r=255;if(g>255)g=255;if(b>255)b=255;
    return RGBA8(r,g,b,255);
}

static int bg_parse_layers(const char *text){
    const char *p=text;int source_order=0;
    while((p=strstr(p,"layer:"))!=NULL){
        const char *end=strstr(p,"layer_end");if(!end)break;
        size_t n=(size_t)(end-(p+6));if(n>2047)n=2047;char block[2048];memcpy(block,p+6,n);block[n]=0;
        if(g_bg.layer_count>=BG_MAX_LAYERS)return -1;
        bg_layer_t *l=&g_bg.layers[g_bg.layer_count];memset(l,0,sizeof(*l));l->texture_index=-1;l->source_order=source_order++;
        bg_extract_layer_path(block,l->path,sizeof(l->path));
        l->transparency=bg_find_int(block,"transparency:",0);
        l->width=bg_find_int(block,"width:",0);l->x=bg_find_int(block,"x:",0);l->y=bg_find_int(block,"y:",0);
        l->height=bg_find_int(block,"height:",0);l->loop=bg_find_int(block,"loop:",0);l->tile=bg_find_int(block,"tile:",0);
        l->cc=bg_find_int(block,"cc:",0);l->c1=bg_find_int(block,"c1:",0);l->c2=bg_find_int(block,"c2:",0);
        l->rect_present=strstr(block,"rect:")!=NULL;l->rect_value=bg_find_int(block,"rect:",0);
        if(l->width<=0){lf2_logf("WARN","BG skip layer without width stage=%d order=%d",g_bg.stage_index,l->source_order);p=end+9;continue;}
        if(!l->rect_present){
            if(!l->path[0]){lf2_logf("WARN","BG skip layer without bitmap stage=%d order=%d",g_bg.stage_index,l->source_order);p=end+9;continue;}
            int ti=bg_texture_get(l->path,l->transparency);if(ti<0){lf2_logf("ERROR","BG texture load failed stage=%d path=%s rc=%d",g_bg.stage_index,l->path,ti);return -2;}l->texture_index=ti;
        }
        g_bg.layer_count++;p=end+9;
    }
    return g_bg.layer_count>0?0:-3;
}

static void bg_sort_draw_order(void){
    for(int i=0;i<g_bg.layer_count;i++)g_bg.draw_order[i]=i;
    for(int i=0;i<g_bg.layer_count;i++)for(int j=i+1;j<g_bg.layer_count;j++){
        bg_layer_t *a=&g_bg.layers[g_bg.draw_order[i]],*b=&g_bg.layers[g_bg.draw_order[j]];
        if(a->width>b->width||(a->width==b->width&&a->source_order>b->source_order)){
            int t=g_bg.draw_order[i];g_bg.draw_order[i]=g_bg.draw_order[j];g_bg.draw_order[j]=t;
        }
    }
}

void lf2_background_unload(void){
    for(int i=0;i<g_bg.texture_count;i++)if(g_bg.textures[i].texture)vita2d_free_texture(g_bg.textures[i].texture);
    memset(&g_bg,0,sizeof(g_bg));g_bg.stage_index=-1;
}

int lf2_background_load(int stage_index){
    lf2_background_unload();
    if(stage_index<0||stage_index>=(int)(sizeof(g_bg_dat_paths)/sizeof(g_bg_dat_paths[0])))return -1;
    g_bg.stage_index=stage_index;char *text=NULL;int rc=bg_decode_dat(g_bg_dat_paths[stage_index],&text);
    if(rc<0){lf2_logf("ERROR","BG dat decode failed stage=%d path=%s rc=%d",stage_index,g_bg_dat_paths[stage_index],rc);lf2_background_unload();return -2;}
    g_bg.width=bg_find_int(text,"width:",0);bg_copy_name(text,g_bg.name,sizeof(g_bg.name));
    rc=bg_parse_layers(text);free(text);
    if(rc<0||g_bg.width<=0){lf2_logf("ERROR","BG parse failed stage=%d rc=%d width=%d",stage_index,rc,g_bg.width);lf2_background_unload();return -3;}
    bg_sort_draw_order();g_bg.timer=0;g_bg.loaded=1;
    lf2_logf("BG","loaded stage=%d name=%s width=%d layers=%d textures=%d",stage_index,g_bg.name,g_bg.width,g_bg.layer_count,g_bg.texture_count);
    return 0;
}

int lf2_background_loaded_stage(void){return g_bg.loaded?g_bg.stage_index:-1;}
void lf2_background_tick(void){if(g_bg.loaded)g_bg.timer++;}

static int bg_layer_visible(const bg_layer_t *l){
    if(!l)return 0;if(l->cc<=0)return 1;int f=(int)(g_bg.timer%(uint32_t)l->cc);return f>=l->c1&&f<=l->c2;
}

static float bg_layer_y(const bg_layer_t *l){
    float y=(float)l->y;
    /* Retail LF2 has this scene-specific offset; F.LF documents the same
       exception. back1 stays authored, all other HK Coliseum layers rise 8px. */
    if(g_bg.stage_index==3 && (!l->path[0]||!strstr(l->path,"back1")))y-=8.0f;
    return y;
}

static void bg_draw_image(vita2d_texture *tex,float x,float y){
    if(!tex)return;float w=(float)vita2d_texture_get_width(tex)*BG_X_SCALE;
    if(x>=BG_VITA_VIEW_W||x+w<=0.0f)return;
    vita2d_draw_texture_scale(tex,x,y,BG_X_SCALE,1.0f);
}

bool lf2_background_draw(float camera_x){
    if(!g_bg.loaded)return false;
    float denom=(float)g_bg.width-BG_PC_VIEW_W;
    for(int oi=0;oi<g_bg.layer_count;oi++){
        bg_layer_t *l=&g_bg.layers[g_bg.draw_order[oi]];if(!bg_layer_visible(l))continue;
        float ratio=fabsf(denom)>0.01f?((float)l->width-BG_PC_VIEW_W)/denom:0.0f;
        float group_x=-camera_x*ratio;float y=bg_layer_y(l);
        if(l->rect_present){
            float x=(float)l->x*BG_X_SCALE+group_x,w=(float)l->width*BG_X_SCALE;
            if(x<BG_VITA_VIEW_W&&x+w>0.0f&&l->height>0)vita2d_draw_rectangle(x,y,w,(float)l->height,bg_rect_color(l->rect_value));
            continue;
        }
        if(l->texture_index<0||l->texture_index>=g_bg.texture_count)continue;vita2d_texture *tex=g_bg.textures[l->texture_index].texture;
        if(l->loop){
            float step=(float)abs(l->loop)*BG_X_SCALE;if(step<1.0f)step=1.0f;
            for(float xx=(float)l->x*BG_X_SCALE+group_x;xx<(float)l->width*BG_X_SCALE+group_x;xx+=step)bg_draw_image(tex,xx,y);
        }else if(l->tile){
            int at=abs(l->tile);float left=((float)l->x-(float)l->width*at)*BG_X_SCALE+group_x;
            float right=((float)l->width+(float)l->width*at)*BG_X_SCALE+group_x;
            float step=(float)l->width*BG_X_SCALE;if(step<1.0f)step=1.0f;
            for(float xx=left;xx<right;xx+=step)bg_draw_image(tex,xx,y);
        }else bg_draw_image(tex,(float)l->x*BG_X_SCALE+group_x,y);
    }
    return true;
}
