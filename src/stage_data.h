#ifndef LF2_STAGE_DATA_H
#define LF2_STAGE_DATA_H

#define LF2_STAGE_MAX_PHASES 128
#define LF2_STAGE_MAX_SPAWNS 40

typedef struct {
    int object_id;
    int hp;
    int times;
    float ratio;
    int boss;
    int soldier;
    int x; /* authored distance from the current right-hand stage bound */
} lf2_stage_spawn_t;

typedef struct {
    int bound;
    lf2_stage_spawn_t spawns[LF2_STAGE_MAX_SPAWNS];
    int spawn_count;
} lf2_stage_phase_t;

typedef struct {
    int id;
    lf2_stage_phase_t phases[LF2_STAGE_MAX_PHASES];
    int phase_count;
} lf2_stage_t;

/* Parse one stock LF2 stage definition directly from encrypted data/stage.dat. */
int lf2_stage_load(int stage_id, lf2_stage_t *out);

/* Convert stage object ids to the native Vita roster. Returns -1 for non-fighters. */
int lf2_stage_object_to_roster(int object_id);

/* Build the concrete enemy list for one phase. The stock game scales HP by
   difficulty. v0.62 keeps the one-player Normal/Difficult spawn count and uses
   a 2x spawn approximation for CRAZY while preserving the authored order. */
int lf2_stage_build_phase(const lf2_stage_phase_t *phase, int difficulty,
                          int *roster_out, int *hp_out, int cap,
                          unsigned *rng_state);

/* Extract authored pickup objects (100..199) from a stage phase. x_out stores
   the original stage.dat x value; runtime places the pickup at bound-x. */
int lf2_stage_build_items(const lf2_stage_phase_t *phase, int difficulty,
                          int *oid_out, int *x_out, int cap,
                          unsigned *rng_state);

#endif
