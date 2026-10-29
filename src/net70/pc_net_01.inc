#include "pc_net.h"

#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/cpu.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <vita2d.h>

#include "log.h"
#include "game.h"
#include "lf2_rng.h"

#define LF2_PC_NET_POOL (1024*1024)
#define LF2_PC_PORT 12345
#define LF2_PC_IO_TIMEOUT_US 6000000ULL
#define LF2_PC_THREAD_PRIORITY 0x10000100
#define LF2_PC_THREAD_STACK 0x10000

static void *g_net_mem;
static int g_net_ready;
static int g_netctl_ready;
static int g_net_module;
static volatile int g_sock=-1;
static volatile int g_cancel;
static SceUID g_thread=-1;
static char g_error[160];
static char g_target_ip[16];
static lf2_pc_probe_state_t g_state;

static uint64_t pc_now(void){return sceKernelGetSystemTimeWide();}

static void pc_set_error(const char *where,int rc){
    snprintf(g_error,sizeof(g_error),"%s: 0x%08X",where,(unsigned)rc);
    g_state.last_error=rc;
    snprintf(g_state.message,sizeof(g_state.message),"%.127s",g_error);
    g_state.phase=LF2_PC_PROBE_ERROR;
    lf2_logf("NETPC","%s",g_error);
}

static void pc_cleanup_net(void){
    if(g_netctl_ready){sceNetCtlTerm();g_netctl_ready=0;}
    if(g_net_ready){sceNetTerm();g_net_ready=0;}
    free(g_net_mem);g_net_mem=NULL;
    if(g_net_module){sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);g_net_module=0;}
}

int lf2_pc_init(void){
    if(g_net_ready&&g_netctl_ready)return 0;
    g_error[0]=0;
    int rc=sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    if(rc<0){pc_set_error("load NET",rc);return rc;}g_net_module=1;
    SceNetInitParam np;memset(&np,0,sizeof(np));np.size=LF2_PC_NET_POOL;
    g_net_mem=memalign(64,np.size);if(!g_net_mem){pc_set_error("net memory",-1);pc_cleanup_net();return -1;}
    np.memory=g_net_mem;
    rc=sceNetInit(&np);if(rc<0){pc_set_error("sceNetInit",rc);pc_cleanup_net();return rc;}g_net_ready=1;
    rc=sceNetCtlInit();if(rc<0){pc_set_error("sceNetCtlInit",rc);pc_cleanup_net();return rc;}g_netctl_ready=1;
    int state=-1,sr=sceNetCtlInetGetState(&state);g_state.inet_state=sr>=0?state:-1;
    lf2_logf("NETPC","PC network init inet_state=%d state_rc=0x%08X",state,(unsigned)sr);
    return 0;
}

int lf2_pc_inet_state(void){
    if(!g_netctl_ready)return -1;
    int state=-1;if(sceNetCtlInetGetState(&state)<0)return -1;g_state.inet_state=state;return state;
}

const char *lf2_pc_error(void){return g_error;}

static int pc_recv_exact(int s,void *buf,int len,uint64_t timeout_us){
    unsigned char *p=(unsigned char*)buf;int got=0;uint64_t start=pc_now();
    while(got<len&&!g_cancel){
        int rc=sceNetRecv(s,p+got,(unsigned)(len-got),SCE_NET_MSG_DONTWAIT);
        if(rc>0){got+=rc;g_state.bytes_rx+=(uint32_t)rc;continue;}
        if(rc==0)return -2;
        if(pc_now()-start>=timeout_us)return -3;
        sceKernelDelayThread(10000);
    }
    return g_cancel?-4:got;
}

static int pc_send_exact(int s,const void *buf,int len,uint64_t timeout_us){
    const unsigned char *p=(const unsigned char*)buf;int sent=0;uint64_t start=pc_now();
    while(sent<len&&!g_cancel){
        int rc=sceNetSend(s,p+sent,(unsigned)(len-sent),SCE_NET_MSG_DONTWAIT);
        if(rc>0){sent+=rc;g_state.bytes_tx+=(uint32_t)rc;continue;}
        if(pc_now()-start>=timeout_us)return -3;
        sceKernelDelayThread(10000);
    }
    return g_cancel?-4:sent;
}

static void pc_name10(unsigned char *dst,const char *prefix,int n){
    char tmp[11];memset(tmp,'-',10);tmp[10]=0;
    int plen=(int)strlen(prefix);if(plen>8)plen=8;memcpy(tmp,prefix,(size_t)plen);
    tmp[9]=(char)('0'+n);
    memcpy(dst,tmp,10);
}

static void pc_build_client_identity(unsigned char out[77]){
    memset(out,0,77);
    memcpy(out,"00001111",8);             /* stock LF2 client owns control slots 5-8 */
    memset(out+8,'0',24);                  /* stock protocol sends ASCII zeroes here */
    for(int i=0;i<4;i++){
        pc_name10(out+32+i*11,"Vita",i+1);
        out[32+i*11+10]='_';
    }
    out[76]=0;
}

static void pc_parse_host_identity(const unsigned char in[77]){
    memcpy(g_state.host_slots,in,8);g_state.host_slots[8]=0;
    for(int i=0;i<4;i++){
        memcpy(g_state.host_names[i],in+32+i*11,10);g_state.host_names[i][10]=0;
        for(int j=9;j>=0&&g_state.host_names[i][j]==' ';j--)g_state.host_names[i][j]=0;
    }
}

static uint32_t pc_fnv1a(const void *data,size_t n){
    const unsigned char *p=(const unsigned char*)data;uint32_t h=2166136261u;
    for(size_t i=0;i<n;i++){h^=p[i];h*=16777619u;}return h;
}

static void pc_finish_thread(void){
    int s=g_sock;if(s>=0){sceNetShutdown(s,SCE_NET_SHUT_RDWR);sceNetSocketClose(s);g_sock=-1;}
    g_state.running=0;
    g_thread=-1;
    sceKernelExitDeleteThread(0);
}

static int pc_probe_thread(SceSize args,void *argp){
    (void)args;(void)argp;uint64_t start=pc_now();int rc;
    g_state.phase=LF2_PC_PROBE_CONNECTING;snprintf(g_state.message,sizeof(g_state.message),"TCP %s:%d wird verbunden...",g_target_ip,LF2_PC_PORT);
    int s=sceNetSocket("lf2-pc-probe",SCE_NET_AF_INET,SCE_NET_SOCK_STREAM,SCE_NET_IPPROTO_TCP);
    if(s<0){pc_set_error("sceNetSocket",s);pc_finish_thread();return 0;}g_sock=s;

    SceNetSockaddrIn sa;memset(&sa,0,sizeof(sa));sa.sin_len=sizeof(sa);sa.sin_family=SCE_NET_AF_INET;sa.sin_port=sceNetHtons(LF2_PC_PORT);
    rc=sceNetInetPton(SCE_NET_AF_INET,g_target_ip,&sa.sin_addr);if(rc<=0){pc_set_error("ungueltige IPv4-Adresse",rc);pc_finish_thread();return 0;}
    lf2_logf("NETPC","connect target=%s:%d",g_target_ip,LF2_PC_PORT);
    rc=sceNetConnect(s,(SceNetSockaddr*)&sa,sizeof(sa));
    if(g_cancel){g_state.phase=LF2_PC_PROBE_CANCELLED;snprintf(g_state.message,sizeof(g_state.message),"Test abgebrochen");pc_finish_thread();return 0;}
    if(rc<0){pc_set_error("sceNetConnect",rc);pc_finish_thread();return 0;}

    int nb=1;sceNetSetsockopt(s,SCE_NET_SOL_SOCKET,SCE_NET_SO_NBIO,&nb,sizeof(nb));
    g_state.phase=LF2_PC_PROBE_BANNER;snprintf(g_state.message,sizeof(g_state.message),"LF2-Begruessung wird geprueft...");
    unsigned char banner[14];rc=pc_recv_exact(s,banner,14,LF2_PC_IO_TIMEOUT_US);
    if(rc!=14){pc_set_error("LF2 banner recv",rc);pc_finish_thread();return 0;}
    static const unsigned char expected[14]={'u',' ','c','a','n',' ','c','o','n','n','e','c','t',0};
    if(memcmp(banner,expected,14)!=0){pc_set_error("kein LF2 2.00a Host",-20);lf2_logf("NETPC","unexpected 14-byte banner");pc_finish_thread();return 0;}
    lf2_logf("NETPC","stock banner ok: u can connect");

    g_state.phase=LF2_PC_PROBE_IDENTITY;snprintf(g_state.message,sizeof(g_state.message),"Spielerblock wird ausgetauscht...");
    unsigned char client[77],host[77];pc_build_client_identity(client);
    rc=pc_send_exact(s,client,77,LF2_PC_IO_TIMEOUT_US);if(rc!=77){pc_set_error("client identity send",rc);pc_finish_thread();return 0;}
    rc=pc_recv_exact(s,host,77,LF2_PC_IO_TIMEOUT_US);if(rc!=77){pc_set_error("host identity recv",rc);pc_finish_thread();return 0;}
    pc_parse_host_identity(host);
    lf2_logf("NETPC","host identity slots=%s names=[%s][%s][%s][%s] tail=%02X",g_state.host_slots,g_state.host_names[0],g_state.host_names[1],g_state.host_names[2],g_state.host_names[3],host[76]);

    g_state.phase=LF2_PC_PROBE_SEED;snprintf(g_state.message,sizeof(g_state.message),"Originalen RNG-Seed lesen (3001 Byte)...");
    unsigned char *seed=(unsigned char*)malloc(3001);if(!seed){pc_set_error("seed memory",-21);pc_finish_thread();return 0;}
    rc=pc_recv_exact(s,seed,3001,LF2_PC_IO_TIMEOUT_US);if(rc!=3001){free(seed);pc_set_error("seed recv",rc);pc_finish_thread();return 0;}
    g_state.seed_hash=pc_fnv1a(seed,3001);free(seed);

    g_state.elapsed_ms=(uint32_t)((pc_now()-start)/1000ULL);
    g_state.phase=LF2_PC_PROBE_COMPATIBLE;g_state.last_error=0;
    snprintf(g_state.message,sizeof(g_state.message),"LF2 2.00a Protokoll erkannt - Handshake komplett");
    lf2_log_stage("network:pc_probe_ok","ip=%s slots=%s seed_fnv=%08X rx=%u tx=%u ms=%u",g_target_ip,g_state.host_slots,g_state.seed_hash,g_state.bytes_rx,g_state.bytes_tx,g_state.elapsed_ms);
    pc_finish_thread();return 0;
}

int lf2_pc_probe_start(const char *ipv4){
    if(!ipv4||!ipv4[0])return -1;
    if(g_state.running)return -2;
    int rc=lf2_pc_init();if(rc<0)return rc;
    SceNetInAddr tmp;if(sceNetInetPton(SCE_NET_AF_INET,ipv4,&tmp)<=0){snprintf(g_error,sizeof(g_error),"Ungueltige IPv4-Adresse");return -3;}
    int state=lf2_pc_inet_state();if(state!=SCE_NETCTL_STATE_CONNECTED){snprintf(g_error,sizeof(g_error),"Vita ist nicht mit WLAN/Internet verbunden (state=%d)",state);g_state.last_error=(int)0x80412108u;g_state.phase=LF2_PC_PROBE_ERROR;snprintf(g_state.message,sizeof(g_state.message),"%.127s",g_error);return g_state.last_error;}
    memset(&g_state,0,sizeof(g_state));g_state.running=1;g_state.phase=LF2_PC_PROBE_NET_INIT;g_state.inet_state=state;snprintf(g_state.target_ip,sizeof(g_state.target_ip),"%s",ipv4);snprintf(g_target_ip,sizeof(g_target_ip),"%s",ipv4);g_cancel=0;
    g_thread=sceKernelCreateThread("lf2-pc-probe",pc_probe_thread,LF2_PC_THREAD_PRIORITY,LF2_PC_THREAD_STACK,0,SCE_KERNEL_CPU_MASK_USER_0,NULL);
    if(g_thread<0){g_state.running=0;pc_set_error("CreateThread",g_thread);return g_thread;}
    rc=sceKernelStartThread(g_thread,0,NULL);if(rc<0){sceKernelDeleteThread(g_thread);g_thread=-1;g_state.running=0;pc_set_error("StartThread",rc);return rc;}
    return 0;
}

void lf2_pc_probe_cancel(void){
    if(!g_state.running)return;g_cancel=1;int s=g_sock;if(s>=0)sceNetSocketAbort(s,0);
    lf2_logf("NETPC","probe cancel requested");
}

const lf2_pc_probe_state_t *lf2_pc_probe_state(void){
    if(g_netctl_ready){int state=-1;if(sceNetCtlInetGetState(&state)>=0)g_state.inet_state=state;}
    return &g_state;
}

const char *lf2_pc_probe_phase_name(int p){
    switch(p){
        case LF2_PC_PROBE_IDLE:return "Bereit";
        case LF2_PC_PROBE_NET_INIT:return "Netzwerk";
        case LF2_PC_PROBE_CONNECTING:return "Verbinden";
        case LF2_PC_PROBE_BANNER:return "LF2 erkennen";
        case LF2_PC_PROBE_IDENTITY:return "Spielerblock";
        case LF2_PC_PROBE_SEED:return "RNG-Seed";
        case LF2_PC_PROBE_COMPATIBLE:return "Kompatibel";
        case LF2_PC_PROBE_ERROR:return "Fehler";
        case LF2_PC_PROBE_CANCELLED:return "Abgebrochen";
        default:return "Unbekannt";
    }
}

