#ifndef OPENLF2_VITA_CORE_H
#define OPENLF2_VITA_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LF2_MAX_OBJECTS 400u

enum {
    LF2_NO_TEAM = 0,
    LF2_TEAM_1 = 1,
    LF2_TEAM_2 = 2,
    LF2_STAGE_ENEMY_TEAM = 5
};

enum {
    LF2_TYPE_CHARACTER = 0
};

enum {
    LF2_TELEPORT_TO_ENEMY = 1,
    LF2_TELEPORT_TO_TEAM = 2
};

typedef struct lf2_object {
    bool active;
    uint32_t id;
    uint32_t type;
    uint32_t team;
    int32_t hp;
    uint8_t facing;
    int32_t x;
    int32_t y;
    int32_t z;
    double x_position;
    double y_position;
    double z_position;
    double x_velocity;
    double y_velocity;
    double z_velocity;
} lf2_object_t;

typedef struct lf2_world {
    lf2_object_t *objects[LF2_MAX_OBJECTS];
    uint32_t rng_state;
} lf2_world_t;

void lf2_world_init(lf2_world_t *world, uint32_t seed);
uint32_t lf2_random(lf2_world_t *world, uint32_t range);
bool lf2_rect_overlap(int32_t ax, int32_t ay, uint32_t aw, uint32_t ah,
                      int32_t bx, int32_t by, uint32_t bw, uint32_t bh);
bool lf2_teleport(lf2_world_t *world, uint32_t object_id, uint32_t mode);

#endif
