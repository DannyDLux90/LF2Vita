#ifndef LF2_GAME_H
#define LF2_GAME_H
#include <stdbool.h>
#include <stdint.h>
#include <vita2d.h>

/* LF2 supports eight fighters total. On a Vita we expose one local human
   player plus up to seven computer slots, matching the original VS screen. */
#define LF2_MAX_CPUS 7
/* Stage Mode separates the eight-player party from the live enemy pool.
   Keep VS at eight fighters, but allow P1 + 7 COM allies plus up to seven
   simultaneously active Stage enemies. */
#define LF2_STAGE_MAX_ALLIES 7
#define LF2_STAGE_ENEMY_SLOTS 7
#define LF2_MAX_ACTORS (1 + LF2_STAGE_MAX_ALLIES + LF2_STAGE_ENEMY_SLOTS)

enum {
    LF2_DIFF_EASY=0,
    LF2_DIFF_NORMAL=1,
    LF2_DIFF_DIFFICULT=2,
    LF2_DIFF_CRAZY=3
};

enum {
    LF2_MATCH_CANCEL=0,
    LF2_MATCH_WIN=1,
    LF2_MATCH_LOSS=2
};

enum {
    LF2_REMOTE_LEFT           = 1u << 0,
    LF2_REMOTE_RIGHT          = 1u << 1,
    LF2_REMOTE_UP             = 1u << 2,
    LF2_REMOTE_DOWN           = 1u << 3,
    LF2_REMOTE_ATTACK         = 1u << 4,
    LF2_REMOTE_JUMP           = 1u << 5,
    LF2_REMOTE_DEFEND         = 1u << 6,
    LF2_REMOTE_PICKUP         = 1u << 7,
    LF2_REMOTE_SPECIAL_ATTACK = 1u << 8,
    LF2_REMOTE_SPECIAL_JUMP   = 1u << 9
};

typedef uint32_t (*lf2_remote_input_fn)(void *userdata);
typedef bool (*lf2_remote_alive_fn)(void *userdata);
typedef void (*lf2_remote_frame_fn)(void *userdata);
typedef bool (*lf2_lockstep_frame_fn)(void *userdata, uint32_t local_held[4], uint32_t remote_held[4]);

typedef struct {
    const char *name;
    const char *dat;
    const char *head;
    bool hidden;
} lf2_roster_entry_t;

typedef struct {
    bool player_ai;
    bool allow_rematch;
    bool auto_continue;
    bool allow_cancel;
    const char *hud_label;
    bool suppress_result_overlay;
    bool stage_clear_overlay;
    int player_hp;
    int player_max_hp;
    int player_mp;
    const int *cpu_hp;
    int *player_hp_out;
    int *player_mp_out;

    int remote_player_slot;
    int remote_player_count;
    int remote_player_slots[4];
    bool lockstep_actor_map_enabled;
    uint8_t lockstep_actor_control[LF2_MAX_CPUS+1];
    lf2_remote_input_fn remote_input;
    lf2_remote_alive_fn remote_alive;
    lf2_remote_frame_fn remote_frame_presented;
    lf2_lockstep_frame_fn lockstep_frame;
    void *remote_userdata;
} lf2_match_options_t;

extern const lf2_roster_entry_t lf2_roster[];
extern const int lf2_roster_count;
/* Map stock data.txt character ids (1..52) to LF2Vita roster indices. */
int lf2_roster_from_original_id(int id);

int lf2_run_match_ex(vita2d_pgf *font, vita2d_texture *stage_bg,
                     int player_index, int player_team,
                     const int *cpu_indices, const int *cpu_teams, int cpu_count,
                     int difficulty, bool cheat_enabled,
                     const lf2_match_options_t *options);

int lf2_run_match(vita2d_pgf *font, vita2d_texture *stage_bg,
                  int player_index, int player_team,
                  const int *cpu_indices, const int *cpu_teams, int cpu_count,
                  int difficulty, bool cheat_enabled);

#define LF2_STAGE_RUNTIME_MAX_PHASES 128
#define LF2_STAGE_RUNTIME_MAX_ENEMIES 64
#define LF2_STAGE_RUNTIME_MAX_ITEMS 24

typedef struct {
    int bound;
    int enemy_count;
    int enemies[LF2_STAGE_RUNTIME_MAX_ENEMIES];
    int hp[LF2_STAGE_RUNTIME_MAX_ENEMIES];
    int item_count;
    int item_oid[LF2_STAGE_RUNTIME_MAX_ITEMS];
    int item_x[LF2_STAGE_RUNTIME_MAX_ITEMS];
} lf2_stage_runtime_phase_t;

typedef struct {
    int phase_count;
    lf2_stage_runtime_phase_t phases[LF2_STAGE_RUNTIME_MAX_PHASES];
    int player_hp;
    int player_mp;
    int *player_hp_out;
    int *player_mp_out;
    int ally_count;
    int ally_roster[LF2_STAGE_MAX_ALLIES];
    int ally_hp[LF2_STAGE_MAX_ALLIES];
    int ally_mp[LF2_STAGE_MAX_ALLIES];
    int *ally_hp_out;
    int *ally_mp_out;
    const char *hud_prefix;
} lf2_stage_runtime_t;

int lf2_run_stage_section(vita2d_pgf *font, vita2d_texture *stage_bg,
                          int player_index, int difficulty, bool cheat_enabled,
                          const lf2_stage_runtime_t *stage);
#endif
