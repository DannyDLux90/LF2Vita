static uint64_t now_us(void){return sceKernelGetSystemTimeWide();}
static void set_error(const char *where,int rc){snprintf(g_error,sizeof(g_error),"%s: 0x%08X",where,(unsigned)rc);lf2_logf("NET","%s",g_error);}
static uint32_t fnv1a(const void *ptr,size_t n){const uint8_t *p=ptr;uint32_t h=2166136261u;for(size_t i=0;i<n;i++){h^=p[i];h*=16777619u;}return h;}

static uint32_t local_avatar_hash(void){
    int fd=sceIoOpen("ur0:user/00/np/myprofile.dat",SCE_O_RDONLY,0);
    if(fd>=0){uint8_t b[256];int n=sceIoRead(fd,b,sizeof(b));sceIoClose(fd);if(n>0)return fnv1a(b,(size_t)n);}
    return fnv1a(g_name,strlen(g_name));
}

static void queue_event(int event,const SceNetInAddr *addr,const void *data,int len){
    sceKernelLockLwMutex(&g_event_mutex,1,NULL);
    int next=(g_ev_tail+1)%64;
    if(next!=g_ev_head){queued_event_t *q=&g_events[g_ev_tail];memset(q,0,sizeof(*q));q->event=event;if(addr)q->addr=*addr;q->when_us=now_us();if(data&&len>0){if(len>(int)sizeof(q->data))len=sizeof(q->data);memcpy(q->data,data,len);q->len=len;}g_ev_tail=next;}
    sceKernelUnlockLwMutex(&g_event_mutex,1);
}

static void matching_cb(int id,int event,SceNetInAddr *addr,SceSize opt_len,void *opt){(void)id;queue_event(event,addr,opt,(int)opt_len);}
static bool pop_event(queued_event_t *out){bool ok=false;sceKernelLockLwMutex(&g_event_mutex,1,NULL);if(g_ev_head!=g_ev_tail){*out=g_events[g_ev_head];g_ev_head=(g_ev_head+1)%64;ok=true;}sceKernelUnlockLwMutex(&g_event_mutex,1);return ok;}

const char *lf2_net_mode_name(int m){static const char *n[]={"VS","Stage","Battle","1 vs 1","2 vs 2"};return (m>=0&&m<5)?n[m]:"?";}
const char *lf2_net_status_name(int s){static const char *n[]={"Warte auf Spieler","Startet","Im Spiel","Ergebnis"};return (s>=0&&s<4)?n[s]:"Unbekannt";}
int lf2_net_ping_bars(uint16_t p){if(p==0xffff)return 0;if(p<=20)return 5;if(p<=45)return 4;if(p<=80)return 3;if(p<=140)return 2;return 1;}

static hello_t make_hello(void){
    hello_t h;memset(&h,0,sizeof(h));h.magic=LF2_HELLO_MAGIC;h.proto=LF2_ADHOC_PROTO_VERSION;h.avatar_hash=g_avatar_hash;strncpy(h.host,g_name,sizeof(h.host)-1);strncpy(h.game_version,LF2_GAME_VERSION,sizeof(h.game_version)-1);strncpy(h.vita_version,LF2_VITA_VERSION,sizeof(h.vita_version)-1);
    if(g_host&&g_lobby.active){int c=0;for(int i=0;i<LF2_ADHOC_MAX_PLAYERS;i++)if(g_lobby.players[i].present)c++;h.players=c;h.max_players=g_lobby.settings.max_players;h.status=g_lobby.settings.status;h.mode=g_lobby.settings.mode;h.stage=g_lobby.settings.stage;h.difficulty=g_lobby.settings.difficulty;h.revision=g_lobby.revision;}
    else {h.max_players=8;h.status=LF2_NET_STATUS_WAITING;}
    return h;
}
static void update_hello(void){if(g_match<0||!g_host)return;hello_t h=make_hello();int rc=sceNetAdhocMatchingSetHelloOpt(g_match,sizeof(h),&h);if(rc<0)set_error("MatchingSetHelloOpt",rc);}

static void clear_match(void){
    if(g_match>=0){sceNetAdhocMatchingStop(g_match);sceNetAdhocMatchingDelete(g_match);g_match=-1;}
    g_browser=false;g_probe_index=-1;g_joining=false;g_probe_ignore_addr=0;g_probe_ignore_until=0;
    g_have_join_host=false;memset(&g_join_host_addr,0,sizeof(g_join_host_addr));
    memset(g_pending,0,sizeof(g_pending));memset(g_peers,0,sizeof(g_peers));
}

static void adhoc_cleanup_runtime(void){
    clear_match();
    if(g_matching_ready){sceNetAdhocMatchingTerm();g_matching_ready=false;}
    if(g_ctl_ready){sceNetAdhocctlTerm();g_ctl_ready=false;}
    if(g_adhoc_ready){sceNetAdhocTerm();g_adhoc_ready=false;}
    if(g_net_ready){sceNetTerm();g_net_ready=false;}
    free(g_match_mem);free(g_net_mem);g_match_mem=g_net_mem=NULL;
    if(g_mod_matching_loaded){sceSysmoduleUnloadModule(SCE_SYSMODULE_NET_ADHOC_MATCHING);g_mod_matching_loaded=false;}
    if(g_mod_adhoc_loaded){sceSysmoduleUnloadModule(SCE_SYSMODULE_PSPNET_ADHOC);g_mod_adhoc_loaded=false;}
    if(g_mod_net_loaded){sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);g_mod_net_loaded=false;}
    if(g_mutex_ready){sceKernelDeleteLwMutex(&g_event_mutex);g_mutex_ready=false;}
}

