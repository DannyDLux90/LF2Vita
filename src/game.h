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

typedef struct {
    const char *name;
    const char *dat;
    const char *head;
    bool hidden;
} lf2_roster_entry_t;

extern const lf2_roster_entry_t lf2_roster[];
extern const int lf2_roster_count;

int lf2_run_match(vita2d_pgf *font, vita2d_texture *stage_bg,
                  int player_index, int player_team,
                  const int *cpu_indices, const int *cpu_teams, int cpu_count,
                  int difficulty, bool cheat_enabled);
#endif
