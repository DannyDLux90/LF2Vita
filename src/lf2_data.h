#ifndef LF2_DATA_H
#define LF2_DATA_H

#include <stdbool.h>
#include <stdint.h>
#include <vita2d.h>

#define LF2_MAX_FRAMES 400
#define LF2_MAX_SHEETS 8
#define LF2_MAX_ITRS 8
#define LF2_MAX_BDYS 8

typedef struct {
    int kind;
    int x, y, w, h;
    int dvx, dvy;
    int fall, arest, vrest, bdefend, injury, effect, zwidth;
} lf2_itr_def_t;

typedef struct {
    int kind;
    int x, y, w, h;
} lf2_bdy_def_t;

typedef struct {
    bool present;
    int id;
    char label[32];
    int pic, state, wait, next;
    int dvx, dvy, dvz;
    int centerx, centery;
    int hit_a, hit_d, hit_j;
    int hit_Fa, hit_Ua, hit_Da, hit_Fj, hit_Uj, hit_Dj, hit_ja;
    int mp;
    char sound[96];
    lf2_itr_def_t itrs[LF2_MAX_ITRS];
    int itr_count;
    lf2_bdy_def_t bdys[LF2_MAX_BDYS];
    int bdy_count;
} lf2_frame_def_t;

typedef struct {
    int first_pic, last_pic;
    int w, h, row, col;
    char path[128];
    char mirror_path[128];
    vita2d_texture *texture;
    vita2d_texture *mirror_texture;
} lf2_sheet_def_t;

typedef struct {
    char name[32];
    char head[128];
    char small[128];
    float walking_frame_rate;
    float walking_speed, walking_speedz;
    float running_frame_rate;
    float running_speed, running_speedz;
    float jump_height, jump_distance, jump_distancez;
    float dash_height, dash_distance, dash_distancez;
    lf2_sheet_def_t sheets[LF2_MAX_SHEETS];
    int sheet_count;
    lf2_frame_def_t frames[LF2_MAX_FRAMES];
} lf2_character_def_t;

int lf2_load_character(const char *app0_root, const char *dat_rel_path, lf2_character_def_t *out);
int lf2_load_character_textures(const char *app0_root, lf2_character_def_t *ch);
void lf2_free_character_textures(lf2_character_def_t *ch);
void lf2_make_black_transparent(vita2d_texture *tex);
vita2d_texture *lf2_load_bmp_colorkey(const char *filename);
int lf2_normalize_relpath(const char *in, char *out, int cap);
lf2_sheet_def_t *lf2_sheet_for_pic(lf2_character_def_t *ch, int pic);

#endif
