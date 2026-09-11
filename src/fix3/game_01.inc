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
    int vrest_ticks[LF2_MAX_ACTORS];
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
    /* Jan Angel regeneration: stock hit_Fa:4 applies a 100-tu heal status. */
    int jan_heal_ticks;
    int jan_heal_phase;
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
