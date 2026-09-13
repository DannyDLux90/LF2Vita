#ifndef LF2_BACKGROUND_H
#define LF2_BACKGROUND_H

#include <stdbool.h>

/* Load one of the nine stock 2.00a backgrounds directly from game.lf2pak.
   The stage index uses the same order as main.c's g_stages table. */
int lf2_background_load(int stage_index);
void lf2_background_unload(void);
void lf2_background_tick(void);
/* Draw authored bg.dat layers for the current camera. Returns true when an
   authored background is active; false lets the caller draw its PNG fallback. */
bool lf2_background_draw(float camera_x);
int lf2_background_loaded_stage(void);

#endif
