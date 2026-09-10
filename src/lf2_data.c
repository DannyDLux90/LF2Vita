#include "lf2_data.h"
#include "asset_pack.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const unsigned char k_cipher[] = "odBearBecauseHeIsVeryGoodSiuHungIsAGo";

static int find_int(const char *line, const char *key, int fallback) {
    const char *p = strstr(line, key);
    if (!p) return fallback;
    p += strlen(key);
    while (*p == ' ' || *p == '\t') ++p;
    return (int)strtol(p, NULL, 10);
}

static float find_float(const char *line, const char *key, float fallback) {
    const char *p = strstr(line, key);
    if (!p) return fallback;
    p += strlen(key);
    while (*p == ' ' || *p == '\t') ++p;
    return strtof(p, NULL);
}

static void copy_trimmed_value(const char *line, const char *key, char *out, size_t cap) {
    const char *p = strstr(line, key);
    if (!p || !cap) return;
    p += strlen(key);
    while (*p == ' ' || *p == '\t') ++p;
    size_t n = strcspn(p, "\r\n");
    while (n && (p[n-1] == ' ' || p[n-1] == '\t')) --n;
    if (n >= cap) n = cap - 1;
    memcpy(out, p, n);
    out[n] = 0;
}

int lf2_normalize_relpath(const char *in, char *out, int cap) {
    if (!in || !out || cap <= 0) return -1;
    int j = 0;
    while (*in && j < cap - 1) {
        char c = *in++;
        if (c == '\\') c = '/';
        out[j++] = c;
    }
    out[j] = 0;
    return 0;
}

static int read_decode_dat(const char *path, char **text_out, size_t *size_out) {
    char norm[192];
    const char *prefix="app0:/game/LittleFighter/";
    if (!strncmp(path,prefix,strlen(prefix))) path += strlen(prefix);
    lf2_normalize_relpath(path,norm,sizeof(norm));
    unsigned char *src=NULL; size_t sz=0;
    int prc=lf2_pak_read(norm,(void**)&src,&sz);
    if (prc<0) { lf2_logf("ERROR","DAT read failed path=%s rc=%d",norm,prc); return -1; }
    if (sz < 123) { free(src); return -3; }
    size_t outsz = sz - 123;
    char *dst = (char *)malloc(outsz + 1);
    if (!dst) { free(src); return -6; }
    const size_t keylen = sizeof(k_cipher) - 1;
    for (size_t i = 0; i < outsz; ++i)
        dst[i] = (char)((unsigned char)src[i + 123] - k_cipher[i % keylen]);
    dst[outsz] = 0;
    free(src);
    *text_out = dst;
    if (size_out) *size_out = outsz;
    return 0;
}

static void make_mirror_path(const char *path, char *out, size_t cap) {
    char input[192];
    snprintf(input, sizeof(input), "%s", path ? path : "");
    path = input;
    const char *dot = strrchr(path, '.');
    if (!dot) {
        if (cap) { size_t room=cap>12?cap-12:0; snprintf(out,cap,"%.*s_mirror.bmp",(int)room,path); }
        return;
    }
    size_t base = (size_t)(dot - path);
    if (base > cap - 13) base = cap - 13;
    memcpy(out, path, base);
    out[base] = 0;
    strncat(out, "_mirror", cap - strlen(out) - 1);
    strncat(out, dot, cap - strlen(out) - 1);
}

static void parse_main_frame_line(lf2_frame_def_t *fr, const char *line) {
    fr->pic = find_int(line, "pic:", fr->pic);
    fr->state = find_int(line, "state:", fr->state);
    fr->wait = find_int(line, "wait:", fr->wait);
    fr->next = find_int(line, "next:", fr->next);
    fr->dvx = find_int(line, "dvx:", fr->dvx);
    fr->dvy = find_int(line, "dvy:", fr->dvy);
    fr->dvz = find_int(line, "dvz:", fr->dvz);
    fr->centerx = find_int(line, "centerx:", fr->centerx);
    fr->centery = find_int(line, "centery:", fr->centery);
    fr->hit_a = find_int(line, "hit_a:", fr->hit_a);
    fr->hit_d = find_int(line, "hit_d:", fr->hit_d);
    fr->hit_j = find_int(line, "hit_j:", fr->hit_j);
    fr->hit_Fa = find_int(line, "hit_Fa:", fr->hit_Fa);
    fr->hit_Ua = find_int(line, "hit_Ua:", fr->hit_Ua);
    fr->hit_Da = find_int(line, "hit_Da:", fr->hit_Da);
    fr->hit_Fj = find_int(line, "hit_Fj:", fr->hit_Fj);
    fr->hit_Uj = find_int(line, "hit_Uj:", fr->hit_Uj);
    fr->hit_Dj = find_int(line, "hit_Dj:", fr->hit_Dj);
    fr->hit_ja = find_int(line, "hit_ja:", fr->hit_ja);
    fr->mp = find_int(line, "mp:", fr->mp);
}

static void parse_itr_line(lf2_itr_def_t *it, const char *line) {
    it->kind = find_int(line, "kind:", it->kind);
    it->x = find_int(line, "x:", it->x);
    it->y = find_int(line, "y:", it->y);
    it->w = find_int(line, "w:", it->w);
    it->h = find_int(line, "h:", it->h);
    it->dvx = find_int(line, "dvx:", it->dvx);
    it->dvy = find_int(line, "dvy:", it->dvy);
    it->fall = find_int(line, "fall:", it->fall);
    it->arest = find_int(line, "arest:", it->arest);
    it->vrest = find_int(line, "vrest:", it->vrest);
    it->bdefend = find_int(line, "bdefend:", it->bdefend);
    it->injury = find_int(line, "injury:", it->injury);
    it->effect = find_int(line, "effect:", it->effect);
    it->zwidth = find_int(line, "zwidth:", it->zwidth);
}

static void parse_bdy_line(lf2_bdy_def_t *b, const char *line) {
    b->kind = find_int(line, "kind:", b->kind);
    b->x = find_int(line, "x:", b->x);
    b->y = find_int(line, "y:", b->y);
    b->w = find_int(line, "w:", b->w);
    b->h = find_int(line, "h:", b->h);
}

static void parse_opoint_line(lf2_opoint_def_t *o, const char *line) {
    o->kind = find_int(line, "kind:", o->kind);
    o->x = find_int(line, "x:", o->x);
    o->y = find_int(line, "y:", o->y);
    o->action = find_int(line, "action:", o->action);
    o->dvx = find_int(line, "dvx:", o->dvx);
    o->dvy = find_int(line, "dvy:", o->dvy);
    o->dvz = find_int(line, "dvz:", o->dvz);
    o->oid = find_int(line, "oid:", o->oid);
    o->facing = find_int(line, "facing:", o->facing);
}

int lf2_load_character(const char *app0_root, const char *dat_rel_path, lf2_character_def_t *out) {
    if (!app0_root || !dat_rel_path || !out) return -1;
    memset(out, 0, sizeof(*out));
    out->walking_frame_rate = 3;
    out->walking_speed = 5.0f;
    out->walking_speedz = 2.5f;
    out->running_frame_rate = 3;
    out->running_speed = 10.0f;
    out->running_speedz = 1.6f;
    out->jump_height = -16.3f;
    out->jump_distance = 10.0f;
    out->jump_distancez = 3.75f;

    char norm[192], full[256];
    lf2_normalize_relpath(dat_rel_path, norm, sizeof(norm));
    snprintf(full, sizeof(full), "%s/%s", app0_root, norm);
    char *text = NULL;
    if (read_decode_dat(full, &text, NULL) < 0) return -2;

    enum { SEC_NONE, SEC_ITR, SEC_BDY, SEC_OPOINT } sec = SEC_NONE;
    lf2_frame_def_t *fr = NULL;
    lf2_itr_def_t *cur_itr = NULL;
    lf2_bdy_def_t *cur_bdy = NULL;
    lf2_opoint_def_t *cur_opoint = NULL;

    int next_pic_index = 0;
    char *save = NULL;
    for (char *line = strtok_r(text, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        while (*line == ' ' || *line == '\t' || *line == '\r') ++line;
        size_t ln = strlen(line);
        while (ln && (line[ln-1] == '\r' || line[ln-1] == ' ' || line[ln-1] == '\t')) line[--ln] = 0;
        if (!*line) continue;

        if (!strncmp(line, "name:", 5)) {
            copy_trimmed_value(line, "name:", out->name, sizeof(out->name));
        } else if (!strncmp(line, "head:", 5)) {
            copy_trimmed_value(line, "head:", out->head, sizeof(out->head));
            lf2_normalize_relpath(out->head, out->head, sizeof(out->head));
        } else if (!strncmp(line, "small:", 6)) {
            copy_trimmed_value(line, "small:", out->small, sizeof(out->small));
            lf2_normalize_relpath(out->small, out->small, sizeof(out->small));
        } else if (!strncmp(line, "file(", 5) && out->sheet_count < LF2_MAX_SHEETS) {
            int a=0,b=0,w=0,h=0,row=0,col=0;
            char p[128] = {0};
            if (sscanf(line, "file(%d-%d): %127s w: %d h: %d row: %d col: %d", &a,&b,p,&w,&h,&row,&col) >= 7) {
                lf2_sheet_def_t *s = &out->sheets[out->sheet_count++];
                /* LF2 does not use the numbers written in file(a-b) as the
                   runtime picture index. They are comments for data authors;
                   pictures are numbered cumulatively by row*col. This matters
                   for stock data such as Justin, where later sheets restart
                   the written labels at 0-69. */
                int declared_first=a, declared_last=b;
                int cell_count=(row>0&&col>0)?row*col:1;
                s->first_pic=next_pic_index;
                s->last_pic=next_pic_index+cell_count-1;
                next_pic_index += cell_count;
                s->w=w; s->h=h; s->row=row; s->col=col;
                lf2_normalize_relpath(p, s->path, sizeof(s->path));
                make_mirror_path(s->path, s->mirror_path, sizeof(s->mirror_path));
                if(declared_first!=s->first_pic || declared_last!=s->last_pic)
                    lf2_logf("ANIM","sheet numbering normalized char=%s path=%s declared=%d-%d actual=%d-%d",
                             out->name,s->path,declared_first,declared_last,s->first_pic,s->last_pic);
            }
        } else if (!strncmp(line, "walking_frame_rate", 18)) out->walking_frame_rate = find_float(line, "walking_frame_rate", out->walking_frame_rate);
        else if (!strncmp(line, "walking_speedz", 14)) out->walking_speedz = find_float(line, "walking_speedz", out->walking_speedz);
        else if (!strncmp(line, "walking_speed", 13)) out->walking_speed = find_float(line, "walking_speed", out->walking_speed);
        else if (!strncmp(line, "running_frame_rate", 18)) out->running_frame_rate = find_float(line, "running_frame_rate", out->running_frame_rate);
        else if (!strncmp(line, "running_speedz", 14)) out->running_speedz = find_float(line, "running_speedz", out->running_speedz);
        else if (!strncmp(line, "running_speed", 13)) out->running_speed = find_float(line, "running_speed", out->running_speed);
        else if (!strncmp(line, "jump_height", 11)) out->jump_height = find_float(line, "jump_height", out->jump_height);
        else if (!strncmp(line, "jump_distancez", 14)) out->jump_distancez = find_float(line, "jump_distancez", out->jump_distancez);
        else if (!strncmp(line, "jump_distance", 13)) out->jump_distance = find_float(line, "jump_distance", out->jump_distance);
        else if (!strncmp(line, "dash_height", 11)) out->dash_height = find_float(line, "dash_height", out->dash_height);
        else if (!strncmp(line, "dash_distancez", 14)) out->dash_distancez = find_float(line, "dash_distancez", out->dash_distancez);
        else if (!strncmp(line, "dash_distance", 13)) out->dash_distance = find_float(line, "dash_distance", out->dash_distance);
        else if (!strncmp(line, "<frame>", 7)) {
            int id=-1;
            char label[32] = {0};
            if (sscanf(line, "<frame> %d %31[^\r\n]", &id, label) >= 1 && id >= 0 && id < LF2_MAX_FRAMES) {
                fr = &out->frames[id];
                memset(fr, 0, sizeof(*fr));
                fr->present = true;
                fr->id = id;
                fr->next = 999;
                if (*label) snprintf(fr->label, sizeof(fr->label), "%s", label);
                sec = SEC_NONE;
            } else fr = NULL;
        } else if (!strcmp(line, "<frame_end>")) {
            fr = NULL; sec = SEC_NONE; cur_itr = NULL; cur_bdy = NULL; cur_opoint = NULL;
        } else if (fr && !strcmp(line, "itr:")) {
            if (fr->itr_count < LF2_MAX_ITRS) {
                cur_itr = &fr->itrs[fr->itr_count];
                memset(cur_itr, 0, sizeof(*cur_itr));
                sec = SEC_ITR;
            }
        } else if (fr && !strcmp(line, "itr_end:")) {
            if (cur_itr && fr->itr_count < LF2_MAX_ITRS) fr->itr_count++;
            cur_itr = NULL; sec = SEC_NONE;
        } else if (fr && !strcmp(line, "bdy:")) {
            if (fr->bdy_count < LF2_MAX_BDYS) {
                cur_bdy = &fr->bdys[fr->bdy_count];
                memset(cur_bdy, 0, sizeof(*cur_bdy));
                sec = SEC_BDY;
            }
        } else if (fr && !strcmp(line, "bdy_end:")) {
            if (cur_bdy && fr->bdy_count < LF2_MAX_BDYS) fr->bdy_count++;
            cur_bdy = NULL; sec = SEC_NONE;
        } else if (fr && !strcmp(line, "opoint:")) {
            if (fr->opoint_count < LF2_MAX_OPOINTS) {
                cur_opoint = &fr->opoints[fr->opoint_count];
                memset(cur_opoint, 0, sizeof(*cur_opoint));
                sec = SEC_OPOINT;
            }
        } else if (fr && !strcmp(line, "opoint_end:")) {
            if (cur_opoint && fr->opoint_count < LF2_MAX_OPOINTS) fr->opoint_count++;
            cur_opoint = NULL; sec = SEC_NONE;
        } else if (fr && !strncmp(line, "sound:", 6)) {
            copy_trimmed_value(line, "sound:", fr->sound, sizeof(fr->sound));
            lf2_normalize_relpath(fr->sound, fr->sound, sizeof(fr->sound));
        } else if (fr) {
            if (sec == SEC_ITR && cur_itr) parse_itr_line(cur_itr, line);
            else if (sec == SEC_BDY && cur_bdy) parse_bdy_line(cur_bdy, line);
            else if (sec == SEC_OPOINT && cur_opoint) parse_opoint_line(cur_opoint, line);
            else if (strstr(line, "pic:")) parse_main_frame_line(fr, line);
        }
    }

    free(text);
    return out->sheet_count > 0 ? 0 : -3;
}

void lf2_make_black_transparent(vita2d_texture *tex) {
    if (!tex) return;
    if (vita2d_texture_get_format(tex) != SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR) return;
    uint32_t *p = (uint32_t *)vita2d_texture_get_datap(tex);
    unsigned w = vita2d_texture_get_width(tex);
    unsigned h = vita2d_texture_get_height(tex);
    unsigned stride = vita2d_texture_get_stride(tex) / 4;
    for (unsigned y=0; y<h; ++y) {
        for (unsigned x=0; x<w; ++x) {
            uint32_t v = p[y*stride+x];
            unsigned r = v & 0xff, g = (v>>8)&0xff, b = (v>>16)&0xff;
            if (r < 5 && g < 5 && b < 5) p[y*stride+x] = 0;
        }
    }
}


#pragma pack(push,1)
typedef struct { uint16_t type; uint32_t size; uint16_t r1,r2; uint32_t off; } bmp_file_hdr_t;
typedef struct { uint32_t size; int32_t width,height; uint16_t planes,bpp; uint32_t compression,size_image; int32_t xppm,yppm; uint32_t colors_used,colors_important; } bmp_info_hdr_t;
#pragma pack(pop)

static vita2d_texture *load_bmp_colorkey_buffer(const unsigned char *buf,size_t size) {
    if(!buf || size < sizeof(bmp_file_hdr_t)+sizeof(bmp_info_hdr_t)) return NULL;
    bmp_file_hdr_t fh; bmp_info_hdr_t ih;
    memcpy(&fh,buf,sizeof(fh)); memcpy(&ih,buf+sizeof(fh),sizeof(ih));
    if(fh.type!=0x4d42) return NULL;
    if(ih.bpp != 8 || ih.width <= 0 || ih.height == 0) {
        vita2d_texture *t=vita2d_load_BMP_buffer(buf);
        if(t) lf2_make_black_transparent(t);
        return t;
    }
    int width=ih.width, height=ih.height < 0 ? -ih.height : ih.height;
    int topdown=ih.height<0; uint32_t ncolors=ih.colors_used?ih.colors_used:256; if(ncolors>256)ncolors=256;
    if((size_t)sizeof(fh)+ih.size+(size_t)ncolors*4>size || fh.off>=size) return NULL;
    uint32_t palette[256]; memset(palette,0,sizeof(palette));
    const unsigned char *pal=buf+sizeof(fh)+ih.size;
    for(uint32_t i=0;i<ncolors;i++){
        const unsigned char *bgra=pal+i*4; unsigned a=(bgra[2]==0&&bgra[1]==0&&bgra[0]==0)?0:255;
        palette[i]=RGBA8(bgra[2],bgra[1],bgra[0],a);
    }
    unsigned char *idx=(unsigned char*)calloc((size_t)width*(size_t)height,1); if(!idx)return NULL;
    const unsigned char *p=buf+fh.off,*pend=buf+size;
    if(ih.compression==0){
        int rowbytes=(width+3)&~3;
        for(int sy=0;sy<height;sy++){
            if(p+rowbytes>pend)break; int dy=topdown?sy:(height-1-sy);
            memcpy(idx+(size_t)dy*width,p,(size_t)width); p+=rowbytes;
        }
    } else if(ih.compression==1){
        int x=0,file_y=0,done=0;
        while(!done&&file_y<height&&p+2<=pend){
            int count=*p++,val=*p++;
            if(count){for(int i=0;i<count;i++){if(x<width&&file_y<height){int dy=topdown?file_y:(height-1-file_y);idx[(size_t)dy*width+x]=(unsigned char)val;}x++;}}
            else if(val==0){x=0;file_y++;}
            else if(val==1){done=1;}
            else if(val==2){if(p+2>pend)break;x+=*p++;file_y+=*p++;}
            else {int n=val;for(int i=0;i<n&&p<pend;i++){int px=*p++;if(x<width&&file_y<height){int dy=topdown?file_y:(height-1-file_y);idx[(size_t)dy*width+x]=(unsigned char)px;}x++;}if((n&1)&&p<pend)p++;}
        }
    } else {free(idx);return NULL;}
    vita2d_texture *t=vita2d_create_empty_texture((unsigned)width,(unsigned)height); if(!t){free(idx);return NULL;}
    uint32_t *dst=(uint32_t*)vita2d_texture_get_datap(t); unsigned stride=vita2d_texture_get_stride(t)/4;
    for(int y=0;y<height;y++)for(int x=0;x<width;x++)dst[(size_t)y*stride+x]=palette[idx[(size_t)y*width+x]];
    free(idx); return t;
}

vita2d_texture *lf2_load_bmp_colorkey(const char *filename) {
    if(!filename)return NULL; const char *path=filename; const char *prefix="app0:/game/LittleFighter/";
    if(!strncmp(path,prefix,strlen(prefix)))path+=strlen(prefix);
    char norm[192];lf2_normalize_relpath(path,norm,sizeof(norm));
    void *buf=NULL;size_t n=0;int rc=lf2_pak_read(norm,&buf,&n);if(rc<0)return NULL;
    vita2d_texture *t=load_bmp_colorkey_buffer((const unsigned char*)buf,n);free(buf);
    if(!t)lf2_logf("ERROR","BMP decode failed: %s",norm);
    return t;
}

int lf2_load_character_textures(const char *app0_root, lf2_character_def_t *ch) {
    (void)app0_root;
    if (!ch) return -1;
    lf2_logf("INFO","loading textures char=%s sheets=%d",ch->name,ch->sheet_count);
    for (int i=0;i<ch->sheet_count;++i) {
        lf2_log_stage("match:texture_load","char=%s sheet=%d path=%s",ch->name,i,ch->sheets[i].path);
        ch->sheets[i].texture = lf2_load_bmp_colorkey(ch->sheets[i].path);
        if (!ch->sheets[i].texture) { lf2_logf("ERROR","texture failed char=%s sheet=%d",ch->name,i); return -2; }
        ch->sheets[i].mirror_texture = NULL; /* Vita renderer flips UVs; do not double GPU memory. */
        /* LF2 sprite sheets are pixel art with a one-pixel separator between
           cells. Linear filtering samples across that separator and adjacent
           frames, so always use nearest/point filtering for character art. */
        vita2d_texture_set_filters(ch->sheets[i].texture,SCE_GXM_TEXTURE_FILTER_POINT,SCE_GXM_TEXTURE_FILTER_POINT);
        unsigned texw=vita2d_texture_get_width(ch->sheets[i].texture),texh=vita2d_texture_get_height(ch->sheets[i].texture);
        unsigned expectw=(unsigned)(ch->sheets[i].row*(ch->sheets[i].w+1));
        unsigned expecth=(unsigned)(ch->sheets[i].col*(ch->sheets[i].h+1));
        lf2_logf("INFO","texture ok char=%s sheet=%d tex=%p data=%p %ux%u stride=%u layout=%dx%d cell=%dx%d expected=%ux%u",ch->name,i,(void*)ch->sheets[i].texture,vita2d_texture_get_datap(ch->sheets[i].texture),texw,texh,vita2d_texture_get_stride(ch->sheets[i].texture),ch->sheets[i].row,ch->sheets[i].col,ch->sheets[i].w,ch->sheets[i].h,expectw,expecth);
        if(texw<expectw || texh<expecth)lf2_logf("WARN","sprite sheet smaller than DAT layout char=%s sheet=%d actual=%ux%u expected>=%ux%u",ch->name,i,texw,texh,expectw,expecth);
        lf2_log_memory("after texture");
    }
    return 0;
}

void lf2_free_character_textures(lf2_character_def_t *ch) {
    if (!ch) return;
    for (int i=0;i<ch->sheet_count;++i) {
        if (ch->sheets[i].texture) vita2d_free_texture(ch->sheets[i].texture);
        if (ch->sheets[i].mirror_texture) vita2d_free_texture(ch->sheets[i].mirror_texture);
        ch->sheets[i].texture = ch->sheets[i].mirror_texture = NULL;
    }
}

lf2_sheet_def_t *lf2_sheet_for_pic(lf2_character_def_t *ch, int pic) {
    if (!ch) return NULL;
    for (int i=0;i<ch->sheet_count;++i)
        if (pic >= ch->sheets[i].first_pic && pic <= ch->sheets[i].last_pic) return &ch->sheets[i];
    return NULL;
}
