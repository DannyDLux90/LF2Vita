#ifndef LF2_REPLAY_REF_H
#define LF2_REPLAY_REF_H

#include <stdint.h>

#define LF2_REF_PLAYER_COUNT 8
#define LF2_REF_RNG_LEN 3000

typedef struct {
    int role;       /* -1 empty, 0 COM, 1 human */
    int character;  /* original data.txt id */
    int team;       /* original LF2 team value */
    int kill;
    int attack;
    int hp_used;
    int mp_used;
    int picking;
    int status;
    char name[12];
} lf2_ref_player_t;

typedef struct {
    int mode;
    int difficulty; /* stock recording values: -1 crazy, 0 difficult, 1 normal, 2 easy */
    int background;
    int stage_raw;
    uint32_t movie_tus;
    uint32_t packet_count;
    uint32_t flags;
    lf2_ref_player_t players[LF2_REF_PLAYER_COUNT];
    uint8_t rng[LF2_REF_RNG_LEN];
    uint8_t *keys;  /* packet_count * 4 local stock key bytes */
    uint32_t tu;
} lf2_ref_replay_t;

int lf2_ref_replay_load(const char *path, lf2_ref_replay_t *out);
void lf2_ref_replay_free(lf2_ref_replay_t *r);
void lf2_ref_replay_reset(lf2_ref_replay_t *r);
int lf2_ref_replay_enable_rng(const lf2_ref_replay_t *r);
int lf2_ref_replay_vita_difficulty(const lf2_ref_replay_t *r);
int lf2_ref_replay_vita_background(const lf2_ref_replay_t *r);
/* Compatible with lf2_lockstep_frame_fn.  A recording stores the four local
   control slots only; the stock demos use slot 1 and leave slots 2..4 idle.
   One 4-byte sample is held for two 30-Hz game TUs, exactly like the original
   15-Hz network clock. */
int lf2_ref_replay_clock(void *userdata, uint32_t local_held[4], uint32_t remote_held[4]);

#endif
