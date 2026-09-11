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
    bool left,right,up,down,attack,jump,defend,run,pickup;
    /* Vita shortcuts are split into the two LF2 command families so hidden
       characters without hit_Fa (Firzen/Jan) remain fully controllable. */
    bool special_attack,special_jump;
    bool attack_pressed,jump_pressed,defend_pressed,pickup_pressed;
    bool special_attack_pressed,special_jump_pressed;
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
    bool active;
    int team; /* 0 = Independent, 1..4 = Team */
    bool dead;
    bool hit_latched; /* legacy field; v0.69 uses arest/vrest instead */
    int arest_ticks;
    int vrest_ticks[LF2_MAX_CPUS+1];
    int holding_slot;
    int caught_by;
    int thrown_by;
    bool opoint_fired;
    int fall_points;
    bool falling;
    int hit_flash;
    /* v0.65: authored LF2 status/reaction state. These timers are simulation
       ticks (30 Hz), not render frames. */
    int frozen_ticks;
    int burn_ticks;
    int sonata_ticks;
    int sonata_damage_cooldown;
    int status_age;
    int mp_regen_tick;
    /* v0.66 hard-coded LF2 type-0 mechanics that are not expressible by
       ordinary frame chaining alone. */
    int roster_idx;
    int transform_origin;      /* Rudolf roster index while impersonating, -1 otherwise */
    int transform_target;
    int fusion_ticks;          /* remaining Firzen fusion time units */
    int fusion_return_roster;
    int fusion_partner_roster;
    int fusion_partner_slot;
    int fusion_partner_hp;
    int fusion_partner_mp;
    int fusion_team;
    bool fusion_partner_ai;
    bool clone;
    int grabbed_ticks;
    int ice_sword_ticks;
    /* v0.66 stock pickup/weapon state. held_oid uses original data.txt IDs. */
    int held_oid;
    int held_units;
    bool drinking;
    int drink_tick;
    unsigned frame_serial;
    unsigned state_processed_serial;
    vita2d_texture *hud_face;
    /* Stage Mode can share one preloaded read-only DAT/texture set between
       several enemy slots. Ordinary match fighters own their assets. */
    bool owns_assets;
} fighter_t;

static bool g_runtime_cheat=false;
static int load_fighter(fighter_t *f,int roster_idx,bool ai,int team,float x,float z,int facing,int slot,int hp);
static void unload_fighter_slot(fighter_t *f);
static int fighter_change_form(fighter_t *f,int roster_idx,int start_frame,const char *reason);
static int item_is_bottle(int oid);
static int item_is_heavy(int oid);

#define LF2_MAX_ACTIVE_OBJECTS 64
#define LF2_MAX_OBJECT_CACHE 48

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
    /* Stock drop/pickup objects from data/data.txt. */
    {100,1,"data/weapon0.dat"}, {101,1,"data/weapon2.dat"},
    {120,1,"data/weapon4.dat"}, {121,4,"data/weapon5.dat"},
    {122,6,"data/weapon6.dat"}, {123,6,"data/weapon8.dat"},
    {124,1,"data/weapon9.dat"}, {150,2,"data/weapon1.dat"},
    {151,2,"data/weapon3.dat"},
    {200,3,"data/john_ball.dat"},      {201,1,"data/henry_arrow1.dat"},
    {202,1,"data/rudolf_weapon.dat"}, {203,3,"data/deep_ball.dat"},
    {204,3,"data/henry_wind.dat"},    {205,3,"data/dennis_ball.dat"},
    {206,3,"data/woody_ball.dat"},    {207,3,"data/davis_ball.dat"},
    {208,3,"data/henry_arrow2.dat"},  {209,3,"data/freeze_ball.dat"},
    {210,3,"data/firen_ball.dat"},    {211,3,"data/firen_flame.dat"},
    {212,3,"data/freeze_column.dat"}, {213,1,"data/weapon7.dat"},
    {214,3,"data/john_biscuit.dat"},  {215,3,"data/dennis_chase.dat"},
    {216,3,"data/jack_ball.dat"},     {217,2,"data/weapon10.dat"},
    {218,2,"data/weapon11.dat"},      {219,3,"data/jan_chaseh.dat"},
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
    uint32_t hit_mask; /* legacy field; v0.69 uses arest/vrest instead */
    int arest_ticks;
    int vrest_ticks[LF2_MAX_CPUS+1];
    bool opoint_fired;
    bool attached;
    bool pickup_item;
    bool recoverable_weapon;
    bool grounded;
    int durability;
    float attach_dx,attach_dy,attach_dz;
    object_cache_entry_t *cache;
} world_object_t;

typedef struct {
    bool active;
    float x,y,z,vx,vy;
    int life;
    int kind; /* 0 hit spark, 1 blood, 2 fire, 3 ice */
} hit_fx_t;

#define LF2_MAX_HIT_FX 96
#define LF2_MAX_STAGE_CORPSES 192
typedef struct {
    bool active;
    const lf2_character_def_t *def; /* owned by the Stage asset cache */
    int frame;
    int facing;
    float x,y,z;
} stage_corpse_t;

typedef struct {
    world_object_t objects[LF2_MAX_ACTIVE_OBJECTS];
    hit_fx_t fx[LF2_MAX_HIT_FX];
    object_cache_entry_t cache[LF2_MAX_OBJECT_CACHE];
    int cache_count;
    stage_corpse_t corpses[LF2_MAX_STAGE_CORPSES];
    int corpse_count;
} object_world_t;

static object_cache_entry_t *object_cache_find_loaded(object_world_t *w,int oid);
static int held_item_frame_and_pose(object_world_t *w,const fighter_t *f,
                                    object_cache_entry_t **cache_out,int *frame_out,
                                    float *x_out,float *y_out,int *cover_out);

static float clampf(float v,float a,float b){return v<a?a:(v>b?b:v);}

/* Regular VS matches use one 960 px arena. Stage mode temporarily widens the
   world and moves the camera through it without restarting the match. */
static float g_world_min_x=40.0f;
static float g_world_max_x=920.0f;
static float g_camera_x=0.0f;

static float screen_x(float world_x){ return world_x-g_camera_x; }

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
    f->frame=id; f->ticks=0; f->opoint_fired=false; f->frame_serial++;
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

static void set_frame_signed(fighter_t *f,int id){
    if(!f||id==0)return;
    if(id<0){f->facing^=1;id=-id;}
    if(id>0&&id<LF2_MAX_FRAMES&&f->def.frames[id].present)set_frame(f,id);
}

static bool in_action(const fighter_t *f) {
    if(f && (f->falling||f->frozen_ticks>0||f->burn_ticks>0||f->sonata_ticks>0))return true;
    int n=f->frame;
    /* 20..58 are the stock light/heavy weapon attack, throw and drink action
       chains. Treating them as idle lets movement overwrite frame 20 on the
       next tick, which was one reason held-item animation looked wrong. */
    if (n>=20 && n<60) return true;
    if (n>=60 && n<110) return true;
    if (n>=115 && n<=117) return true;
    if (n>=120 && n<=144) return true;
    if (n>=232 && n<=234) return true;
    if (n>=220) return true;
    if (n>=240) return true;
    return false;
}
