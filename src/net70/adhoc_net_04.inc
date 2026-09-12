static void host_join_request(const queued_event_t *e){
    if(e->len<(int)sizeof(join_t))return;join_t j;memcpy(&j,e->data,sizeof(j));if(j.magic!=LF2_NET_MAGIC||j.proto!=LF2_ADHOC_PROTO_VERSION)return;if(strncmp(j.game_version,LF2_GAME_VERSION,sizeof(j.game_version))!=0||strncmp(j.vita_version,LF2_VITA_VERSION,sizeof(j.vita_version))!=0){lf2_logf("NET","join rejected incompatible name=%s game=%s vita=%s",j.name,j.game_version,j.vita_version);sceNetAdhocMatchingCancelTarget(g_match,(SceNetInAddr*)&e->addr);return;}pending_t *p=pending_find(e->addr.s_addr,true);if(!p)return;p->probe=j.probe!=0;p->join=j;
    if(!p->probe&&free_player_slot()<0){sceNetAdhocMatchingCancelTarget(g_match,&p->addr);p->valid=false;return;}
    /* Selecting a JOINing target is the host-side accept operation. Return the
       current hello so the client can validate protocol/version immediately. */
    hello_t h=make_hello();int rc=sceNetAdhocMatchingSelectTarget(g_match,&p->addr,sizeof(h),&h);lf2_logf("NET","join request name=%s probe=%d rc=0x%08X",j.name,j.probe,rc);
}

static void host_complete(const queued_event_t *e){pending_t *p=pending_find(e->addr.s_addr,false);if(!p)return;if(p->probe){/* Probe connections are deliberately never inserted in the player list. */p->valid=false;return;}int slot=free_player_slot();if(slot<0)return;peer_t *peer=peer_find(e->addr.s_addr);if(!peer){for(int i=0;i<LF2_ADHOC_MAX_PLAYERS;i++)if(!g_peers[i].used){peer=&g_peers[i];peer->used=true;peer->addr=e->addr;peer->slot=slot;break;}}if(!peer)return;lf2_net_player_t *pl=&g_lobby.players[slot];memset(pl,0,sizeof(*pl));pl->present=1;pl->character=p->join.character;pl->partner_character=p->join.partner_character;pl->battle_follower=p->join.battle_follower;pl->battle_count=p->join.battle_count;pl->team=1;pl->avatar_hash=p->join.avatar_hash;pl->peer_id=p->join.peer_id;strncpy(pl->name,p->join.name,sizeof(pl->name)-1);pl->ping_ms=0xffff;g_lobby.revision++;p->valid=false;broadcast_lobby();lf2_logf("NET","peer joined slot=%d name=%s",slot,pl->name);}

static void host_remove(uint32_t a){peer_t *peer=peer_find(a);if(peer){int slot=peer->slot;memset(peer,0,sizeof(*peer));if(slot>0&&slot<LF2_ADHOC_MAX_PLAYERS)memset(&g_lobby.players[slot],0,sizeof(g_lobby.players[slot]));g_lobby.revision++;broadcast_lobby();}}

static void cache_input_one(match_input_cache_t *cache,uint32_t match_id,uint32_t frame,uint32_t held){
    match_input_cache_t *c=&cache[frame%LF2_MATCH_CACHE_CAP];
    c->valid=true;c->match_id=match_id;c->frame=frame;c->held=held;
}

static bool peek_input_one(match_input_cache_t *cache,uint32_t match_id,uint32_t frame,uint32_t *held){
    match_input_cache_t *c=&cache[frame%LF2_MATCH_CACHE_CAP];
    if(!c->valid||c->match_id!=match_id||c->frame!=frame)return false;
    if(held)*held=c->held;return true;
}

static void native_compare_state_if_ready(void){
    uint32_t peer_tu=0,peer_digest=0;
    if(g_input_mutex_ready){
        sceKernelLockLwMutex(&g_input_mutex,1,NULL);
        peer_tu=g_native_peer_state_tu;peer_digest=g_native_peer_state_digest;
        sceKernelUnlockLwMutex(&g_input_mutex,1);
    }else {peer_tu=g_native_peer_state_tu;peer_digest=g_native_peer_state_digest;}
    if(!g_native_match_active||!g_native_state_tu||g_native_state_tu!=peer_tu)return;
    if(g_native_state_digest!=peer_digest&&!g_native_desync){
        g_native_desync=true;g_native_desyncs++;
        lf2_logf("NETDESYNC","id=%u tu=%u local=%08X peer=%08X",(unsigned)g_native_match_id,(unsigned)g_native_state_tu,(unsigned)g_native_state_digest,(unsigned)peer_digest);
    }
}

static bool match_input_packet_valid(const match_input_packet_t *p){
    if(!p||p->count<1||p->count>LF2_MATCH_INPUT_WINDOW||p->newest_frame<p->first_frame)return false;
    return p->newest_frame-p->first_frame+1u==(uint32_t)p->count;
}

static void cache_match_input_locked(const match_input_packet_t *p){
    if(!match_input_packet_valid(p))return;
    for(uint32_t i=0;i<(uint32_t)p->count;i++)cache_input_one(g_match_inputs,p->match_id,p->first_frame+i,p->held[i]);
    /* Packets may arrive just before match_begin and must still seed frame 0.
       Progress/ACK diagnostics, however, belong only to the active match. */
    if(!g_native_match_active||p->match_id!=g_native_match_id)return;
    if(!g_native_peer_highest_valid||p->newest_frame>g_native_peer_highest){g_native_peer_highest=p->newest_frame;g_native_peer_highest_valid=true;}
    if(p->ack_frame!=0xffffffffu&&(!g_native_peer_ack_valid||p->ack_frame>g_native_peer_ack)){g_native_peer_ack=p->ack_frame;g_native_peer_ack_valid=true;}
    if(p->state_tu&&p->state_tu>=g_native_peer_state_tu){g_native_peer_state_tu=p->state_tu;g_native_peer_state_digest=p->state_digest;}
}

static uint32_t native_ack_frame_snapshot(void){
    uint32_t ack=0xffffffffu;
    if(g_input_mutex_ready)sceKernelLockLwMutex(&g_input_mutex,1,NULL);
    if(g_native_peer_highest_valid)ack=g_native_peer_highest;
    if(g_input_mutex_ready)sceKernelUnlockLwMutex(&g_input_mutex,1);
    return ack;
}

static bool native_peer_acked(uint32_t frame){
    bool ok=false;
    if(g_input_mutex_ready)sceKernelLockLwMutex(&g_input_mutex,1,NULL);
    ok=g_native_peer_ack_valid&&g_native_peer_ack>=frame;
    if(g_input_mutex_ready)sceKernelUnlockLwMutex(&g_input_mutex,1);
    return ok;
}

static void cache_match_input(const match_input_packet_t *p){
    if(!match_input_packet_valid(p))return;
    if(g_input_mutex_ready)sceKernelLockLwMutex(&g_input_mutex,1,NULL);
    cache_match_input_locked(p);
    if(g_input_mutex_ready)sceKernelUnlockLwMutex(&g_input_mutex,1);
    native_compare_state_if_ready();
}

/* Called on the matching worker. Only latency-critical input packets for the
   currently active match are consumed here. Everything else remains on the
   normal event queue so lobby/browser state stays single-threaded. */
static bool fast_match_input(const SceNetInAddr *addr,const void *data,int len){
    if(!addr||!data||len<(int)sizeof(match_input_packet_t)||!g_input_mutex_ready)return false;
    match_input_packet_t p;memcpy(&p,data,sizeof(p));
    if(p.h.magic!=LF2_NET_MAGIC||p.h.proto!=LF2_ADHOC_PROTO_VERSION||p.h.type!=PKT_MATCH_INPUT||p.h.size!=sizeof(p)||len<(int)p.h.size||!match_input_packet_valid(&p))return false;
    bool accepted=false;
    sceKernelLockLwMutex(&g_input_mutex,1,NULL);
    if(g_native_match_active&&g_native_have_peer&&p.match_id==g_native_match_id&&addr->s_addr==g_native_peer_addr.s_addr){
        cache_match_input_locked(&p);g_native_fast_rx++;accepted=true;
    }
    sceKernelUnlockLwMutex(&g_input_mutex,1);
    return accepted;
}

static bool take_match_input(uint32_t match_id,uint32_t frame,uint32_t *held){
    bool ok=false;
    if(g_input_mutex_ready)sceKernelLockLwMutex(&g_input_mutex,1,NULL);
    match_input_cache_t *c=&g_match_inputs[frame%LF2_MATCH_CACHE_CAP];
    if(c->valid&&c->match_id==match_id&&c->frame==frame){if(held)*held=c->held;c->valid=false;ok=true;}
    if(g_input_mutex_ready)sceKernelUnlockLwMutex(&g_input_mutex,1);
    if(ok)native_compare_state_if_ready();
    return ok;
}

static void process_packet(const queued_event_t *e){
    if(e->len<(int)sizeof(packet_hdr_t))return;packet_hdr_t h;memcpy(&h,e->data,sizeof(h));if(h.magic!=LF2_NET_MAGIC||h.proto!=LF2_ADHOC_PROTO_VERSION||h.size>e->len)return;
    if(h.type==PKT_LOBBY&&!g_host&&e->len>=(int)sizeof(lobby_packet_t)){lobby_packet_t p;memcpy(&p,e->data,sizeof(p));g_lobby.active=true;g_lobby.connected=true;g_lobby.host=false;g_joining=false;g_lobby.settings=p.settings;g_lobby.revision=p.revision;memcpy(g_lobby.players,p.players,sizeof(g_lobby.players));g_lobby.local_slot=-1;for(int i=0;i<LF2_ADHOC_MAX_PLAYERS;i++)if(g_lobby.players[i].present&&g_lobby.players[i].peer_id==g_local_peer_id){g_lobby.local_slot=i;break;}if(g_lobby.local_slot>0){lf2_net_player_t *me=&g_lobby.players[g_lobby.local_slot];g_local_ready=me->ready;}if(g_lobby.revision!=g_last_logged_revision){lf2_logf("NET","lobby state rev=%u local_slot=%d mode=%s players_max=%u stage=%u difficulty=%u status=%s",g_lobby.revision,g_lobby.local_slot,lf2_net_mode_name(g_lobby.settings.mode),g_lobby.settings.max_players,g_lobby.settings.stage,g_lobby.settings.difficulty,lf2_net_status_name(g_lobby.settings.status));g_last_logged_revision=g_lobby.revision;}return;}
    if(h.type==PKT_PLAYER&&g_host&&e->len>=(int)sizeof(player_packet_t)){peer_t *peer=peer_find(e->addr.s_addr);if(!peer)return;player_packet_t p;memcpy(&p,e->data,sizeof(p));lf2_net_player_t *pl=&g_lobby.players[peer->slot];if(pl->character!=p.character||pl->partner_character!=p.partner_character||pl->battle_follower!=p.battle_follower||pl->battle_count!=p.battle_count||pl->ready!=p.ready){pl->character=p.character;pl->partner_character=p.partner_character;pl->battle_follower=(p.battle_follower>=10&&p.battle_follower<=17)?p.battle_follower:10;pl->battle_count=p.battle_count>3?3:p.battle_count;pl->ready=p.ready;g_lobby.revision++;lf2_logf("NET","player update slot=%d name=%s character=%u partner=%u battle=%ux%u ready=%u rev=%u",peer->slot,pl->name,pl->character,pl->partner_character,pl->battle_count,pl->battle_follower,pl->ready,g_lobby.revision);broadcast_lobby();}return;}
    if(h.type==PKT_PING&&g_host&&e->len>=(int)sizeof(ping_packet_t)){ping_packet_t p;memcpy(&p,e->data,sizeof(p));p.h.type=PKT_PONG;p.h.seq=g_seq++;send_to(&e->addr,&p,sizeof(p));return;}
    if(h.type==PKT_PONG&&!g_host&&e->len>=(int)sizeof(ping_packet_t)){ping_packet_t p;memcpy(&p,e->data,sizeof(p));uint64_t d=now_us()-p.stamp_us;g_lobby.host_ping_ms=(uint16_t)(d/1000>65534?65534:d/1000);return;}
    if(h.type==PKT_MATCH_INPUT&&e->len>=(int)sizeof(match_input_packet_t)){match_input_packet_t p;memcpy(&p,e->data,sizeof(p));cache_match_input(&p);return;}
}

void lf2_adhoc_browser_update(void){
    queued_event_t e;while(pop_event(&e)){
        if(e.event==MATCH_HELLO)browser_hello(&e);
        else if(e.event==MATCH_ACCEPT&&g_probe_index>=0&&e.addr.s_addr==g_probe_addr.s_addr){/* wait for COMPLETE to measure the full matching RTT */}
        else if(e.event==MATCH_COMPLETE&&g_probe_index>=0&&e.addr.s_addr==g_probe_addr.s_addr){uint64_t d=now_us()-g_probe_started;uint16_t ms=(uint16_t)(d/1000>65534?65534:d/1000);int si=session_find(g_probe_addr.s_addr);if(si>=0){g_sessions[si].ping_ms=ms;lf2_logf("NET","browser ping host=%s rtt=%u ms bars=%d",g_sessions[si].host,(unsigned)ms,lf2_net_ping_bars(ms));}g_probe_ignore_addr=e.addr.s_addr;g_probe_ignore_until=now_us()+1500000ULL;sceNetAdhocMatchingCancelTarget(g_match,&g_probe_addr);g_probe_index=-1;}
        else if(g_probe_ignore_addr==e.addr.s_addr&&now_us()<g_probe_ignore_until&&(e.event==MATCH_LEFT||e.event==MATCH_CANCEL||e.event==MATCH_DISCONNECT||e.event==MATCH_COMPLETE||e.event==MATCH_ACCEPT)){/* finish transient probe without entering a lobby */}
        else if(g_joining&&(e.event==MATCH_ACCEPT||e.event==MATCH_COMPLETE)){g_lobby.active=true;g_lobby.connected=true;g_lobby.host=false;}
        else if(e.event==MATCH_REJECT||e.event==MATCH_TIMEOUT){if(g_probe_index>=0){int si=session_find(g_probe_addr.s_addr);if(si>=0)g_sessions[si].ping_ms=999;lf2_logf("NET","browser probe failed event=%d addr=%08X",e.event,(unsigned)e.addr.s_addr);g_probe_index=-1;}else if(g_joining){g_joining=false;g_lobby.connected=false;lf2_logf("NET","join failed event=%d addr=%08X",e.event,(unsigned)e.addr.s_addr);}}
        else if(e.event==MATCH_DATA)process_packet(&e);
    }
    uint64_t n=now_us();for(int i=0;i<g_session_count;){if(n-g_sessions[i].last_seen_us>5000000ULL){memmove(&g_sessions[i],&g_sessions[i+1],(g_session_count-i-1)*sizeof(g_sessions[0]));g_session_count--;if(g_probe_index==i)g_probe_index=-1;continue;}i++;}
}

int lf2_adhoc_join(int i,int character){
    if(!g_browser||i<0||i>=g_session_count)return -1;
    if(g_probe_index>=0){sceNetAdhocMatchingCancelTarget(g_match,&g_probe_addr);g_probe_index=-1;}
    join_t j;memset(&j,0,sizeof(j));j.magic=LF2_NET_MAGIC;j.proto=LF2_ADHOC_PROTO_VERSION;j.character=(uint8_t)character;j.partner_character=(uint8_t)((character+1)%10);j.battle_follower=10;j.battle_count=2;j.avatar_hash=g_avatar_hash;j.peer_id=g_local_peer_id;strncpy(j.name,g_name,sizeof(j.name)-1);strncpy(j.game_version,LF2_GAME_VERSION,sizeof(j.game_version)-1);strncpy(j.vita_version,LF2_VITA_VERSION,sizeof(j.vita_version)-1);
    SceNetInAddr a={g_sessions[i].address};int rc=sceNetAdhocMatchingSelectTarget(g_match,&a,sizeof(j),&j);
    if(rc>=0){memset(&g_lobby,0,sizeof(g_lobby));g_lobby.active=true;g_lobby.host=false;g_lobby.local_slot=-1;g_joining=true;g_join_host_addr=a;g_have_join_host=true;g_local_character=(uint8_t)character;g_local_partner=(uint8_t)((character+1)%10);g_local_battle_follower=10;g_local_battle_count=2;g_local_ready=0;g_last_logged_revision=0;lf2_logf("NET","join selected host=%s addr=%08X",g_sessions[i].host,g_sessions[i].address);}
    return rc;
}

int lf2_adhoc_host_start(const lf2_net_settings_t *settings,int character){int rc=lf2_adhoc_init();if(rc<0)return rc;g_host=true;memset(&g_lobby,0,sizeof(g_lobby));g_lobby.active=true;g_lobby.connected=true;g_lobby.host=true;g_lobby.local_slot=0;g_lobby.settings=*settings;if(g_lobby.settings.max_players<2)g_lobby.settings.max_players=2;if(g_lobby.settings.max_players>LF2_ADHOC_MAX_PLAYERS)g_lobby.settings.max_players=LF2_ADHOC_MAX_PLAYERS;lf2_net_player_t *p=&g_lobby.players[0];p->present=1;p->character=character;p->partner_character=(uint8_t)((character+1)%10);p->battle_follower=10;p->battle_count=2;p->team=1;p->ready=0;p->avatar_hash=g_avatar_hash;p->peer_id=g_local_peer_id;p->ping_ms=0;strncpy(p->name,g_name,sizeof(p->name)-1);g_local_character=(uint8_t)character;g_local_partner=(uint8_t)((character+1)%10);g_local_battle_follower=10;g_local_battle_count=2;g_local_ready=0;g_lobby.revision=1;g_last_logged_revision=1;hello_t h=make_hello();rc=start_matching(SCE_NET_ADHOC_MATCHING_MODE_PARENT,&h,sizeof(h));if(rc<0){memset(&g_lobby,0,sizeof(g_lobby));g_host=false;return rc;}g_host=true;update_hello();lf2_log_stage("network:adhoc_host","name=%s max=%d mode=%s stage=%u difficulty=%u",g_name,g_lobby.settings.max_players,lf2_net_mode_name(g_lobby.settings.mode),g_lobby.settings.stage,g_lobby.settings.difficulty);return 0;}

static void send_player_update(void){
    if(g_host||g_match<0||!g_have_join_host)return;
    player_packet_t p;memset(&p,0,sizeof(p));p.h.magic=LF2_NET_MAGIC;p.h.proto=LF2_ADHOC_PROTO_VERSION;p.h.type=PKT_PLAYER;p.h.size=sizeof(p);p.h.seq=g_seq++;p.character=g_local_character;p.partner_character=g_local_partner;p.battle_follower=g_local_battle_follower;p.battle_count=g_local_battle_count;p.ready=g_local_ready;
    send_to(&g_join_host_addr,&p,sizeof(p));
}
