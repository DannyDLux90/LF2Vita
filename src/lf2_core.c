#include "lf2_core.h"

#include <limits.h>
#include <string.h>

static uint32_t distance_xz(const lf2_object_t *a, const lf2_object_t *b) {
    int64_t dx = (int64_t)a->x - (int64_t)b->x;
    int64_t dz = (int64_t)a->z - (int64_t)b->z;
    if (dx < 0) dx = -dx;
    if (dz < 0) dz = -dz;
    uint64_t d = (uint64_t)dx + (uint64_t)dz;
    return d > UINT32_MAX ? UINT32_MAX : (uint32_t)d;
}

void lf2_world_init(lf2_world_t *world, uint32_t seed) {
    if (!world) return;
    memset(world, 0, sizeof(*world));
    world->rng_state = seed ? seed : 0x4C463256u;
}

uint32_t lf2_random(lf2_world_t *world, uint32_t range) {
    if (!world || range == 0) return 0;
    uint32_t x = world->rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    world->rng_state = x;
    return x % range;
}

bool lf2_rect_overlap(int32_t ax, int32_t ay, uint32_t aw, uint32_t ah,
                      int32_t bx, int32_t by, uint32_t bw, uint32_t bh) {
    int64_t ar = (int64_t)ax + aw;
    int64_t ab = (int64_t)ay + ah;
    int64_t br = (int64_t)bx + bw;
    int64_t bb = (int64_t)by + bh;
    return (int64_t)ax < br && ar > bx && (int64_t)ay < bb && ab > by;
}

bool lf2_teleport(lf2_world_t *world, uint32_t object_id, uint32_t mode) {
    if (!world || object_id >= LF2_MAX_OBJECTS) return false;
    lf2_object_t *self = world->objects[object_id];
    if (!self || !self->active) return false;

    lf2_object_t *target = NULL;

    if (mode == LF2_TELEPORT_TO_ENEMY) {
        uint32_t best_distance = UINT32_MAX;
        for (uint32_t i = 0; i < LF2_MAX_OBJECTS; ++i) {
            lf2_object_t *candidate = world->objects[i];
            if (!candidate || !candidate->active || candidate == self) continue;
            if (candidate->type != LF2_TYPE_CHARACTER || candidate->hp <= 0) continue;
            if (candidate->team == self->team) continue;

            uint32_t d = distance_xz(self, candidate);
            if (d < best_distance) {
                best_distance = d;
                target = candidate;
            }
        }

        self->y = 0;
        if (target) {
            self->z = target->z + 1;
            self->x = target->x + (self->facing == 0 ? -120 : 120);
        }
    } else if (mode == LF2_TELEPORT_TO_TEAM) {
        uint32_t best_distance = 0;
        for (uint32_t i = 0; i < LF2_MAX_OBJECTS; ++i) {
            lf2_object_t *candidate = world->objects[i];
            if (!candidate || !candidate->active || candidate == self) continue;
            if (candidate->type != LF2_TYPE_CHARACTER || candidate->hp <= 0) continue;
            if (candidate->team != self->team) continue;

            uint32_t d = distance_xz(self, candidate);
            if (!target || d > best_distance) {
                best_distance = d;
                target = candidate;
            }
        }

        self->y = 0;
        if (target) {
            self->z = target->z + 1;
            self->x = target->x + (self->facing == 0 ? -60 : 60);
        }
    } else {
        return false;
    }

    self->x_position = self->x;
    self->y_position = self->y;
    self->z_position = self->z;
    self->x_velocity = 0.0;
    self->y_velocity = 0.0;
    self->z_velocity = 0.0;
    return target != NULL;
}
