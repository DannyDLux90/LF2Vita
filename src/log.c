#include "log.h"
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LOG_DIR "ux0:data/LF2V00001"
#define LOG_FILE LOG_DIR "/lf2.log"
#define LOG_PREV_FILE LOG_DIR "/lf2_prev.log"
#define STATE_FILE LOG_DIR "/last_state.txt"
static SceUID g_log=-1;
static char g_last_stage[128]="boot";

static void write_all(SceUID fd,const char *s,int n){
    while(n>0){int w=sceIoWrite(fd,s,n);if(w<=0)break;s+=w;n-=w;}
}
static void sync_log(void){if(g_log>=0)sceIoSyncByFd(g_log,0);}
static unsigned long long ms_now(void){return (unsigned long long)(sceKernelGetProcessTimeWide()/1000ULL);}

const char *lf2_log_path(void){return LOG_FILE;}

void lf2_logf(const char *level,const char *fmt,...){
    if(g_log<0)return;
    char body[1400],line[1536];va_list ap;va_start(ap,fmt);vsnprintf(body,sizeof(body),fmt,ap);va_end(ap);
    int n=snprintf(line,sizeof(line),"[%010llu ms] %-5s %s\n",ms_now(),level?level:"INFO",body);
    if(n>0)write_all(g_log,line,n<(int)sizeof(line)?n:(int)sizeof(line)-1);
}

void lf2_log_memory(const char *tag){
    SceKernelFreeMemorySizeInfo m={0};m.size=sizeof(m);int rc=sceKernelGetFreeMemorySize(&m);
    if(rc>=0)lf2_logf("MEM","%s user=%d cdram=%d phycont=%d",tag?tag:"",m.size_user,m.size_cdram,m.size_phycont);
    else lf2_logf("WARN","sceKernelGetFreeMemorySize(%s) rc=0x%08X",tag?tag:"",rc);
}

void lf2_log_stage(const char *stage,const char *fmt,...){
    if(stage&&*stage)snprintf(g_last_stage,sizeof(g_last_stage),"%s",stage);
    char detail[768]="";if(fmt&&*fmt){va_list ap;va_start(ap,fmt);vsnprintf(detail,sizeof(detail),fmt,ap);va_end(ap);}
    lf2_logf("STAGE","%s%s%s",g_last_stage,detail[0]?" | ":"",detail);
    SceUID fd=sceIoOpen(STATE_FILE,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
    if(fd>=0){char s[1024];int n=snprintf(s,sizeof(s),"unclean=1\nstage=%s\ndetail=%s\ntime_ms=%llu\nlog=%s\n",g_last_stage,detail,ms_now(),LOG_FILE);write_all(fd,s,n);sceIoSyncByFd(fd,0);sceIoClose(fd);}
    sync_log();
}

static void crash_handler(int sig){
    if(g_log>=0){char line[384];int n=snprintf(line,sizeof(line),"[%010llu ms] CRASH signal=%d last_stage=%s\n",ms_now(),sig,g_last_stage);write_all(g_log,line,n);sync_log();}
    SceUID fd=sceIoOpen(STATE_FILE,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
    if(fd>=0){char s[512];int n=snprintf(s,sizeof(s),"unclean=1\nsignal=%d\nstage=%s\ntime_ms=%llu\nlog=%s\n",sig,g_last_stage,ms_now(),LOG_FILE);write_all(fd,s,n);sceIoSyncByFd(fd,0);sceIoClose(fd);}
    signal(sig,SIG_DFL);raise(sig);
}
void lf2_log_install_signal_handlers(void){
    signal(SIGABRT,crash_handler);signal(SIGSEGV,crash_handler);signal(SIGILL,crash_handler);signal(SIGFPE,crash_handler);
#ifdef SIGBUS
    signal(SIGBUS,crash_handler);
#endif
}

void lf2_log_init(void){
    sceIoMkdir("ux0:data",0777);sceIoMkdir(LOG_DIR,0777);
    SceUID old=sceIoOpen(STATE_FILE,SCE_O_RDONLY,0);
    char prev[512]={0};if(old>=0){int n=sceIoRead(old,prev,sizeof(prev)-1);if(n>0)prev[n]=0;sceIoClose(old);}
    /* Keep the current diagnostic log unambiguous. Older builds appended all
       sessions forever, which made it easy to send a stale 0.40/0.50 session
       while testing a newer VPK. Preserve exactly one previous session. */
    sceIoRemove(LOG_PREV_FILE);
    sceIoRename(LOG_FILE,LOG_PREV_FILE);
    g_log=sceIoOpen(LOG_FILE,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
    lf2_logf("INFO","============================================================");
    lf2_logf("INFO","Little Fighter 2 Vita 0.69 fix3 session start");
    lf2_logf("INFO","build=0.69-fix3 log_policy=current_session prev=%s",LOG_PREV_FILE);
    if(prev[0]&&strstr(prev,"unclean=1"))lf2_logf("WARN","Previous session did not finish cleanly: %s",prev);
    lf2_log_memory("startup");sync_log();
}

void lf2_log_shutdown(int clean){
    if(g_log<0)return;
    lf2_log_memory("shutdown");lf2_logf("INFO","session end clean=%d",clean);sync_log();
    SceUID fd=sceIoOpen(STATE_FILE,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
    if(fd>=0){char s[256];int n=snprintf(s,sizeof(s),"unclean=%d\nstage=%s\ntime_ms=%llu\nlog=%s\n",clean?0:1,clean?"clean_shutdown":g_last_stage,ms_now(),LOG_FILE);write_all(fd,s,n);sceIoSyncByFd(fd,0);sceIoClose(fd);}
    sceIoClose(g_log);g_log=-1;
}
