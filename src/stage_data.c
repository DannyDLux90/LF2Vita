#include "stage_data.h"
#include "asset_pack.h"
#include "game.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const unsigned char k_cipher[] = "odBearBecauseHeIsVeryGoodSiuHungIsAGo";

static int decode_stage_text(char **out){
    void *raw=NULL;size_t size=0;
    int rc=lf2_pak_read("data/stage.dat",&raw,&size);
    if(rc<0||!raw||size<=123){free(raw);return -1;}
    size_t n=size-123,keyn=sizeof(k_cipher)-1;
    char *text=(char*)malloc(n+1);if(!text){free(raw);return -2;}
    const unsigned char *src=(const unsigned char*)raw;
    for(size_t i=0;i<n;i++)text[i]=(char)((unsigned char)src[i+123]-k_cipher[i%keyn]);
    text[n]=0;free(raw);*out=text;return 0;
}

static int line_int(const char *line,const char *key,int fallback){
    const char *p=strstr(line,key);if(!p)return fallback;p+=strlen(key);
    while(*p==' '||*p=='\t')p++;return (int)strtol(p,NULL,10);
}
static float line_float(const char *line,const char *key,float fallback){
    const char *p=strstr(line,key);if(!p)return fallback;p+=strlen(key);
    while(*p==' '||*p=='\t')p++;return strtof(p,NULL);
}

int lf2_stage_load(int stage_id,lf2_stage_t *out){
    if(!out)return -1;memset(out,0,sizeof(*out));out->id=stage_id;
    char *text=NULL;if(decode_stage_text(&text)<0)return -2;
    int in_stage=0;lf2_stage_phase_t *phase=NULL;char *save=NULL;
    for(char *line=strtok_r(text,"\n",&save);line;line=strtok_r(NULL,"\n",&save)){
        while(*line==' '||*line=='\t'||*line=='\r')line++;
        size_t ln=strlen(line);while(ln&&(line[ln-1]=='\r'||line[ln-1]==' '||line[ln-1]=='\t'))line[--ln]=0;
        if(!strncmp(line,"<stage>",7)){
            int id=line_int(line,"id:",-999);in_stage=(id==stage_id);phase=NULL;continue;
        }
        if(!strcmp(line,"<stage_end>")){if(in_stage)break;in_stage=0;phase=NULL;continue;}
        if(!in_stage)continue;
        if(!strncmp(line,"<phase>",7)){
            if(out->phase_count>=LF2_STAGE_MAX_PHASES)continue;
            phase=&out->phases[out->phase_count++];memset(phase,0,sizeof(*phase));phase->bound=line_int(line,"bound:",0);continue;
        }
        if(!strcmp(line,"<phase_end>")){phase=NULL;continue;}
        if(!phase)continue;
        if(strstr(line,"bound:")){phase->bound=line_int(line,"bound:",phase->bound);continue;}
        if(!strstr(line,"id:"))continue;
        if(phase->spawn_count>=LF2_STAGE_MAX_SPAWNS)continue;
        int id=line_int(line,"id:",-1);
        /* Keep both fighters and stock pickup objects. v0.65 discarded the
           100..199 range, which is why authored milk/beer never appeared. */
        if(lf2_stage_object_to_roster(id)<0 && id!=1000 && id!=3000 && !(id>=100&&id<=199))continue;
        lf2_stage_spawn_t *sp=&phase->spawns[phase->spawn_count++];
        memset(sp,0,sizeof(*sp));sp->object_id=id;sp->hp=line_int(line,"hp:",500);
        sp->times=line_int(line,"times:",1);if(sp->times<1)sp->times=1;
        sp->ratio=line_float(line,"ratio:",-1.0f);sp->boss=strstr(line,"<boss>")!=NULL;sp->soldier=strstr(line,"<soldier>")!=NULL;
        sp->x=line_int(line,"x:",100);
    }
    free(text);
    if(!in_stage && out->phase_count==0)return -3;
    lf2_logf("STAGE","parsed stage id=%d phases=%d",stage_id,out->phase_count);
    return out->phase_count>0?0:-4;
}

int lf2_stage_object_to_roster(int id){
    switch(id){
        case 1:return 0; case 2:return 1; case 4:return 2; case 5:return 3;
        case 6:return 4; case 7:return 5; case 8:return 6; case 9:return 7;
        case 10:return 8; case 11:return 9;
        case 30:return 10; case 31:return 11; case 32:return 12; case 33:return 13;
        case 34:return 14; case 35:return 15; case 36:return 16; case 37:return 17;
        case 39:return 18; case 38:return 19; case 50:return 20; case 51:return 21;
        case 52:return 22;
        default:return -1;
    }
}

static unsigned next_rand(unsigned *state){
    unsigned x=*state;if(!x)x=0x6d2b79f5u;x^=x<<13;x^=x>>17;x^=x<<5;*state=x;return x;
}

static int stage_spawn_copies(const lf2_stage_spawn_t *sp,int difficulty){
    int copies;
    if(sp->ratio>=0.0f){
        float scaled=sp->ratio*(difficulty==LF2_DIFF_CRAZY?2.0f:1.0f);
        copies=(int)scaled;
    }else copies=sp->times;
    if(difficulty==LF2_DIFF_CRAZY && sp->ratio<0.0f)copies*=2;
    return copies<0?0:copies;
}

int lf2_stage_build_phase(const lf2_stage_phase_t *phase,int difficulty,
                          int *roster_out,int *hp_out,int cap,unsigned *rng_state){
    if(!phase||!roster_out||!hp_out||cap<=0||!rng_state)return 0;
    int n=0;
    for(int i=0;i<phase->spawn_count&&n<cap;i++){
        const lf2_stage_spawn_t *sp=&phase->spawns[i];
        /* For a single local player the stock ratio table reduces to the integer
           part of ratio on E/N/D (e.g. .7 and .3 do not add enemies). Lines
           without ratio use their authored `times` count. CRAZY roughly doubles
           both the unconditional and ratio-derived population. */
        if(sp->object_id>=100&&sp->object_id<=199)continue;
        int copies=stage_spawn_copies(sp,difficulty);
        for(int k=0;k<copies&&n<cap;k++){
            int roster;
            if(sp->object_id==3000)roster=10+(int)(next_rand(rng_state)&1u); /* Bandit/Hunter */
            else if(sp->object_id==1000)roster=(int)(next_rand(rng_state)%10u); /* selectable heroes */
            else roster=lf2_stage_object_to_roster(sp->object_id);
            if(roster<0)continue;
            int hp=sp->hp>0?sp->hp:500;
            if(difficulty==LF2_DIFF_EASY)hp=(hp*3+3)/4;
            else if(difficulty==LF2_DIFF_CRAZY)hp=(hp*3+1)/2;
            if(hp<1)hp=1;
            roster_out[n]=roster;hp_out[n]=hp;n++;
        }
    }
    return n;
}

int lf2_stage_build_items(const lf2_stage_phase_t *phase,int difficulty,
                          int *oid_out,int *x_out,int cap,unsigned *rng_state){
    (void)rng_state;
    if(!phase||!oid_out||!x_out||cap<=0)return 0;
    int n=0;
    for(int i=0;i<phase->spawn_count&&n<cap;i++){
        const lf2_stage_spawn_t *sp=&phase->spawns[i];
        if(sp->object_id<100||sp->object_id>199)continue;
        int copies=stage_spawn_copies(sp,difficulty);
        for(int k=0;k<copies&&n<cap;k++){
            oid_out[n]=sp->object_id;
            x_out[n]=sp->x + k*34;
            n++;
        }
    }
    return n;
}
