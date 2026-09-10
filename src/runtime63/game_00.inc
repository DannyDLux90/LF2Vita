#include "game.h"
#include "audio.h"
#include "lf2_data.h"
#include "lf2_core.h"
#include "log.h"
#include "ui_font.h"

#include <math.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/threadmgr.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_W 960
#define SCREEN_H 544
#define GAME_ROOT "app0:/game/LittleFighter"
#define GROUND_Y 420.0f
#define SPRITE_SCALE 1.0f

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


const lf2_roster_entry_t lf2_roster[] = {
    {"Deep",     "data/deep.dat",     "sprite/sys/deep_f.bmp",     false},
    {"John",     "data/john.dat",     "sprite/sys/john_f.bmp",     false},
    {"Henry",    "data/henry.dat",    "sprite/sys/henry_f.bmp",    false},
    {"Rudolf",   "data/rudolf.dat",   "sprite/sys/rudolf_f.bmp",   false},
    {"Louis",    "data/louis.dat",    "sprite/sys/louis_f.bmp",    false},
    {"Firen",    "data/firen.dat",    "sprite/sys/firen_f.bmp",    false},
    {"Freeze",   "data/freeze.dat",   "sprite/sys/freeze_f.bmp",   false},
    {"Dennis",   "data/dennis.dat",   "sprite/sys/dennis_f.bmp",   false},
    {"Woody",    "data/woody.dat",    "sprite/sys/woody_f.bmp",    false},
    {"Davis",    "data/davis.dat",    "sprite/sys/davis_f.bmp",    false},
    {"Bandit",   "data/bandit.dat",   "sprite/sys/bandit_f.bmp",   true},
    {"Hunter",   "data/hunter.dat",   "sprite/sys/hunter_f.bmp",   true},
    {"Mark",     "data/mark.dat",     "sprite/sys/mark_f.bmp",     true},
    {"Jack",     "data/jack.dat",     "sprite/sys/jack_f.bmp",     true},
    {"Sorcerer", "data/sorcerer.dat", "sprite/sys/sorcerer_f.bmp", true},
    {"Monk",     "data/monk.dat",     "sprite/sys/monk_f.bmp",     true},
    {"Jan",      "data/jan.dat",      "sprite/sys/jan_f.bmp",      true},
    {"Knight",   "data/knight.dat",   "sprite/sys/knight_f.bmp",   true},
    {"Justin",   "data/justin.dat",   "sprite/sys/justin_f.bmp",   true},
    {"Bat",      "data/bat.dat",      "sprite/sys/bat_f.bmp",      true},
    {"LouisEX",  "data/louisEX.dat",  "sprite/sys/louisEX_f.bmp",  true},
    {"Firzen",   "data/firzen.dat",   "sprite/sys/firzen_f.bmp",   true},
    {"Julian",   "data/julian.dat",   "sprite/sys/julian_f.bmp",   true},
};
const int lf2_roster_count = (int)(sizeof(lf2_roster)/sizeof(lf2_roster[0]));

typedef struct {
    bool left,right,up,down,attack,jump,defend,special,run;
    bool attack_pressed,jump_pressed,defend_pressed,special_pressed;
} input_t;

typedef struct {
    lf2_character_def_t def;
    float x,y,z;
    float vx,vy,vz;
    int facing; /* 0 right, 1 left */
    int hp,max_hp,mp;
    int frame;
    int ticks;
    int walk_phase;
    int punch_variant;
    int invuln;
    int ai_cooldown;
    bool ai;
    int team; /* 0 = Independent, 1..4 = Team */
    bool dead;
    bool hit_latched;
    bool opoint_fired;
} fighter_t;

#define LF2_MAX_ACTIVE_OBJECTS 64
#define LF2_MAX_OBJECT_CACHE 32

typedef struct {
    int oid;
    int type;
    const char *dat;
} object_catalog_entry_t;

/* Stock 2.00a object ids from data/data.txt. Type 3 entries are LF2 energy
   objects/balls; type 1 entries are light weapons such as Henry's arrows.
   Keeping this table explicit makes the Vita runtime deterministic and avoids
   depending on the Windows executable's object registry. */
static const object_catalog_entry_t g_object_catalog[] = {
    {200,3,"data/john_ball.dat"},      {201,1,"data/henry_arrow1.dat"},
    {202,1,"data/rudolf_weapon.dat"}, {203,3,"data/deep_ball.dat"},
    {204,3,"data/henry_wind.dat"},    {205,3,"data/dennis_ball.dat"},
    {206,3,"data/woody_ball.dat"},    {207,3,"data/davis_ball.dat"},
    {208,3,"data/henry_arrow2.dat"},  {209,3,"data/freeze_ball.dat"},
    {210,3,"data/firen_ball.dat"},    {211,3,"data/firen_flame.dat"},
    {212,3,"data/freeze_column.dat"}, {213,1,"data/weapon7.dat"},
    {214,3,"data/john_biscuit.dat"},  {215,3,"data/dennis_chase.dat"},
    {216,3,"data/jack_ball.dat"},     {219,3,"data/jan_chaseh.dat"},
    {220,3,"data/jan_chase.dat"},     {221,3,"data/firzen_chasef.dat"},
    {222,3,"data/firzen_chasei.dat"}, {223,3,"data/firzen_ball.dat"},
    {224,3,"data/bat_ball.dat"},      {225,3,"data/bat_chase.dat"},
    {226,3,"data/justin_ball.dat"},   {228,3,"data/julian_ball.dat"},
    {229,3,"data/julian_ball2.dat"}
};

typedef struct {
    bool used;
    int oid;
    int type;
    lf2_character_def_t def;
} object_cache_entry_t;

typedef struct {
    bool active;
    int oid;
    int type;
    int owner;
    int team;
    int frame;
    int ticks;
    int age;
    int facing;
    float x,y,z;
    float vx,vy,vz;
    uint32_t hit_mask;
    bool opoint_fired;
    object_cache_entry_t *cache;
} world_object_t;

typedef struct {
    world_object_t objects[LF2_MAX_ACTIVE_OBJECTS];
    object_cache_entry_t cache[LF2_MAX_OBJECT_CACHE];
    int cache_count;
} object_world_t;

static float clampf(float v,float a,float b){return v<a?a:(v>b?b:v);}

static lf2_frame_def_t *frame_def(fighter_t *f) {
    if (!f || f->frame<0 || f->frame>=LF2_MAX_FRAMES || !f->def.frames[f->frame].present) return NULL;
    return &f->def.frames[f->frame];
}

static float lf2_axis_value(int v) {
    /* LF2 uses 550 as an explicit "stop this axis" command. 0 means keep
       the current velocity. Treating 550 as a literal velocity caused severe
       teleport / animation glitches, especially for Woody. */
    return v == 550 ? 0.0f : (float)v;
}

static void set_frame(fighter_t *f,int id) {
    if (!f) return;
    int old_frame=f->frame;

    if (id<0 || id>=LF2_MAX_FRAMES || !f->def.frames[id].present) id=0;
    f->frame=id; f->ticks=0; f->opoint_fired=false;
    lf2_frame_def_t *fr=frame_def(f);
    if (fr) {
        if(!f->ai && old_frame!=id && (old_frame>=60 || id>=60))
            lf2_logf("ANIM","P1 %s frame %d -> %d pic=%d wait=%d next=%d state=%d",
                     f->def.name,old_frame,id,fr->pic,fr->wait,fr->next,fr->state);
        if (fr->dvx == 550) f->vx = 0.0f;
        else if (fr->dvx) f->vx = (float)(f->facing ? -fr->dvx : fr->dvx);
        if (fr->dvy == 550) f->vy = 0.0f;
        else if (fr->dvy) f->vy = lf2_axis_value(fr->dvy);
        if (fr->dvz == 550) f->vz = 0.0f;
        else if (fr->dvz) f->vz = lf2_axis_value(fr->dvz);
        if (fr->sound[0]) lf2_audio_play(fr->sound);
    }
}
