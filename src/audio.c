#include "audio.h"
#include "asset_pack.h"
#include "log.h"

#include <psp2/audioout.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * LF2 2.00a ships effects at many unusual sample rates (10 kHz..44.1 kHz).
 * v0.40 stepped through those sources with nearest-neighbour resampling and
 * hard-clipped the sum of all voices.  On Vita speakers that sounds very
 * gritty.  v0.50 converts every effect once to 48 kHz with linear interpolation
 * and uses a headroom-aware mixer with a gentle limiter.
 */
#define AUDIO_RATE 48000
#define AUDIO_FRAMES 1024
#define AUDIO_VOICES 16
#define AUDIO_MAX_SOUNDS 128
#define PATH_CAP 96
#define RETRIGGER_US 14000ULL

typedef struct {
    char path[PATH_CAP];
    int16_t *samples;          /* mono, pre-resampled to AUDIO_RATE */
    uint32_t frames;
    uint32_t source_rate;
    uint64_t last_play_us;
} sound_t;

typedef struct {
    const sound_t *sound;
    uint32_t pos;
    uint32_t serial;
    int active;
} voice_t;

static sound_t g_sounds[AUDIO_MAX_SOUNDS];
static int g_sound_count;
static voice_t g_voices[AUDIO_VOICES];
static volatile int g_lock;
static volatile int g_running;
static int g_port=-1;
static SceUID g_thread=-1;
static uint32_t g_serial;
static sound_t g_music;
static uint32_t g_music_pos;
static volatile int g_music_playing;
static int g_music_loaded;

static void lock_audio(void){ while(__sync_lock_test_and_set(&g_lock,1)) sceKernelDelayThread(50); }
static void unlock_audio(void){ __sync_lock_release(&g_lock); }

static uint16_t rd16(const unsigned char *p){ return (uint16_t)(p[0]|((uint16_t)p[1]<<8)); }
static uint32_t rd32(const unsigned char *p){ return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }

static int16_t clamp16(int32_t v){ return (int16_t)(v>32767?32767:(v<-32768?-32768:v)); }

static int decode_wav(const unsigned char *buf,size_t size,sound_t *out){
    if(!buf||size<44||!out||memcmp(buf,"RIFF",4)||memcmp(buf+8,"WAVE",4))return -1;
    uint16_t fmt=0,ch=0,bits=0;uint32_t rate=0;const unsigned char *pcm=NULL;uint32_t pcm_size=0;
    size_t off=12;
    while(off+8<=size){
        const unsigned char *h=buf+off;uint32_t n=rd32(h+4);size_t data=off+8;if(data+n>size)break;
        if(!memcmp(h,"fmt ",4)&&n>=16){fmt=rd16(buf+data);ch=rd16(buf+data+2);rate=rd32(buf+data+4);bits=rd16(buf+data+14);}
        else if(!memcmp(h,"data",4)){pcm=buf+data;pcm_size=n;}
        off=data+n+(n&1u);
    }
    if(fmt!=1||!pcm||!rate||(ch!=1&&ch!=2)||(bits!=8&&bits!=16))return -2;
    uint32_t bps=(uint32_t)(bits/8);uint32_t src_frames=pcm_size/(bps*ch);if(!src_frames)return -3;

    int16_t *src=(int16_t*)malloc((size_t)src_frames*sizeof(int16_t));if(!src)return -4;
    for(uint32_t i=0;i<src_frames;i++){
        int32_t sum=0;
        for(uint32_t c=0;c<ch;c++){
            size_t pos=((size_t)i*ch+c)*bps;
            int32_t v=(bits==8)?(((int32_t)pcm[pos]-128)<<8):(int16_t)rd16(pcm+pos);
            sum+=v;
        }
        src[i]=(int16_t)(sum/(int32_t)ch);
    }

    uint64_t out64=((uint64_t)src_frames*AUDIO_RATE + rate/2u)/rate;
    if(out64<1)out64=1;if(out64>0x7fffffffULL){free(src);return -5;}
    uint32_t dst_frames=(uint32_t)out64;
    int16_t *dst=(int16_t*)malloc((size_t)dst_frames*sizeof(int16_t));if(!dst){free(src);return -6;}

    if(src_frames==1){for(uint32_t i=0;i<dst_frames;i++)dst[i]=src[0];}
    else {
        /* 32.32 fixed point linear interpolation.  This avoids the strong
           imaging/aliasing audible in v0.40's nearest-neighbour path. */
        uint64_t step=((uint64_t)rate<<32)/AUDIO_RATE;
        uint64_t phase=0;
        for(uint32_t i=0;i<dst_frames;i++,phase+=step){
            uint32_t a=(uint32_t)(phase>>32);uint32_t frac=(uint32_t)phase;
            if(a>=src_frames-1){dst[i]=src[src_frames-1];continue;}
            int32_t s0=src[a],s1=src[a+1];
            int64_t mix=(int64_t)s0*(int64_t)(0x100000000ULL-frac)+(int64_t)s1*(int64_t)frac;
            dst[i]=clamp16((int32_t)(mix>>32));
        }
    }
    free(src);
    out->samples=dst;out->frames=dst_frames;out->source_rate=rate;out->last_play_us=0;return 0;
}

static sound_t *find_sound_mut(const char *path){
    if(!path||!*path)return NULL;
    for(int i=0;i<g_sound_count;i++)if(!strcmp(g_sounds[i].path,path))return &g_sounds[i];
    return NULL;
}


static int read_file_all(const char *path,unsigned char **out,size_t *out_n){
    if(!path||!out||!out_n)return -1;*out=NULL;*out_n=0;
    SceUID fd=sceIoOpen(path,SCE_O_RDONLY,0);if(fd<0)return fd;
    SceOff end=sceIoLseek(fd,0,SCE_SEEK_END);if(end<=0){sceIoClose(fd);return -2;}
    sceIoLseek(fd,0,SCE_SEEK_SET);unsigned char *buf=(unsigned char*)malloc((size_t)end);if(!buf){sceIoClose(fd);return -3;}
    size_t got=0;while(got<(size_t)end){int r=sceIoRead(fd,buf+got,(SceSize)((size_t)end-got));if(r<=0)break;got+=(size_t)r;}sceIoClose(fd);
    if(got!=(size_t)end){free(buf);return -4;}*out=buf;*out_n=got;return 0;
}

static void load_menu_music(void){
    memset(&g_music,0,sizeof(g_music));g_music_pos=0;g_music_playing=0;g_music_loaded=0;
    unsigned char *raw=NULL;size_t n=0;int rc=read_file_all("app0:/assets/main_bgm.wav",&raw,&n);
    if(rc<0){lf2_logf("WARN","menu music read failed rc=%d",rc);return;}
    rc=decode_wav(raw,n,&g_music);free(raw);
    if(rc<0){lf2_logf("WARN","menu music decode failed rc=%d",rc);return;}
    snprintf(g_music.path,sizeof(g_music.path),"assets/main_bgm.wav");g_music_loaded=1;
    lf2_logf("AUDIO","menu music ready frames=%u src_rate=%u",(unsigned)g_music.frames,(unsigned)g_music.source_rate);
}

static int choose_voice(void){
    for(int i=0;i<AUDIO_VOICES;i++)if(!g_voices[i].active)return i;
    /* If all voices are busy, replace the one closest to its natural end.
       Replacing slot 0 unconditionally (v0.40) caused audible discontinuities. */
    int best=0;uint32_t best_remaining=0xffffffffu;
    for(int i=0;i<AUDIO_VOICES;i++){
        const sound_t *s=g_voices[i].sound;
        uint32_t rem=(s&&g_voices[i].pos<s->frames)?(s->frames-g_voices[i].pos):0;
        if(rem<best_remaining){best=i;best_remaining=rem;}
    }
    return best;
}

static int audio_thread(SceSize args,void *argp){
    (void)args;(void)argp;static int16_t mix[AUDIO_FRAMES*2];
    lf2_logf("AUDIO","thread start port=%d rate=%d frames=%d voices=%d",g_port,AUDIO_RATE,AUDIO_FRAMES,AUDIO_VOICES);
    while(g_running){
        lock_audio();
        for(int i=0;i<AUDIO_FRAMES;i++){
            int64_t acc=0;int active=0;
            if(g_music_playing&&g_music_loaded&&g_music.samples&&g_music.frames){
                if(g_music_pos>=g_music.frames)g_music_pos=0;
                /* Original main.wma is background music. Keep it below combat/UI
                   effects so menu feedback stays crisp on the Vita speakers. */
                acc+=(int32_t)g_music.samples[g_music_pos++]*3/8;active++;
            }
            for(int v=0;v<AUDIO_VOICES;v++)if(g_voices[v].active){
                const sound_t *s=g_voices[v].sound;uint32_t p=g_voices[v].pos;
                if(!s||p>=s->frames){g_voices[v].active=0;continue;}
                int32_t sample=s->samples[p++];g_voices[v].pos=p;
                /* Tiny end fade prevents a click when a source ends on a
                   non-zero sample. Keep the attack transient intact. */
                uint32_t remain=s->frames-p;
                if(remain<48)sample=(int32_t)((int64_t)sample*(int64_t)remain/48);
                acc+=sample;active++;
                if(p>=s->frames)g_voices[v].active=0;
            }
            if(active>1){
                /* Preserve a single effect at full level, but create increasing
                   headroom as effects overlap. */
                int denom=1024+(active-1)*480;
                acc=acc*1024/denom;
            }
            /* Gentle knee instead of v0.40's brick-wall clipping. */
            int64_t a=acc<0?-acc:acc;
            if(a>28000){a=28000+(a-28000)/5;if(a>32767)a=32767;acc=acc<0?-a:a;}
            int16_t smp=(int16_t)acc;mix[i*2]=smp;mix[i*2+1]=smp;
        }
        unlock_audio();
        int rc=sceAudioOutOutput(g_port,mix);if(rc<0){lf2_logf("ERROR","sceAudioOutOutput rc=0x%08X",rc);sceKernelDelayThread(10000);}
    }
    if(g_port>=0)sceAudioOutOutput(g_port,NULL);
    lf2_logf("AUDIO","thread stop");return 0;
}

int lf2_audio_init(void){
    if(g_thread>=0)return 0;g_sound_count=0;g_lock=0;g_serial=0;memset(g_voices,0,sizeof(g_voices));load_menu_music();
    int total=lf2_pak_count();size_t pcm_bytes=0;
    for(int i=0;i<total&&g_sound_count<AUDIO_MAX_SOUNDS;i++){
        const char *name=lf2_pak_entry_name(i);size_t ln=name?strlen(name):0;
        if(!name||ln<4||strcmp(name+ln-4,".wav"))continue;
        void *raw=NULL;size_t raw_n=0;int rc=lf2_pak_read_index(i,&raw,&raw_n);if(rc<0){lf2_logf("WARN","audio read failed %s rc=%d",name,rc);continue;}
        sound_t *s=&g_sounds[g_sound_count];memset(s,0,sizeof(*s));rc=decode_wav((const unsigned char*)raw,raw_n,s);free(raw);
        if(rc<0){lf2_logf("WARN","audio decode skipped %s rc=%d",name,rc);continue;}
        strncpy(s->path,name,sizeof(s->path)-1);pcm_bytes+=(size_t)s->frames*sizeof(int16_t);g_sound_count++;
    }
    g_port=sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN,AUDIO_FRAMES,AUDIO_RATE,SCE_AUDIO_OUT_MODE_STEREO);
    if(g_port<0){lf2_logf("ERROR","sceAudioOutOpenPort failed rc=0x%08X sounds=%d",g_port,g_sound_count);return -1;}
    int vols[2]={SCE_AUDIO_OUT_MAX_VOL,SCE_AUDIO_OUT_MAX_VOL};int vrc=sceAudioOutSetVolume(g_port,SCE_AUDIO_VOLUME_FLAG_L_CH|SCE_AUDIO_VOLUME_FLAG_R_CH,vols);
    lf2_logf("AUDIO","HQ cache ready sounds=%d resampled_pcm_bytes=%u rate=%d port=%d volume_rc=0x%08X",g_sound_count,(unsigned)pcm_bytes,AUDIO_RATE,g_port,vrc);
    g_running=1;g_thread=sceKernelCreateThread("lf2_audio",audio_thread,0x10000100,0x10000,0,0,NULL);
    if(g_thread<0){lf2_logf("ERROR","audio thread create failed rc=0x%08X",g_thread);g_running=0;sceAudioOutReleasePort(g_port);g_port=-1;return -2;}
    int rc=sceKernelStartThread(g_thread,0,NULL);if(rc<0){lf2_logf("ERROR","audio thread start failed rc=0x%08X",rc);g_running=0;sceKernelDeleteThread(g_thread);g_thread=-1;sceAudioOutReleasePort(g_port);g_port=-1;return -3;}
    return 0;
}

void lf2_audio_play(const char *relpath){
    sound_t *s=find_sound_mut(relpath);if(!s){static int misses=0;if(misses<20){lf2_logf("WARN","sound not cached: %s",relpath?relpath:"(null)");misses++;}return;}
    uint64_t now=sceKernelGetProcessTimeWide();
    if(s->last_play_us&&now-s->last_play_us<RETRIGGER_US)return;
    s->last_play_us=now;
    lock_audio();int slot=choose_voice();g_voices[slot].sound=s;g_voices[slot].pos=0;g_voices[slot].serial=++g_serial;g_voices[slot].active=1;unlock_audio();
    lf2_logf("AUDIO","play slot=%d path=%s src_rate=%u out_frames=%u",slot,s->path,(unsigned)s->source_rate,(unsigned)s->frames);
}

int lf2_audio_ready(void){return g_port>=0&&g_thread>=0;}

void lf2_audio_music_start(void){
    if(!g_music_loaded)return;lock_audio();g_music_playing=1;unlock_audio();
    lf2_logf("AUDIO","menu music start pos=%u",(unsigned)g_music_pos);
}
void lf2_audio_music_stop(void){
    lock_audio();g_music_playing=0;g_music_pos=0;unlock_audio();
    lf2_logf("AUDIO","menu music stop");
}
int lf2_audio_music_ready(void){return g_music_loaded;}

void lf2_audio_shutdown(void){
    if(g_thread>=0){g_running=0;sceKernelWaitThreadEnd(g_thread,NULL,NULL);sceKernelDeleteThread(g_thread);g_thread=-1;}
    if(g_port>=0){sceAudioOutReleasePort(g_port);g_port=-1;}
    for(int i=0;i<g_sound_count;i++){free(g_sounds[i].samples);g_sounds[i].samples=NULL;}
    free(g_music.samples);g_music.samples=NULL;g_music_loaded=0;g_music_playing=0;g_music_pos=0;
    lf2_logf("AUDIO","shutdown sounds=%d",g_sound_count);g_sound_count=0;
}
