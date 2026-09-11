#ifndef LF2_GAME_H
#define LF2_GAME_H
#include <stdbool.h>
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
#define LF2_STAGE_SUMMARY_MAX_PLAYERS (1 + LF2_STAGE_MAX_ALLIES)

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

typedef struct {
    const char *name;
    const char *dat;
    const char *head;
    bool hidden;
} lf2_roster_entry_t;

typedef struct {
    /* Used by Demo mode: the first slot is controlled by the same AI as COMs. */
    bool player_ai;
    /* VS uses the classic X=rematch / Circle=menu result prompt. */
    bool allow_rematch;
    /* Stage/Demo can automatically return after a short KO/result display. */
    bool auto_continue;
    /* Allow Circle to abort a CPU-only demo without opening pause first. */
    bool allow_cancel;
    /* Optional small status string shown in the HUD, e.g. Stage 2-3 / Phase 4. */
    const char *hud_label;
    /* Stage Mode has no round-over splash between authored waves. */
    bool suppress_result_overlay;
    /* Initial state. <=0 selects the regular 500 default. */
    int player_hp;
    int player_max_hp;
    int player_mp;
    /* Optional exact maximum/start HP for each CPU slot. NULL => 500. */
    const int *cpu_hp;
    /* Optional result state for multi-round modes. */
    int *player_hp_out;
    int *player_mp_out;
} lf2_match_options_t;

typedef struct {
    int participant_count;
    int roster[LF2_STAGE_SUMMARY_MAX_PLAYERS];
    int kills[LF2_STAGE_SUMMARY_MAX_PLAYERS];
    int attack[LF2_STAGE_SUMMARY_MAX_PLAYERS];
    int hp_lost[LF2_STAGE_SUMMARY_MAX_PLAYERS];
    int mp_usage[LF2_STAGE_SUMMARY_MAX_PLAYERS];
    int picking[LF2_STAGE_SUMMARY_MAX_PLAYERS];
    int hp[LF2_STAGE_SUMMARY_MAX_PLAYERS];
    int mp[LF2_STAGE_SUMMARY_MAX_PLAYERS];
} lf2_stage_summary_t;

extern const lf2_roster_entry_t lf2_roster[];
extern const int lf2_roster_count;

int lf2_run_match_ex(vita2d_pgf *font, vita2d_texture *stage_bg,
                     int player_index, int player_team,
                     const int *cpu_indices, const int *cpu_teams, int cpu_count,
                     int difficulty, bool cheat_enabled,
                     const lf2_match_options_t *options);

int lf2_run_match(vita2d_pgf *font, vita2d_texture *stage_bg,
                  int player_index, int player_team,
                  const int *cpu_indices, const int *cpu_teams, int cpu_count,
                  int difficulty, bool cheat_enabled);

/* A Stage Mode section is one continuously scrolling world. Each authored
   stage.dat phase extends the right-hand bound and supplies a queue of enemy
   fighters. Up to seven are active at once; additional enemies enter through
   freed slots without tearing down the player/background/GXM scene. */
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
    /* fix3: Stage Mode supports a Vita-controlled P1 plus up to seven
       persistent COM companions, matching the PC Stage party size. Party
       slots and Stage-enemy actor slots are separate in the Vita runtime. */
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
void lf2_get_last_stage_summary(lf2_stage_summary_t *out);
#endif
