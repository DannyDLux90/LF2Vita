/* -------------------------------------------------------------------------
 * Playable LF2Vita PC companion transport
 * ------------------------------------------------------------------------- */
#define LF2_PC_BRIDGE_PORT 24500
#define LF2_PC_BRIDGE_MAGIC0 'L'
#define LF2_PC_BRIDGE_MAGIC1 '2'
#define LF2_PC_BRIDGE_MAGIC2 'P'
#define LF2_PC_BRIDGE_MAGIC3 'C'
#define LF2_PC_BRIDGE_VERSION 1
#define LF2_PC_FRAME_W 480
#define LF2_PC_FRAME_H 272
#define LF2_PC_FRAME_RAW (LF2_PC_FRAME_W*LF2_PC_FRAME_H*3)
#define LF2_PC_FRAME_INTERVAL_US 100000ULL /* 10 fps stream, gameplay stays 30 Hz */
#define LF2_PC_BRIDGE_TIMEOUT_US 5000000ULL

enum {
    LF2PCB_HELLO=1,
    LF2PCB_HELLO_ACK=2,
    LF2PCB_INPUT=3,
    LF2PCB_FRAME=4,
    LF2PCB_MATCH_INFO=5,
    LF2PCB_STATUS=6,
    LF2PCB_PING=7,
    LF2PCB_PONG=8
};

static volatile int g_bridge_sock=-1;
static volatile int g_bridge_cancel;
static SceUID g_bridge_thread=-1;
static char g_bridge_target[16];
static lf2_pc_bridge_state_t g_bridge;
static unsigned char g_bridge_frame_raw[LF2_PC_FRAME_RAW];
static unsigned char g_bridge_frame_z[LF2_PC_FRAME_RAW+4096];
static volatile int g_bridge_frame_ready;
static volatile uint32_t g_bridge_frame_seq;
static uint64_t g_bridge_last_capture;
static volatile int g_bridge_info_dirty;
static char g_bridge_match_info[192];
static uint64_t g_bridge_last_input_us;

static void bridge_put16(unsigned char *p,unsigned v){p[0]=(unsigned char)(v>>8);p[1]=(unsigned char)v;}
static void bridge_put32(unsigned char *p,uint32_t v){p[0]=(unsigned char)(v>>24);p[1]=(unsigned char)(v>>16);p[2]=(unsigned char)(v>>8);p[3]=(unsigned char)v;}
static unsigned bridge_get16(const unsigned char *p){return ((unsigned)p[0]<<8)|p[1];}
static uint32_t bridge_get32(const unsigned char *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}

static void bridge_make_header(unsigned char h[16],unsigned type,uint32_t size,uint32_t seq){
    h[0]=LF2_PC_BRIDGE_MAGIC0;h[1]=LF2_PC_BRIDGE_MAGIC1;h[2]=LF2_PC_BRIDGE_MAGIC2;h[3]=LF2_PC_BRIDGE_MAGIC3;
    bridge_put16(h+4,LF2_PC_BRIDGE_VERSION);bridge_put16(h+6,type);bridge_put32(h+8,size);bridge_put32(h+12,seq);
}

static int bridge_send_exact(int s,const void *buf,int len,uint64_t timeout_us){
    const unsigned char *p=(const unsigned char*)buf;int sent=0;uint64_t start=pc_now();
    while(sent<len&&!g_bridge_cancel){
        int rc=sceNetSend(s,p+sent,(unsigned)(len-sent),SCE_NET_MSG_DONTWAIT);
        if(rc>0){sent+=rc;g_bridge.bytes_tx+=(uint32_t)rc;continue;}
        if(pc_now()-start>=timeout_us)return -3;
        sceKernelDelayThread(1000);
    }
    return g_bridge_cancel?-4:sent;
}

static int bridge_recv_exact(int s,void *buf,int len,uint64_t timeout_us){
    unsigned char *p=(unsigned char*)buf;int got=0;uint64_t start=pc_now();
    while(got<len&&!g_bridge_cancel){
        int rc=sceNetRecv(s,p+got,(unsigned)(len-got),SCE_NET_MSG_DONTWAIT);
        if(rc>0){got+=rc;g_bridge.bytes_rx+=(uint32_t)rc;continue;}
        if(rc==0)return -2;
        if(pc_now()-start>=timeout_us)return -3;
        sceKernelDelayThread(1000);
    }
    return g_bridge_cancel?-4:got;
}

static int bridge_send_msg(int s,unsigned type,uint32_t seq,const void *payload,uint32_t size){
    unsigned char h[16];bridge_make_header(h,type,size,seq);
    if(bridge_send_exact(s,h,16,LF2_PC_BRIDGE_TIMEOUT_US)!=16)return -1;
    if(size&&bridge_send_exact(s,payload,(int)size,LF2_PC_BRIDGE_TIMEOUT_US)!=(int)size)return -2;
    return 0;
}

static void bridge_set_error(const char *where,int rc){
    g_bridge.last_error=rc;g_bridge.connected=0;g_bridge.phase=LF2_PC_BRIDGE_ERROR;
    snprintf(g_bridge.message,sizeof(g_bridge.message),"%s: 0x%08X",where,(unsigned)rc);
    lf2_logf("NETPC","bridge %s",g_bridge.message);
}

static int bridge_send_frame(int s){
    uLongf zn=sizeof(g_bridge_frame_z);
    int zr=compress2(g_bridge_frame_z,&zn,g_bridge_frame_raw,LF2_PC_FRAME_RAW,1);
    if(zr!=Z_OK)return -20;
    unsigned char h[16],meta[8];
    bridge_put16(meta,LF2_PC_FRAME_W);bridge_put16(meta+2,LF2_PC_FRAME_H);bridge_put32(meta+4,LF2_PC_FRAME_RAW);
    uint32_t seq=g_bridge_frame_seq;bridge_make_header(h,LF2PCB_FRAME,(uint32_t)zn+8,seq);
    if(bridge_send_exact(s,h,16,LF2_PC_BRIDGE_TIMEOUT_US)!=16)return -21;
    if(bridge_send_exact(s,meta,8,LF2_PC_BRIDGE_TIMEOUT_US)!=8)return -22;
    if(bridge_send_exact(s,g_bridge_frame_z,(int)zn,LF2_PC_BRIDGE_TIMEOUT_US)!=(int)zn)return -23;
    g_bridge.frames_sent++;
    return 0;
}

static int pc_bridge_thread(SceSize args,void *argp){
    (void)args;(void)argp;int rc;
    g_bridge.phase=LF2_PC_BRIDGE_CONNECTING;
    snprintf(g_bridge.message,sizeof(g_bridge.message),"PC-Client %s:%d wird verbunden...",g_bridge_target,LF2_PC_BRIDGE_PORT);
    int s=sceNetSocket("lf2-pc-bridge",SCE_NET_AF_INET,SCE_NET_SOCK_STREAM,SCE_NET_IPPROTO_TCP);
    if(s<0){bridge_set_error("sceNetSocket",s);goto done;}g_bridge_sock=s;
    SceNetSockaddrIn sa;memset(&sa,0,sizeof(sa));sa.sin_len=sizeof(sa);sa.sin_family=SCE_NET_AF_INET;sa.sin_port=sceNetHtons(LF2_PC_BRIDGE_PORT);
    rc=sceNetInetPton(SCE_NET_AF_INET,g_bridge_target,&sa.sin_addr);if(rc<=0){bridge_set_error("ungueltige IPv4",rc);goto done;}
    lf2_logf("NETPC","bridge connect target=%s:%d",g_bridge_target,LF2_PC_BRIDGE_PORT);
    rc=sceNetConnect(s,(SceNetSockaddr*)&sa,sizeof(sa));if(rc<0){bridge_set_error("sceNetConnect",rc);goto done;}
    int one=1;sceNetSetsockopt(s,SCE_NET_IPPROTO_TCP,SCE_NET_TCP_NODELAY,&one,sizeof(one));
    int nb=1;sceNetSetsockopt(s,SCE_NET_SOL_SOCKET,SCE_NET_SO_NBIO,&nb,sizeof(nb));

    g_bridge.phase=LF2_PC_BRIDGE_HANDSHAKE;snprintf(g_bridge.message,sizeof(g_bridge.message),"LF2Vita PC-Client wird erkannt...");
    char hello[96];snprintf(hello,sizeof(hello),"LF2Vita 0.70|screen=%dx%d|input=30|video=10",LF2_PC_FRAME_W,LF2_PC_FRAME_H);
    if(bridge_send_msg(s,LF2PCB_HELLO,0,hello,(uint32_t)strlen(hello)+1)<0){bridge_set_error("HELLO send",-30);goto done;}
    unsigned char h[16];rc=bridge_recv_exact(s,h,16,LF2_PC_BRIDGE_TIMEOUT_US);
    if(rc!=16||h[0]!='L'||h[1]!='2'||h[2]!='P'||h[3]!='C'||bridge_get16(h+4)!=LF2_PC_BRIDGE_VERSION||bridge_get16(h+6)!=LF2PCB_HELLO_ACK){bridge_set_error("PC HELLO",rc);goto done;}
    uint32_t hs=bridge_get32(h+8);if(hs>512){bridge_set_error("PC HELLO size",(int)hs);goto done;}
    if(hs){unsigned char junk[512];rc=bridge_recv_exact(s,junk,(int)hs,LF2_PC_BRIDGE_TIMEOUT_US);if(rc!=(int)hs){bridge_set_error("PC HELLO payload",rc);goto done;}}
    g_bridge.connected=1;g_bridge.phase=LF2_PC_BRIDGE_CONNECTED;g_bridge.last_error=0;g_bridge_last_input_us=pc_now();
    snprintf(g_bridge.message,sizeof(g_bridge.message),"PC-Client verbunden");
    lf2_log_stage("network:pc_bridge_connected","ip=%s port=%d",g_bridge_target,LF2_PC_BRIDGE_PORT);

    unsigned char rx[4096];int rxn=0;
    while(!g_bridge_cancel){
        int rr=sceNetRecv(s,rx+rxn,(unsigned)(sizeof(rx)-rxn),SCE_NET_MSG_DONTWAIT);
        if(rr>0){rxn+=rr;g_bridge.bytes_rx+=(uint32_t)rr;}
        else if(rr==0){bridge_set_error("PC getrennt",-40);break;}
        int off=0;
        while(rxn-off>=16){
            unsigned char *ph=rx+off;
            if(ph[0]!='L'||ph[1]!='2'||ph[2]!='P'||ph[3]!='C'||bridge_get16(ph+4)!=LF2_PC_BRIDGE_VERSION){bridge_set_error("ungueltiger PC Datenstrom",-41);g_bridge_cancel=1;break;}
            unsigned type=bridge_get16(ph+6);uint32_t sz=bridge_get32(ph+8);uint32_t seq=bridge_get32(ph+12);
            if(sz>2048){bridge_set_error("PC Paket zu gross",(int)sz);g_bridge_cancel=1;break;}
            if(rxn-off<16+(int)sz)break;
            const unsigned char *pl=ph+16;
            if(type==LF2PCB_INPUT&&sz>=4){g_bridge.input_mask=bridge_get32(pl);g_bridge.input_packets++;g_bridge_last_input_us=pc_now();}
            else if(type==LF2PCB_PING){bridge_send_msg(s,LF2PCB_PONG,seq,pl,sz);}
            off+=16+(int)sz;
        }
        if(off){memmove(rx,rx+off,(size_t)(rxn-off));rxn-=off;}
        if(g_bridge_info_dirty){
            char info[192];snprintf(info,sizeof(info),"%s",g_bridge_match_info);g_bridge_info_dirty=0;
            if(bridge_send_msg(s,LF2PCB_MATCH_INFO,0,info,(uint32_t)strlen(info)+1)<0){bridge_set_error("Match-Info send",-42);break;}
        }
        if(g_bridge_frame_ready){
            int fr=bridge_send_frame(s);g_bridge_frame_ready=0;
            if(fr<0){bridge_set_error("Video send",fr);break;}
        }
        sceKernelDelayThread(2000);
    }

done:
    if(s>=0){sceNetShutdown(s,SCE_NET_SHUT_RDWR);sceNetSocketClose(s);}g_bridge_sock=-1;g_bridge.connected=0;g_bridge.running=0;g_bridge.input_mask=0;g_bridge_frame_ready=0;
    if(g_bridge.phase!=LF2_PC_BRIDGE_ERROR){g_bridge.phase=LF2_PC_BRIDGE_CLOSED;snprintf(g_bridge.message,sizeof(g_bridge.message),"PC-Verbindung beendet");}
    g_bridge_thread=-1;sceKernelExitDeleteThread(0);return 0;
}

int lf2_pc_bridge_connect(const char *ipv4){
    if(!ipv4||!ipv4[0])return -1;if(g_bridge.running)return -2;if(g_state.running)return -3;
    int rc=lf2_pc_init();if(rc<0)return rc;
    SceNetInAddr tmp;if(sceNetInetPton(SCE_NET_AF_INET,ipv4,&tmp)<=0)return -4;
    int state=lf2_pc_inet_state();if(state!=SCE_NETCTL_STATE_CONNECTED){memset(&g_bridge,0,sizeof(g_bridge));g_bridge.phase=LF2_PC_BRIDGE_ERROR;g_bridge.inet_state=state;g_bridge.last_error=(int)0x80412108u;snprintf(g_bridge.message,sizeof(g_bridge.message),"Vita ist nicht mit WLAN/Internet verbunden (state=%d)",state);return g_bridge.last_error;}
    memset(&g_bridge,0,sizeof(g_bridge));g_bridge.running=1;g_bridge.phase=LF2_PC_BRIDGE_CONNECTING;g_bridge.inet_state=state;snprintf(g_bridge.target_ip,sizeof(g_bridge.target_ip),"%s",ipv4);snprintf(g_bridge_target,sizeof(g_bridge_target),"%s",ipv4);
    g_bridge_cancel=0;g_bridge_frame_ready=0;g_bridge_info_dirty=0;g_bridge_last_capture=0;g_bridge_frame_seq=0;
    g_bridge_thread=sceKernelCreateThread("lf2-pc-bridge",pc_bridge_thread,LF2_PC_THREAD_PRIORITY,LF2_PC_THREAD_STACK,0,SCE_KERNEL_CPU_MASK_USER_0,NULL);
    if(g_bridge_thread<0){g_bridge.running=0;bridge_set_error("CreateThread",g_bridge_thread);return g_bridge_thread;}
    rc=sceKernelStartThread(g_bridge_thread,0,NULL);if(rc<0){sceKernelDeleteThread(g_bridge_thread);g_bridge_thread=-1;g_bridge.running=0;bridge_set_error("StartThread",rc);return rc;}
    return 0;
}

void lf2_pc_bridge_disconnect(void){
    if(!g_bridge.running&&!g_bridge.connected)return;g_bridge_cancel=1;int s=g_bridge_sock;if(s>=0)sceNetSocketAbort(s,0);
    SceUID th=g_bridge_thread;if(th>=0){sceKernelWaitThreadEnd(th,NULL,NULL);g_bridge_thread=-1;}
    g_bridge.input_mask=0;g_bridge.connected=0;g_bridge.running=0;g_bridge_frame_ready=0;
}

const lf2_pc_bridge_state_t *lf2_pc_bridge_state(void){
    if(g_netctl_ready){int st=-1;if(sceNetCtlInetGetState(&st)>=0)g_bridge.inet_state=st;}
    if(g_bridge_last_input_us&&g_bridge.connected)g_bridge.last_input_age_ms=(uint32_t)((pc_now()-g_bridge_last_input_us)/1000ULL);else g_bridge.last_input_age_ms=0;
    return &g_bridge;
}
uint32_t lf2_pc_bridge_input_mask(void){return g_bridge.connected?g_bridge.input_mask:0;}
int lf2_pc_bridge_alive(void){return g_bridge.connected?1:0;}

void lf2_pc_bridge_submit_frame(void){
    if(!g_bridge.connected)return;uint64_t now=pc_now();if(now-g_bridge_last_capture<LF2_PC_FRAME_INTERVAL_US)return;
    if(g_bridge_frame_ready){g_bridge.frames_dropped++;return;}
    const uint32_t *fb=(const uint32_t*)vita2d_get_current_fb();if(!fb)return;
    unsigned char *d=g_bridge_frame_raw;
    for(int y=0;y<LF2_PC_FRAME_H;y++){
        const uint32_t *row=fb+(y*2)*960;
        for(int x=0;x<LF2_PC_FRAME_W;x++){
            uint32_t px=row[x*2];*d++=(unsigned char)(px&0xff);*d++=(unsigned char)((px>>8)&0xff);*d++=(unsigned char)((px>>16)&0xff);
        }
    }
    g_bridge_last_capture=now;g_bridge_frame_seq++;g_bridge_frame_ready=1;
}

void lf2_pc_bridge_match_info(const char *p1,const char *p2,const char *stage,int difficulty,int running){
    snprintf(g_bridge_match_info,sizeof(g_bridge_match_info),"state=%s|p1=%s|p2=%s|stage=%s|difficulty=%d",running?"playing":"lobby",p1?p1:"?",p2?p2:"?",stage?stage:"?",difficulty);
    g_bridge_info_dirty=1;
}
