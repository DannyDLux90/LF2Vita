int lf2_adhoc_init(void){
    if(g_init)return 0;
    g_error[0]=0;memset(&g_lobby,0,sizeof(g_lobby));g_ev_head=g_ev_tail=0;g_event_drops=0;g_event_peak=0;g_last_lobby_broadcast=0;g_last_player_refresh=0;
    int rc=sceKernelCreateLwMutex(&g_event_mutex,"lf2adhoc",0,0,NULL);
    if(rc<0){set_error("CreateLwMutex",rc);return rc;}g_mutex_ready=true;
    rc=sceKernelCreateLwMutex(&g_input_mutex,"lf2input",0,0,NULL);
    if(rc<0){set_error("CreateInputLwMutex",rc);goto fail;}g_input_mutex_ready=true;

    rc=sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    if(rc<0){set_error("load NET",rc);goto fail;}g_mod_net_loaded=true;
    rc=sceSysmoduleLoadModule(SCE_SYSMODULE_PSPNET_ADHOC);
    if(rc<0){set_error("load PSPNET_ADHOC",rc);goto fail;}g_mod_adhoc_loaded=true;
    rc=sceSysmoduleLoadModule(SCE_SYSMODULE_NET_ADHOC_MATCHING);
    if(rc<0){set_error("load ADHOC_MATCHING",rc);goto fail;}g_mod_matching_loaded=true;

    SceNetInitParam np;memset(&np,0,sizeof(np));np.size=LF2_NET_POOL;
    g_net_mem=memalign(64,np.size);if(!g_net_mem){rc=-1;snprintf(g_error,sizeof(g_error),"net memory allocation failed");goto fail;}
    np.memory=g_net_mem;
    rc=sceNetInit(&np);if(rc<0){set_error("sceNetInit",rc);goto fail;}g_net_ready=true;

    /* SceNetAdhocMatchingStart depends on SceNetCtl being initialized.
       Without this, real Vita hardware returns 0x80412101
       (SCE_NET_CTL_ERROR_NOT_INITIALIZED) before discovery can start. */
    rc=sceNetCtlInit();
    if(rc<0){set_error("sceNetCtlInit",rc);goto fail;}
    g_netctl_ready=true;

    rc=sceNetAdhocInit();
    if(rc==SCE_ERROR_NET_ADHOC_ALREADY_INITIALIZED)g_adhoc_ready=true;
    else if(rc<0){set_error("sceNetAdhocInit",rc);goto fail;}
    else g_adhoc_ready=true;

    SceNetAdhocctlAdhocId id;memset(&id,0,sizeof(id));id.type=SCE_NET_ADHOCCTL_ADHOCTYPE_RESERVED;memcpy(id.data,"LF2V00001",9);
    rc=sceNetAdhocctlInit(&id);
    if(rc==SCE_ERROR_NET_ADHOCCTL_ALREADY_INITIALIZED)g_ctl_ready=true;
    else if(rc<0){set_error("sceNetAdhocctlInit",rc);goto fail;}
    else g_ctl_ready=true;
    refresh_adhoc_diag("after AdhocctlInit");
    lf2_logf("NET","%s",g_diag);

    g_match_mem=memalign(64,LF2_MATCH_POOL);
    if(!g_match_mem){rc=-1;snprintf(g_error,sizeof(g_error),"matching memory allocation failed");goto fail;}
    rc=sceNetAdhocMatchingInit(LF2_MATCH_POOL,g_match_mem);
    if(rc==SCE_NET_ADHOC_MATCHING_ERROR_ALREADY_INITIALIZED)g_matching_ready=true;
    else if(rc<0){set_error("MatchingInit",rc);goto fail;}
    else g_matching_ready=true;

    memset(g_name,0,sizeof(g_name));
    if(sceAppUtilSystemParamGetString(SCE_SYSTEM_PARAM_ID_USERNAME,(SceChar8*)g_name,sizeof(g_name))<0||!g_name[0])strcpy(g_name,"Vita Player");
    g_avatar_hash=local_avatar_hash();
    g_local_peer_id=fnv1a(g_name,strlen(g_name))^(uint32_t)now_us()^g_avatar_hash;if(!g_local_peer_id)g_local_peer_id=1;
    g_local_character=0;g_local_partner=1;g_local_battle_follower=10;g_local_battle_count=2;g_local_ready=0;g_last_logged_revision=0;g_init=true;
    lf2_logf("NET","adhoc init user=%s avatar=%08X peer=%08X proto=%d",g_name,g_avatar_hash,g_local_peer_id,LF2_ADHOC_PROTO_VERSION);
    return 0;
fail:
    adhoc_cleanup_runtime();g_init=false;return rc;
}

void lf2_adhoc_shutdown(void){
    if(!g_init&&!g_mutex_ready)return;
    lf2_log_stage("network:adhoc_shutdown","active=%d host=%d connected=%d",g_lobby.active?1:0,g_host?1:0,g_lobby.connected?1:0);
    adhoc_cleanup_runtime();memset(&g_lobby,0,sizeof(g_lobby));g_host=false;g_init=false;
}
const char *lf2_adhoc_local_name(void){return g_name;}uint32_t lf2_adhoc_local_avatar_hash(void){return g_avatar_hash;}const char *lf2_adhoc_error(void){return g_error;}const char *lf2_adhoc_diag(void){refresh_adhoc_diag(g_browser?"Browser aktiv":(g_host?"Host aktiv":"AdHoc bereit"));return g_diag;}const char *lf2_adhoc_game_version(void){return LF2_GAME_VERSION;}const char *lf2_adhoc_vita_version(void){return LF2_VITA_VERSION;}

static int start_matching(int mode,const void *hello,int hello_len){
    clear_match();g_ev_head=g_ev_tail=0;
    refresh_adhoc_diag("vor MatchingCreate");lf2_logf("NET","%s",g_diag);
    g_match=sceNetAdhocMatchingCreate(mode,LF2_ADHOC_MAX_PLAYERS,LF2_MATCH_PORT,LF2_MATCH_RX,LF2_MATCH_HELLO_US,LF2_MATCH_PING_US,LF2_MATCH_RETRY_COUNT,LF2_MATCH_REXMT_US,matching_cb);
    if(g_match<0){set_error("MatchingCreate",g_match);return g_match;}
    int rc=sceNetAdhocMatchingStart(g_match,LF2_MATCH_THREAD_PRIORITY,LF2_MATCH_THREAD_STACK,LF2_MATCH_THREAD_AFFINITY,hello_len,(void*)hello);
    if(rc<0){set_error("MatchingStart",rc);clear_match();return rc;}
    refresh_adhoc_diag(mode==SCE_NET_ADHOC_MATCHING_MODE_PARENT?"Matching HOST gestartet":"Matching BROWSER gestartet");
    lf2_logf("NET","%s pri=0x%08X stack=0x%X affinity=0x%X retry=%d rexmt_us=%u",g_diag,LF2_MATCH_THREAD_PRIORITY,LF2_MATCH_THREAD_STACK,LF2_MATCH_THREAD_AFFINITY,LF2_MATCH_RETRY_COUNT,(unsigned)LF2_MATCH_REXMT_US);
    return 0;
}

int lf2_adhoc_browser_start(void){int rc=lf2_adhoc_init();if(rc<0)return rc;g_host=false;g_session_count=0;memset(g_sessions,0,sizeof(g_sessions));rc=start_matching(SCE_NET_ADHOC_MATCHING_MODE_CHILD,NULL,0);if(rc>=0){g_browser=true;refresh_adhoc_diag("Browser aktiv");lf2_log_stage("network:adhoc_scan","started=1 %s",g_diag);}else lf2_log_stage("network:adhoc_scan","started=0 rc=0x%08X error=%s",(unsigned)rc,g_error);return rc;}
void lf2_adhoc_browser_stop(void){if(g_browser)clear_match();g_session_count=0;}
int lf2_adhoc_browser_count(void){return g_session_count;}
const lf2_adhoc_session_t *lf2_adhoc_browser_session(int i){return (i>=0&&i<g_session_count)?&g_sessions[i]:NULL;}

static int session_find(uint32_t a){for(int i=0;i<g_session_count;i++)if(g_sessions[i].address==a)return i;return -1;}
static void browser_hello(const queued_event_t *e){
    if(e->len<(int)sizeof(hello_t))return;hello_t h;memcpy(&h,e->data,sizeof(h));if(h.magic!=LF2_HELLO_MAGIC||h.proto!=LF2_ADHOC_PROTO_VERSION)return;
    if(strncmp(h.game_version,LF2_GAME_VERSION,sizeof(h.game_version))!=0||strncmp(h.vita_version,LF2_VITA_VERSION,sizeof(h.vita_version))!=0){
        lf2_logf("NET","browser ignored incompatible host=%s game=%s vita=%s local_game=%s local_vita=%s",h.host,h.game_version,h.vita_version,LF2_GAME_VERSION,LF2_VITA_VERSION);
        return;
    }
    int i=session_find(e->addr.s_addr);bool fresh=false;
    if(i<0){if(g_session_count>=LF2_ADHOC_MAX_SESSIONS)return;i=g_session_count++;memset(&g_sessions[i],0,sizeof(g_sessions[i]));g_sessions[i].address=e->addr.s_addr;g_sessions[i].ping_ms=0xffff;fresh=true;}
    lf2_adhoc_session_t *s=&g_sessions[i];uint32_t oldrev=s->revision;
    strncpy(s->host,h.host,sizeof(s->host)-1);s->avatar_hash=h.avatar_hash;s->players=h.players;s->max_players=h.max_players;s->status=h.status;s->mode=h.mode;s->stage=h.stage;s->difficulty=h.difficulty;s->revision=h.revision;strncpy(s->game_version,h.game_version,sizeof(s->game_version)-1);strncpy(s->vita_version,h.vita_version,sizeof(s->vita_version)-1);s->last_seen_us=e->when_us;
    if(fresh||oldrev!=h.revision)lf2_logf("NET","browser host=%s players=%u/%u status=%s mode=%s rev=%u%s",s->host,s->players,s->max_players,lf2_net_status_name(s->status),lf2_net_mode_name(s->mode),s->revision,fresh?" discovered":" updated");
}

int lf2_adhoc_browser_probe(int i){if(!g_browser||g_match<0||i<0||i>=g_session_count||g_probe_index>=0)return -1;join_t j;memset(&j,0,sizeof(j));j.magic=LF2_NET_MAGIC;j.proto=LF2_ADHOC_PROTO_VERSION;j.probe=1;j.avatar_hash=g_avatar_hash;j.peer_id=g_local_peer_id;strncpy(j.name,g_name,sizeof(j.name)-1);strncpy(j.game_version,LF2_GAME_VERSION,sizeof(j.game_version)-1);strncpy(j.vita_version,LF2_VITA_VERSION,sizeof(j.vita_version)-1);g_probe_addr.s_addr=g_sessions[i].address;int rc=sceNetAdhocMatchingSelectTarget(g_match,&g_probe_addr,sizeof(j),&j);if(rc>=0){g_probe_index=i;g_probe_started=now_us();}return rc;}

static pending_t *pending_find(uint32_t a,bool create){for(int i=0;i<(int)(sizeof(g_pending)/sizeof(g_pending[0]));i++)if(g_pending[i].valid&&g_pending[i].addr.s_addr==a)return &g_pending[i];if(create)for(int i=0;i<(int)(sizeof(g_pending)/sizeof(g_pending[0]));i++)if(!g_pending[i].valid){g_pending[i].valid=true;g_pending[i].addr.s_addr=a;return &g_pending[i];}return NULL;}
static peer_t *peer_find(uint32_t a){for(int i=0;i<LF2_ADHOC_MAX_PLAYERS;i++)if(g_peers[i].used&&g_peers[i].addr.s_addr==a)return &g_peers[i];return NULL;}
static int free_player_slot(void){for(int i=1;i<g_lobby.settings.max_players&&i<LF2_ADHOC_MAX_PLAYERS;i++)if(!g_lobby.players[i].present)return i;return -1;}

static int send_to(const SceNetInAddr *a,const void *p,int n){if(g_match<0)return -1;int rc=sceNetAdhocMatchingSendData(g_match,(SceNetInAddr*)a,n,(void*)p);if(rc<0&&rc!=SCE_NET_ADHOC_MATCHING_ERROR_DATA_BUSY)set_error("MatchingSendData",rc);return rc;}
static void send_lobby_to(const SceNetInAddr *a){lobby_packet_t p;memset(&p,0,sizeof(p));p.h.magic=LF2_NET_MAGIC;p.h.proto=LF2_ADHOC_PROTO_VERSION;p.h.type=PKT_LOBBY;p.h.size=sizeof(p);p.h.seq=g_seq++;p.revision=g_lobby.revision;p.settings=g_lobby.settings;memcpy(p.players,g_lobby.players,sizeof(p.players));send_to(a,&p,sizeof(p));}
static void broadcast_lobby(void){for(int i=0;i<LF2_ADHOC_MAX_PLAYERS;i++)if(g_peers[i].used)send_lobby_to(&g_peers[i].addr);update_hello();}
