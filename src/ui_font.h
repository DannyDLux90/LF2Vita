#ifndef LF2_UI_FONT_H
#define LF2_UI_FONT_H

#include <vita2d.h>

int lf2_ui_font_init(void);
void lf2_ui_font_shutdown(void);
int lf2_ui_font_ready(void);
float lf2_ui_text_width(float scale, const char *text);
void lf2_ui_draw_text(float x, float y, float scale, unsigned color, const char *text);
void lf2_ui_draw_textf(float x, float y, float scale, unsigned color, const char *fmt, ...);

#endif
