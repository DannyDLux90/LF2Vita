#ifndef LF2_GAME_H
#define LF2_GAME_H
#include <stdbool.h>
#include <vita2d.h>

/* LF2 supports eight fighters total. On a Vita we expose one local human
   player plus up to seven computer slots, matching the original VS screen. */
#define LF2_MAX_CPUS 7

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
#endif
