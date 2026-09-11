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
            fr = NULL; sec = SEC_NONE; cur_itr = NULL; cur_bdy = NULL; cur_opoint = NULL; cur_wpoint = NULL; cur_cpoint = NULL;
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
        } else if (fr && !strcmp(line, "wpoint:")) {
            cur_wpoint = &fr->wpoint;
            memset(cur_wpoint, 0, sizeof(*cur_wpoint));
            cur_wpoint->present = true;
            sec = SEC_WPOINT;
        } else if (fr && !strcmp(line, "wpoint_end:")) {
            cur_wpoint = NULL; sec = SEC_NONE;
        } else if (fr && !strcmp(line, "cpoint:")) {
            cur_cpoint = &fr->cpoint;
            memset(cur_cpoint, 0, sizeof(*cur_cpoint));
            cur_cpoint->present = true;
            sec = SEC_CPOINT;
        } else if (fr && !strcmp(line, "cpoint_end:")) {
            cur_cpoint = NULL; sec = SEC_NONE;
        } else if (fr && !strncmp(line, "sound:", 6)) {
            copy_trimmed_value(line, "sound:", fr->sound, sizeof(fr->sound));
            lf2_normalize_relpath(fr->sound, fr->sound, sizeof(fr->sound));
        } else if (fr) {
            if (sec == SEC_ITR && cur_itr) parse_itr_line(cur_itr, line);
            else if (sec == SEC_BDY && cur_bdy) parse_bdy_line(cur_bdy, line);
            else if (sec == SEC_OPOINT && cur_opoint) parse_opoint_line(cur_opoint, line);
            else if (sec == SEC_WPOINT && cur_wpoint) parse_wpoint_line(cur_wpoint, line);
            else if (sec == SEC_CPOINT && cur_cpoint) parse_cpoint_line(cur_cpoint, line);
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
