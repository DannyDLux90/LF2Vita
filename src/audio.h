#ifndef LF2_AUDIO_H
#define LF2_AUDIO_H

int lf2_audio_init(void);
void lf2_audio_shutdown(void);
void lf2_audio_play(const char *relpath);
int lf2_audio_ready(void);

#endif
