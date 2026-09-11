/* -------------------------------------------------------------------------
 * Original LF2 2.00a TCP/12345 transport
 *
 * The stock Windows executable uses a strict 22-byte lockstep after a
 * 14/77/3001-byte handshake.  We preserve bytes that are not yet fully
 * reverse-engineered and only replace the four key bytes owned by this peer.
 * This is intentional compatibility scaffolding: it lets the Vita participate
 * in the original input clock without inventing values for LF2's private
 * function/checksum fields while the deterministic runtime is being matched.
 * ------------------------------------------------------------------------- */
#define LF2_PC_STOCK_PACKET 22
#define LF2_PC_STOCK_SEED 3001
#define LF2_PC_STOCK_TIMEOUT_US 6000000ULL

static volatile int g_stock_sock=-1;
static volatile int g_stock_listen_sock=-1;
static volatile int g_stock_cancel;
static SceUID g_stock_thread=-1;
static char g_stock_target[16];
static lf2_pc_stock_state_t g_stock;
static uint8_t g_stock_seed[LF2_PC_STOCK_SEED];
static uint32_t g_stock_clock_local[4];
static uint32_t g_stock_clock_remote[4];
static int g_stock_clock_phase;
static const uint8_t g_stock_initial_packet[LF2_PC_STOCK_PACKET]={
    0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x1e,
    0x01,0x01,0x69,0x62,0x01,0x01,0x01,0x01,0x00,0x00
};

static void stock_set_message(const char *msg){
    snprintf(g_stock.message,sizeof(g_stock.message),"%s",msg?msg:"");
}

static void stock_set_error(const char *where,int rc){
    g_stock.last_error=rc;g_stock.connected=0;g_stock.phase=LF2_PC_STOCK_ERROR;
    snprintf(g_stock.message,sizeof(g_stock.message),"%s: 0x%08X",where?where:"stock",(unsigned)rc);
    lf2_logf("NETPC","stock %s",g_stock.message);
}

static int stock_recv_exact(int s,void *buf,int len,uint64_t timeout_us){
    uint8_t *p=(uint8_t*)buf;int got=0;uint64_t start=pc_now();
    while(got<len&&!g_stock_cancel){
        int rc=sceNetRecv(s,p+got,(unsigned)(len-got),SCE_NET_MSG_DONTWAIT);
        if(rc>0){got+=rc;g_stock.bytes_rx+=(uint32_t)rc;continue;}
        if(rc==0)return -2;
        if(pc_now()-start>=timeout_us){g_stock.stalls++;return -3;}
        sceKernelDelayThread(1000);
    }
    return g_stock_cancel?-4:got;
}

static int stock_send_exact(int s,const void *buf,int len,uint64_t timeout_us){
    const uint8_t *p=(const uint8_t*)buf;int sent=0;uint64_t start=pc_now();
    while(sent<len&&!g_stock_cancel){
        int rc=sceNetSend(s,p+sent,(unsigned)(len-sent),SCE_NET_MSG_DONTWAIT);
        if(rc>0){sent+=rc;g_stock.bytes_tx+=(uint32_t)rc;continue;}
        if(pc_now()-start>=timeout_us){g_stock.stalls++;return -3;}
        sceKernelDelayThread(1000);
    }
    return g_stock_cancel?-4:sent;
}

static void stock_build_identity(uint8_t out[77],int role){
    memset(out,0,77);
    memcpy(out,role==LF2_PC_STOCK_ROLE_HOST?"11110000":"00001111",8);
    memset(out+8,'0',24);
    for(int i=0;i<4;i++){
        pc_name10(out+32+i*11,"Vita",i+1);
        out[32+i*11+10]='_';
    }
    out[76]=0;
}

static void stock_parse_peer_identity(const uint8_t in[77]){
    memcpy(g_stock.peer_slots,in,8);g_stock.peer_slots[8]=0;
    for(int i=0;i<4;i++){
        memcpy(g_stock.peer_names[i],in+32+i*11,10);g_stock.peer_names[i][10]=0;
        for(int j=9;j>=0&&(g_stock.peer_names[i][j]==' '||g_stock.peer_names[i][j]=='-');j--)g_stock.peer_names[i][j]=0;
    }
}

static void stock_generate_seed(void){
    /* The stock host supplies a 3001-byte shared random table.  Exact Windows
       generation is still being reverse-engineered; for host mode the only
       protocol requirement is that both peers consume the identical table. */
    uint32_t x=(uint32_t)pc_now()^0x4c463256u;
    for(int i=0;i<LF2_PC_STOCK_SEED-1;i++){
        x^=x<<13;x^=x>>17;x^=x<<5;
        g_stock_seed[i]=(uint8_t)(x&0xffu);
    }
    g_stock_seed[LF2_PC_STOCK_SEED-1]=0;
}

static void stock_close_socket(volatile int *slot){
    int s=*slot;if(s>=0){sceNetSocketAbort(s,0);sceNetShutdown(s,SCE_NET_SHUT_RDWR);sceNetSocketClose(s);*slot=-1;}
}

void lf2_pc_stock_clock_reset(void){
    memset(g_stock_clock_local,0,sizeof(g_stock_clock_local));
    memset(g_stock_clock_remote,0,sizeof(g_stock_clock_remote));
    g_stock_clock_phase=0;
    g_stock.tu_frames=0;
}

static void stock_handshake_thread_done(void){
    g_stock.running=0;
    g_stock_thread=-1;
    sceKernelExitDeleteThread(0);
}

static int pc_stock_join_thread(SceSize args,void *argp){
    (void)args;(void)argp;int rc;
    g_stock.phase=LF2_PC_STOCK_CONNECTING;
    snprintf(g_stock.message,sizeof(g_stock.message),"LF2 2.00a %s:%d verbinden...",g_stock_target,LF2_PC_PORT);
    int s=sceNetSocket("lf2-stock-client",SCE_NET_AF_INET,SCE_NET_SOCK_STREAM,SCE_NET_IPPROTO_TCP);
    if(s<0){stock_set_error("sceNetSocket",s);stock_handshake_thread_done();return 0;}g_stock_sock=s;
    SceNetSockaddrIn sa;memset(&sa,0,sizeof(sa));sa.sin_len=sizeof(sa);sa.sin_family=SCE_NET_AF_INET;sa.sin_port=sceNetHtons(LF2_PC_PORT);
    rc=sceNetInetPton(SCE_NET_AF_INET,g_stock_target,&sa.sin_addr);if(rc<=0){stock_set_error("ungueltige IPv4",rc);stock_close_socket(&g_stock_sock);stock_handshake_thread_done();return 0;}
    rc=sceNetConnect(s,(SceNetSockaddr*)&sa,sizeof(sa));
    if(g_stock_cancel){g_stock.phase=LF2_PC_STOCK_CLOSED;stock_close_socket(&g_stock_sock);stock_handshake_thread_done();return 0;}
    if(rc<0){stock_set_error("sceNetConnect",rc);stock_close_socket(&g_stock_sock);stock_handshake_thread_done();return 0;}
    int one=1;sceNetSetsockopt(s,SCE_NET_IPPROTO_TCP,SCE_NET_TCP_NODELAY,&one,sizeof(one));
    int nb=1;sceNetSetsockopt(s,SCE_NET_SOL_SOCKET,SCE_NET_SO_NBIO,&nb,sizeof(nb));

    g_stock.phase=LF2_PC_STOCK_HANDSHAKE;stock_set_message("LF2 2.00a Handshake...");
    static const uint8_t banner_expected[14]={'u',' ','c','a','n',' ','c','o','n','n','e','c','t',0};
    uint8_t banner[14],mine[77],peer[77];
    rc=stock_recv_exact(s,banner,14,LF2_PC_STOCK_TIMEOUT_US);if(rc!=14||memcmp(banner,banner_expected,14)){stock_set_error("LF2 banner",rc==14?-20:rc);stock_close_socket(&g_stock_sock);stock_handshake_thread_done();return 0;}
    stock_build_identity(mine,LF2_PC_STOCK_ROLE_CLIENT);
    rc=stock_send_exact(s,mine,77,LF2_PC_STOCK_TIMEOUT_US);if(rc!=77){stock_set_error("identity send",rc);stock_close_socket(&g_stock_sock);stock_handshake_thread_done();return 0;}
    rc=stock_recv_exact(s,peer,77,LF2_PC_STOCK_TIMEOUT_US);if(rc!=77){stock_set_error("identity recv",rc);stock_close_socket(&g_stock_sock);stock_handshake_thread_done();return 0;}
    stock_parse_peer_identity(peer);
    rc=stock_recv_exact(s,g_stock_seed,LF2_PC_STOCK_SEED,LF2_PC_STOCK_TIMEOUT_US);if(rc!=LF2_PC_STOCK_SEED){stock_set_error("seed recv",rc);stock_close_socket(&g_stock_sock);stock_handshake_thread_done();return 0;}
    g_stock.seed_hash=pc_fnv1a(g_stock_seed,sizeof(g_stock_seed));
    memcpy(g_stock.last_rx,g_stock_initial_packet,sizeof(g_stock.last_rx));memcpy(g_stock.last_tx,g_stock_initial_packet,sizeof(g_stock.last_tx));
    lf2_pc_stock_clock_reset();
    g_stock.connected=1;g_stock.last_error=0;g_stock.phase=LF2_PC_STOCK_LOCKSTEP;
    lf2_rng_stock_enable(g_stock_seed,LF2_PC_STOCK_SEED);
    snprintf(g_stock.message,sizeof(g_stock.message),"LF2 2.00a verbunden - Lockstep bereit");
    lf2_log_stage("network:pc_stock_ready","role=client ip=%s peer_slots=%s seed=%08X",g_stock_target,g_stock.peer_slots,g_stock.seed_hash);
    stock_handshake_thread_done();return 0;
}

static int pc_stock_host_thread(SceSize args,void *argp){
    (void)args;(void)argp;int rc;
    g_stock.phase=LF2_PC_STOCK_LISTENING;stock_set_message("LF2 2.00a: warte auf PC auf TCP 12345...");
    int ls=sceNetSocket("lf2-stock-listen",SCE_NET_AF_INET,SCE_NET_SOCK_STREAM,SCE_NET_IPPROTO_TCP);
    if(ls<0){stock_set_error("listen socket",ls);stock_handshake_thread_done();return 0;}g_stock_listen_sock=ls;
    int one=1;sceNetSetsockopt(ls,SCE_NET_SOL_SOCKET,SCE_NET_SO_REUSEADDR,&one,sizeof(one));
    SceNetSockaddrIn sa;memset(&sa,0,sizeof(sa));sa.sin_len=sizeof(sa);sa.sin_family=SCE_NET_AF_INET;sa.sin_port=sceNetHtons(LF2_PC_PORT);sa.sin_addr.s_addr=sceNetHtonl(SCE_NET_INADDR_ANY);
    rc=sceNetBind(ls,(SceNetSockaddr*)&sa,sizeof(sa));if(rc<0){stock_set_error("sceNetBind",rc);stock_close_socket(&g_stock_listen_sock);stock_handshake_thread_done();return 0;}
    rc=sceNetListen(ls,1);if(rc<0){stock_set_error("sceNetListen",rc);stock_close_socket(&g_stock_listen_sock);stock_handshake_thread_done();return 0;}
    SceNetSockaddrIn peer_sa;unsigned int peer_len=sizeof(peer_sa);memset(&peer_sa,0,sizeof(peer_sa));
    int s=sceNetAccept(ls,(SceNetSockaddr*)&peer_sa,&peer_len);
    if(g_stock_cancel){if(s>=0){sceNetSocketClose(s);}stock_close_socket(&g_stock_listen_sock);g_stock.phase=LF2_PC_STOCK_CLOSED;stock_handshake_thread_done();return 0;}
    if(s<0){stock_set_error("sceNetAccept",s);stock_close_socket(&g_stock_listen_sock);stock_handshake_thread_done();return 0;}
    g_stock_sock=s;stock_close_socket(&g_stock_listen_sock);
    sceNetSetsockopt(s,SCE_NET_IPPROTO_TCP,SCE_NET_TCP_NODELAY,&one,sizeof(one));int nb=1;sceNetSetsockopt(s,SCE_NET_SOL_SOCKET,SCE_NET_SO_NBIO,&nb,sizeof(nb));

    g_stock.phase=LF2_PC_STOCK_HANDSHAKE;stock_set_message("LF2 2.00a Host-Handshake...");
    static const uint8_t banner[14]={'u',' ','c','a','n',' ','c','o','n','n','e','c','t',0};
    uint8_t mine[77],peer[77];
    rc=stock_send_exact(s,banner,14,LF2_PC_STOCK_TIMEOUT_US);if(rc!=14){stock_set_error("banner send",rc);stock_close_socket(&g_stock_sock);stock_handshake_thread_done();return 0;}
    rc=stock_recv_exact(s,peer,77,LF2_PC_STOCK_TIMEOUT_US);if(rc!=77){stock_set_error("identity recv",rc);stock_close_socket(&g_stock_sock);stock_handshake_thread_done();return 0;}
    stock_parse_peer_identity(peer);stock_build_identity(mine,LF2_PC_STOCK_ROLE_HOST);
    rc=stock_send_exact(s,mine,77,LF2_PC_STOCK_TIMEOUT_US);if(rc!=77){stock_set_error("identity send",rc);stock_close_socket(&g_stock_sock);stock_handshake_thread_done();return 0;}
    stock_generate_seed();g_stock.seed_hash=pc_fnv1a(g_stock_seed,sizeof(g_stock_seed));
    rc=stock_send_exact(s,g_stock_seed,LF2_PC_STOCK_SEED,LF2_PC_STOCK_TIMEOUT_US);if(rc!=LF2_PC_STOCK_SEED){stock_set_error("seed send",rc);stock_close_socket(&g_stock_sock);stock_handshake_thread_done();return 0;}
    memcpy(g_stock.last_rx,g_stock_initial_packet,sizeof(g_stock.last_rx));memcpy(g_stock.last_tx,g_stock_initial_packet,sizeof(g_stock.last_tx));
    lf2_pc_stock_clock_reset();
    g_stock.connected=1;g_stock.last_error=0;g_stock.phase=LF2_PC_STOCK_LOCKSTEP;
    lf2_rng_stock_enable(g_stock_seed,LF2_PC_STOCK_SEED);
    stock_set_message("LF2 2.00a verbunden - Lockstep bereit");
    lf2_log_stage("network:pc_stock_ready","role=host peer_slots=%s seed=%08X",g_stock.peer_slots,g_stock.seed_hash);
    stock_handshake_thread_done();return 0;
}

static int stock_start_common(int role,const char *ipv4){
    if(g_stock.running||g_stock.connected)return -2;
    int rc=lf2_pc_init();if(rc<0)return rc;
    int state=lf2_pc_inet_state();
    if(state!=SCE_NETCTL_STATE_CONNECTED){memset(&g_stock,0,sizeof(g_stock));g_stock.phase=LF2_PC_STOCK_ERROR;g_stock.inet_state=state;g_stock.last_error=(int)0x80412108u;snprintf(g_stock.message,sizeof(g_stock.message),"Vita WLAN nicht verbunden (state=%d)",state);return g_stock.last_error;}
    memset(&g_stock,0,sizeof(g_stock));g_stock.running=1;g_stock.phase=role==LF2_PC_STOCK_ROLE_HOST?LF2_PC_STOCK_LISTENING:LF2_PC_STOCK_CONNECTING;g_stock.role=role;g_stock.inet_state=state;g_stock_cancel=0;g_stock_sock=-1;g_stock_listen_sock=-1;
    if(ipv4){SceNetInAddr tmp;if(sceNetInetPton(SCE_NET_AF_INET,ipv4,&tmp)<=0){g_stock.running=0;stock_set_error("ungueltige IPv4",-3);return -3;}snprintf(g_stock.target_ip,sizeof(g_stock.target_ip),"%s",ipv4);snprintf(g_stock_target,sizeof(g_stock_target),"%s",ipv4);}else{g_stock_target[0]=0;}
    SceKernelThreadEntry entry=role==LF2_PC_STOCK_ROLE_HOST?pc_stock_host_thread:pc_stock_join_thread;
    g_stock_thread=sceKernelCreateThread(role==LF2_PC_STOCK_ROLE_HOST?"lf2-stock-host":"lf2-stock-client",entry,LF2_PC_THREAD_PRIORITY,LF2_PC_THREAD_STACK,0,SCE_KERNEL_CPU_MASK_USER_0,NULL);
    if(g_stock_thread<0){int e=g_stock_thread;g_stock_thread=-1;g_stock.running=0;stock_set_error("CreateThread",e);return e;}
    rc=sceKernelStartThread(g_stock_thread,0,NULL);if(rc<0){sceKernelDeleteThread(g_stock_thread);g_stock_thread=-1;g_stock.running=0;stock_set_error("StartThread",rc);return rc;}
    return 0;
}

int lf2_pc_stock_join_start(const char *ipv4){return ipv4&&ipv4[0]?stock_start_common(LF2_PC_STOCK_ROLE_CLIENT,ipv4):-1;}
int lf2_pc_stock_host_start(void){return stock_start_common(LF2_PC_STOCK_ROLE_HOST,NULL);}

void lf2_pc_stock_stop(void){
    lf2_rng_stock_disable();
    lf2_pc_stock_clock_reset();
    g_stock_cancel=1;
    int s=g_stock_sock;if(s>=0)sceNetSocketAbort(s,0);
    int ls=g_stock_listen_sock;if(ls>=0)sceNetSocketAbort(ls,0);
    SceUID th=g_stock_thread;if(th>=0){sceKernelWaitThreadEnd(th,NULL,NULL);g_stock_thread=-1;}
    stock_close_socket(&g_stock_sock);stock_close_socket(&g_stock_listen_sock);
    if(g_stock.phase!=LF2_PC_STOCK_IDLE)lf2_log_stage("network:pc_stock_stop","role=%d phase=%d frames=%u stalls=%u",g_stock.role,g_stock.phase,g_stock.frames,g_stock.stalls);
    g_stock.running=0;g_stock.connected=0;if(g_stock.phase!=LF2_PC_STOCK_ERROR)g_stock.phase=LF2_PC_STOCK_CLOSED;
}

const lf2_pc_stock_state_t *lf2_pc_stock_state(void){
    if(g_netctl_ready){int state=-1;if(sceNetCtlInetGetState(&state)>=0)g_stock.inet_state=state;}
    return &g_stock;
}
int lf2_pc_stock_ready(void){return g_stock.connected&&g_stock.phase==LF2_PC_STOCK_LOCKSTEP&&g_stock_sock>=0;}
int lf2_pc_stock_role(void){return g_stock.role;}
uint32_t lf2_pc_stock_seed_hash(void){return g_stock.seed_hash;}
const uint8_t *lf2_pc_stock_seed_data(void){return g_stock_seed;}

uint8_t lf2_pc_stock_encode_keys(uint32_t m){
    uint8_t v=1;
    if(m&LF2_REMOTE_DEFEND)v|=0x02;
    if(m&LF2_REMOTE_JUMP)v|=0x04;
    if(m&LF2_REMOTE_ATTACK)v|=0x08;
    if(m&LF2_REMOTE_RIGHT)v|=0x10;
    if(m&LF2_REMOTE_LEFT)v|=0x20;
    if(m&LF2_REMOTE_UP)v|=0x40;
    if(m&LF2_REMOTE_DOWN)v|=0x80;
    return v;
}

uint32_t lf2_pc_stock_decode_keys(uint8_t v){
    uint32_t m=0;
    if(v&0x02)m|=LF2_REMOTE_DEFEND;
    if(v&0x04)m|=LF2_REMOTE_JUMP;
    if(v&0x08)m|=LF2_REMOTE_ATTACK;
    if(v&0x10)m|=LF2_REMOTE_RIGHT;
    if(v&0x20)m|=LF2_REMOTE_LEFT;
    if(v&0x40)m|=LF2_REMOTE_UP;
    if(v&0x80)m|=LF2_REMOTE_DOWN;
    return m;
}

static int stock_is_key_byte(int i){
    return i>=4&&i<=11;
}

static void stock_packet_hex(const uint8_t p[LF2_PC_STOCK_PACKET],char out[LF2_PC_STOCK_PACKET*2+1]){
    static const char hex[]="0123456789ABCDEF";
    for(int i=0;i<LF2_PC_STOCK_PACKET;i++){
        out[i*2]=hex[p[i]>>4];
        out[i*2+1]=hex[p[i]&15];
    }
    out[LF2_PC_STOCK_PACKET*2]=0;
}

static uint32_t stock_note_opaque_changes(const uint8_t rx[LF2_PC_STOCK_PACKET]){
    uint32_t mask=0;
    for(int i=0;i<LF2_PC_STOCK_PACKET;i++){
        if(stock_is_key_byte(i))continue;
        if(rx[i]!=g_stock.last_rx[i]){
            mask|=(1u<<i);
            g_stock.opaque_byte_changes[i]++;
        }
    }
    if(mask){
        g_stock.opaque_change_events++;
        g_stock.opaque_change_mask=mask;
    }
    return mask;
}

int lf2_pc_stock_exchange(const uint32_t local_masks[4],uint32_t remote_masks[4],uint32_t timeout_ms){
    if(!lf2_pc_stock_ready()||!local_masks||!remote_masks)return -1;
    int s=g_stock_sock;uint64_t timeout_us=timeout_ms?(uint64_t)timeout_ms*1000ULL:LF2_PC_STOCK_TIMEOUT_US;
    uint8_t rx[LF2_PC_STOCK_PACKET],tx[LF2_PC_STOCK_PACKET];int rc;
    memset(remote_masks,0,4*sizeof(*remote_masks));uint64_t start=pc_now();
    if(g_stock.role==LF2_PC_STOCK_ROLE_HOST){
        rc=stock_recv_exact(s,rx,sizeof(rx),timeout_us);if(rc!=(int)sizeof(rx)){stock_set_error("lockstep recv",rc);stock_close_socket(&g_stock_sock);return rc;}
        memcpy(tx,rx,sizeof(tx));
        for(int i=0;i<4;i++){remote_masks[i]=lf2_pc_stock_decode_keys(rx[8+i]);tx[4+i]=lf2_pc_stock_encode_keys(local_masks[i]);}
        rc=stock_send_exact(s,tx,sizeof(tx),timeout_us);if(rc!=(int)sizeof(tx)){stock_set_error("lockstep send",rc);stock_close_socket(&g_stock_sock);return rc;}
    }else{
        memcpy(tx,g_stock.last_rx,sizeof(tx));
        for(int i=0;i<4;i++)tx[8+i]=lf2_pc_stock_encode_keys(local_masks[i]);
        rc=stock_send_exact(s,tx,sizeof(tx),timeout_us);if(rc!=(int)sizeof(tx)){stock_set_error("lockstep send",rc);stock_close_socket(&g_stock_sock);return rc;}
        rc=stock_recv_exact(s,rx,sizeof(rx),timeout_us);if(rc!=(int)sizeof(rx)){stock_set_error("lockstep recv",rc);stock_close_socket(&g_stock_sock);return rc;}
        for(int i=0;i<4;i++)remote_masks[i]=lf2_pc_stock_decode_keys(rx[4+i]);
    }
    uint32_t opaque_mask=stock_note_opaque_changes(rx);
    g_stock.frames++;g_stock.last_exchange_ms=(uint32_t)((pc_now()-start)/1000ULL);
    int periodic=(g_stock.frames<=4||(g_stock.frames%300u)==0u);
    int opaque_log=opaque_mask&&(g_stock.opaque_change_events<=32u||(g_stock.opaque_change_events%60u)==0u);
    if(periodic||opaque_log){
        char rxhex[LF2_PC_STOCK_PACKET*2+1],txhex[LF2_PC_STOCK_PACKET*2+1];
        stock_packet_hex(rx,rxhex);stock_packet_hex(tx,txhex);
        lf2_logf("NETPC","stock frame=%u role=%d txkeys=%02X/%02X/%02X/%02X rxkeys=%02X/%02X/%02X/%02X opaque=%06X events=%u ms=%u rx=%s tx=%s",g_stock.frames,g_stock.role,
            g_stock.role==LF2_PC_STOCK_ROLE_HOST?tx[4]:tx[8],g_stock.role==LF2_PC_STOCK_ROLE_HOST?tx[5]:tx[9],g_stock.role==LF2_PC_STOCK_ROLE_HOST?tx[6]:tx[10],g_stock.role==LF2_PC_STOCK_ROLE_HOST?tx[7]:tx[11],
            g_stock.role==LF2_PC_STOCK_ROLE_HOST?rx[8]:rx[4],g_stock.role==LF2_PC_STOCK_ROLE_HOST?rx[9]:rx[5],g_stock.role==LF2_PC_STOCK_ROLE_HOST?rx[10]:rx[6],g_stock.role==LF2_PC_STOCK_ROLE_HOST?rx[11]:rx[7],
            (unsigned)opaque_mask,g_stock.opaque_change_events,g_stock.last_exchange_ms,rxhex,txhex);
    }
    memcpy(g_stock.last_rx,rx,sizeof(rx));memcpy(g_stock.last_tx,tx,sizeof(tx));
    return 0;
}

int lf2_pc_stock_clock_step(uint32_t local_masks[4],uint32_t remote_masks[4],uint32_t timeout_ms){
    if(!local_masks||!remote_masks||!lf2_pc_stock_ready())return -1;
    int rc=0;
    if(g_stock_clock_phase==0){
        uint32_t proposed[4];memcpy(proposed,local_masks,sizeof(proposed));
        rc=lf2_pc_stock_exchange(proposed,g_stock_clock_remote,timeout_ms);
        if(rc<0)return rc;
        memcpy(g_stock_clock_local,proposed,sizeof(g_stock_clock_local));
        g_stock_clock_phase=1;
    }else{
        g_stock_clock_phase=0;
    }
    memcpy(local_masks,g_stock_clock_local,sizeof(g_stock_clock_local));
    memcpy(remote_masks,g_stock_clock_remote,sizeof(g_stock_clock_remote));
    g_stock.tu_frames++;
    if(g_stock.tu_frames<=4||(g_stock.tu_frames%600u)==0u){
        lf2_logf("NETPC","stock TU=%u packet=%u phase=%d rng=%u/%u calls=%u",g_stock.tu_frames,g_stock.frames,g_stock_clock_phase,lf2_rng_stock_i(),lf2_rng_stock_j(),lf2_rng_calls());
    }
    return 0;
}

void lf2_pc_shutdown(void){
    lf2_pc_stock_stop();
    lf2_pc_bridge_disconnect();
    lf2_pc_probe_cancel();
    SceUID th=g_thread;if(th>=0){sceKernelWaitThreadEnd(th,NULL,NULL);g_thread=-1;}
    int s=g_sock;if(s>=0){sceNetSocketAbort(s,0);sceNetSocketClose(s);g_sock=-1;}
    if(g_net_ready||g_netctl_ready)lf2_log_stage("network:pc_shutdown","phase=%s",lf2_pc_probe_phase_name(g_state.phase));
    pc_cleanup_net();
}

