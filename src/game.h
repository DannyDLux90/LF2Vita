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

/* Platform-neutral input bits used by remote/network controllers.  The
   gameplay runtime converts these to the same input_t used by Vita controls. */
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
/* Stock LF2 networking owns four control slots per machine.  local_held[] is
   both input and output: the runtime proposes the current Vita controls in
   slot 0 and the callback may replace them with the stock network clock's
   latched value (original LF2 samples controls on its network cadence).
   remote_held[] returns the four slots owned by the other machine. */
typedef bool (*lf2_lockstep_frame_fn)(void *userdata, uint32_t local_held[4], uint32_t remote_held[4]);

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
    /* Freeze the final battlefield and draw the original-style Stage Clear banner. */
    bool stage_clear_overlay;
    /* Initial state. <=0 selects the regular 500 default. */
    int player_hp;
    int player_max_hp;
    int player_mp;
    /* Optional exact maximum/start HP for each CPU slot. NULL => 500. */
    const int *cpu_hp;
    /* Optional result state for multi-round modes. */
    int *player_hp_out;
    int *player_mp_out;

    /* Optional authoritative remote-player path.  remote_player_slot is the
       fighter index inside this match (1 == first slot after P1).  The Vita
       remains simulation authority; the remote peer contributes only input. */
    int remote_player_slot;
    /* Stock LF2 owns four input slots per computer.  For native stock-PC
       compatibility, map each remote control byte to a fighter actor.  A
       count of zero keeps the legacy single remote_player_slot behaviour. */
    int remote_player_count;
    int remote_player_slots[4];
    /* Full stock-LF2 input map.  When enabled, actor i uses control code
       1..4 for this machine's stock slots and 5..8 for the peer's slots;
       0 leaves that actor under AI control.  This preserves stock actor order
       even when the Vita is the connecting (slots 5..8) machine. */
    bool lockstep_actor_map_enabled;
    uint8_t lockstep_actor_control[LF2_MAX_CPUS+1];
    lf2_remote_input_fn remote_input;
    lf2_remote_alive_fn remote_alive;
    lf2_remote_frame_fn remote_frame_presented;
    /* Optional original-LF2-compatible blocking input clock.  When set, the
       runtime disables Vita-only shortcut buttons and advances each gameplay
       tick only after this callback returns the peer input for that tick. */
    lf2_lockstep_frame_fn lockstep_frame;
    void *remote_userdata;
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
#endif
