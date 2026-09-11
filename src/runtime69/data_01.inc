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
    it->catchingact = find_int(line, "catchingact:", it->catchingact);
    it->caughtact = find_int(line, "caughtact:", it->caughtact);
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


static void parse_cpoint_line(lf2_cpoint_def_t *c, const char *line) {
    if (!c) return;
    c->present = true;
    c->kind = find_int(line, "kind:", c->kind);
    c->x = find_int(line, "x:", c->x);
    c->y = find_int(line, "y:", c->y);
    c->injury = find_int(line, "injury:", c->injury);
    c->vaction = find_int(line, "vaction:", c->vaction);
    c->aaction = find_int(line, "aaction:", c->aaction);
    c->jaction = find_int(line, "jaction:", c->jaction);
    c->taction = find_int(line, "taction:", c->taction);
    c->tx = find_int(line, "tx:", c->tx);
    c->ty = find_int(line, "ty:", c->ty);
    c->throwvx = find_int(line, "throwvx:", c->throwvx);
    c->throwvy = find_int(line, "throwvy:", c->throwvy);
    c->throwvz = find_int(line, "throwvz:", c->throwvz);
    c->hurtable = find_int(line, "hurtable:", c->hurtable);
    c->throwinjury = find_int(line, "throwinjury:", c->throwinjury);
    c->decrease = find_int(line, "decrease:", c->decrease);
    c->dircontrol = find_int(line, "dircontrol:", c->dircontrol);
    c->cover = find_int(line, "cover:", c->cover);
    c->fronthurtact = find_int(line, "fronthurtact:", c->fronthurtact);
    c->backhurtact = find_int(line, "backhurtact:", c->backhurtact);
    /* Several stock 2.00a DATs contain the MSVC debug-fill sentinel
       -842150451 in unused cpoint fields. It means "unset", not velocity. */
    if(c->throwvx < -1000000 || c->throwvx > 1000000)c->throwvx=0;
    if(c->throwvy < -1000000 || c->throwvy > 1000000)c->throwvy=0;
    if(c->throwvz < -1000000 || c->throwvz > 1000000)c->throwvz=0;
    if(c->throwinjury < -1000000 || c->throwinjury > 1000000)c->throwinjury=0;
}

static void parse_wpoint_line(lf2_wpoint_def_t *w, const char *line) {
    if (!w) return;
    w->present = true;
    w->kind = find_int(line, "kind:", w->kind);
    w->x = find_int(line, "x:", w->x);
    w->y = find_int(line, "y:", w->y);
    w->weaponact = find_int(line, "weaponact:", w->weaponact);
    w->attacking = find_int(line, "attacking:", w->attacking);
    w->cover = find_int(line, "cover:", w->cover);
    w->dvx = find_int(line, "dvx:", w->dvx);
    w->dvy = find_int(line, "dvy:", w->dvy);
    w->dvz = find_int(line, "dvz:", w->dvz);
}

int lf2_load_character(const char *app0_root, const char *dat_rel_path, lf2_character_def_t *out) {
    if (!app0_root || !dat_rel_path || !out) return -1;
    memset(out, 0, sizeof(*out));
    out->walking_frame_rate = 3;
    out->walking_speed = 5.0f;
    out->heavy_walking_speed = 3.0f;
    out->heavy_walking_speedz = 1.5f;
    out->heavy_running_speed = 5.0f;
    out->heavy_running_speedz = 0.8f;
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

    enum { SEC_NONE, SEC_ITR, SEC_BDY, SEC_OPOINT, SEC_WPOINT, SEC_CPOINT } sec = SEC_NONE;
    lf2_frame_def_t *fr = NULL;
    lf2_itr_def_t *cur_itr = NULL;
    lf2_bdy_def_t *cur_bdy = NULL;
    lf2_opoint_def_t *cur_opoint = NULL;
    lf2_wpoint_def_t *cur_wpoint = NULL;
    lf2_cpoint_def_t *cur_cpoint = NULL;
    bool in_weapon_strength = false;
    int strength_entry = -1;

    int next_pic_index = 0;
    char *save = NULL;
    for (char *line = strtok_r(text, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        while (*line == ' ' || *line == '\t' || *line == '\r') ++line;
        size_t ln = strlen(line);
        while (ln && (line[ln-1] == '\r' || line[ln-1] == ' ' || line[ln-1] == '\t')) line[--ln] = 0;
        if (!*line) continue;

        if (!strcmp(line, "<weapon_strength_list>")) {
            in_weapon_strength=true;strength_entry=-1;continue;
        } else if (!strcmp(line, "<weapon_strength_list_end>")) {
            in_weapon_strength=false;strength_entry=-1;continue;
        } else if (in_weapon_strength && !strncmp(line, "entry:", 6)) {
            int e=find_int(line,"entry:",0);strength_entry=(e>=1&&e<=4)?e-1:-1;
            if(strength_entry>=0){memset(&out->weapon_strength[strength_entry],0,sizeof(out->weapon_strength[strength_entry]));out->weapon_strength_present[strength_entry]=true;}
            continue;
        } else if (in_weapon_strength && strength_entry>=0) {
            parse_itr_line(&out->weapon_strength[strength_entry],line);continue;
        } else if (!strncmp(line,"weapon_hp:",10)) out->weapon_hp=find_int(line,"weapon_hp:",out->weapon_hp);
        else if (!strncmp(line,"weapon_drop_hurt:",17)) out->weapon_drop_hurt=find_int(line,"weapon_drop_hurt:",out->weapon_drop_hurt);
        else if (!strncmp(line,"weapon_hit_sound:",17)) {copy_trimmed_value(line,"weapon_hit_sound:",out->weapon_hit_sound,sizeof(out->weapon_hit_sound));lf2_normalize_relpath(out->weapon_hit_sound,out->weapon_hit_sound,sizeof(out->weapon_hit_sound));}
        else if (!strncmp(line,"weapon_drop_sound:",18)) {copy_trimmed_value(line,"weapon_drop_sound:",out->weapon_drop_sound,sizeof(out->weapon_drop_sound));lf2_normalize_relpath(out->weapon_drop_sound,out->weapon_drop_sound,sizeof(out->weapon_drop_sound));}
        else if (!strncmp(line,"weapon_broken_sound:",20)) {copy_trimmed_value(line,"weapon_broken_sound:",out->weapon_broken_sound,sizeof(out->weapon_broken_sound));lf2_normalize_relpath(out->weapon_broken_sound,out->weapon_broken_sound,sizeof(out->weapon_broken_sound));}
        else if (!strncmp(line, "name:", 5)) {
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
        } else if (!strncmp(line, "heavy_walking_speedz", 20)) out->heavy_walking_speedz = find_float(line, "heavy_walking_speedz", out->heavy_walking_speedz);
        else if (!strncmp(line, "heavy_walking_speed", 19)) out->heavy_walking_speed = find_float(line, "heavy_walking_speed", out->heavy_walking_speed);
        else if (!strncmp(line, "heavy_running_speedz", 20)) out->heavy_running_speedz = find_float(line, "heavy_running_speedz", out->heavy_running_speedz);
        else if (!strncmp(line, "heavy_running_speed", 19)) out->heavy_running_speed = find_float(line, "heavy_running_speed", out->heavy_running_speed);
        else if (!strncmp(line, "walking_frame_rate", 18)) out->walking_frame_rate = find_float(line, "walking_frame_rate", out->walking_frame_rate);
        else if (!strncmp(line, "walking_speedz", 14)) out->walking_speedz = find_float(line, "walking_speedz", out->walking_speedz);
        else if (!strncmp(line, "walking_speed", 13)) out->walking_speed = find_float(line, "walking_speed", out->walking_speed);
        else if (!strncmp(line, "running_frame_rate", 18)) out->running_frame_rate = find_float(line, "running_frame_rate", out->running_frame_rate);
        else if (!strncmp(line, "running_speedz", 14)) out->running_speedz = find_float(line, "running_speedz", out->running_speedz);
        else if (!strncmp(line, "running_speed", 13)) out->running_speed = find_float(line, "running_speed", out->running_speed);
        else if (!strncmp(line, "jump_height", 11)) out->jump_height = find_float(line, "jump_height", out->jump_height);
        else if (!strncmp(line, "jump_distancez", 14)) out->jump_distancez = find_float(line, "jump_distancez", out->jump_distancez);
