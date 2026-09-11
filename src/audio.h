#ifndef LF2_AUDIO_H
#define LF2_AUDIO_H

int lf2_audio_init(void);
void lf2_audio_shutdown(void);
void lf2_audio_play(const char *relpath);
int lf2_audio_ready(void);
void lf2_audio_music_start(void);
void lf2_audio_music_stop(void);
int lf2_audio_music_ready(void);

#endif
