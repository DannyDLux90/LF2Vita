void lf2_adhoc_lobby_update(void){
    queued_event_t e;while(pop_event(&e)){
        if(g_host&&e.event==MATCH_JOIN)host_join_request(&e);
        else if(g_host&&e.event==MATCH_COMPLETE)host_complete(&e);
        else if(g_host&&(e.event==MATCH_LEFT||e.event==MATCH_DISCONNECT||e.event==MATCH_TIMEOUT))host_remove(e.addr.s_addr);
        else if(!g_host&&g_joining&&(e.event==MATCH_ACCEPT||e.event==MATCH_COMPLETE)){g_lobby.active=true;g_lobby.connected=true;}
        else if(!g_host&&(e.event==MATCH_LEFT||e.event==MATCH_DISCONNECT||e.event==MATCH_TIMEOUT)){g_lobby.connected=false;g_joining=false;lf2_logf("NET","host connection ended event=%d addr=%08X",e.event,(unsigned)e.addr.s_addr);}
        else if(e.event==MATCH_DATA)process_packet(&e);
    }
    if(!g_host&&g_lobby.connected&&g_have_join_host&&now_us()-g_last_ping>1000000ULL){ping_packet_t p;memset(&p,0,sizeof(p));p.h.magic=LF2_NET_MAGIC;p.h.proto=LF2_ADHOC_PROTO_VERSION;p.h.type=PKT_PING;p.h.size=sizeof(p);p.h.seq=g_seq++;p.stamp_us=now_us();send_to(&g_join_host_addr,&p,sizeof(p));g_last_ping=now_us();}
}
const lf2_adhoc_lobby_t *lf2_adhoc_lobby(void){return &g_lobby;}
int lf2_adhoc_lobby_set_character(int c){if(!g_lobby.active)return -1;g_local_character=(uint8_t)c;if(g_host){g_lobby.players[0].character=(uint8_t)c;g_lobby.revision++;lf2_logf("NET","host character=%d rev=%u",c,g_lobby.revision);broadcast_lobby();}else{int s=g_lobby.local_slot;if(s<0)for(int i=1;i<8;i++)if(g_lobby.players[i].present&&g_lobby.players[i].peer_id==g_local_peer_id){s=i;g_lobby.local_slot=i;break;}if(s>=0)g_lobby.players[s].character=(uint8_t)c;send_player_update();}return 0;}
int lf2_adhoc_lobby_set_ready(bool r){if(!g_lobby.active)return -1;g_local_ready=r?1:0;if(g_host){g_lobby.players[0].ready=g_local_ready;g_lobby.revision++;lf2_logf("NET","host ready=%u rev=%u",g_local_ready,g_lobby.revision);broadcast_lobby();}else{int s=g_lobby.local_slot;if(s>=0)g_lobby.players[s].ready=g_local_ready;send_player_update();}return 0;}
int lf2_adhoc_lobby_host_settings(const lf2_net_settings_t *s){if(!g_host||!g_lobby.active)return -1;g_lobby.settings=*s;int present=0;for(int i=0;i<LF2_ADHOC_MAX_PLAYERS;i++)if(g_lobby.players[i].present)present++;if(g_lobby.settings.max_players<present)g_lobby.settings.max_players=(uint8_t)present;if(g_lobby.settings.max_players<2)g_lobby.settings.max_players=2;if(g_lobby.settings.max_players>8)g_lobby.settings.max_players=8;g_lobby.revision++;lf2_logf("NET","host settings rev=%u mode=%s max=%u stage=%u difficulty=%u",g_lobby.revision,lf2_net_mode_name(g_lobby.settings.mode),g_lobby.settings.max_players,g_lobby.settings.stage,g_lobby.settings.difficulty);broadcast_lobby();return 0;}
int lf2_adhoc_lobby_host_status(lf2_net_status_t st){if(!g_host)return -1;g_lobby.settings.status=(uint8_t)st;g_lobby.revision++;lf2_logf("NET","host status=%s rev=%u",lf2_net_status_name(st),g_lobby.revision);broadcast_lobby();return 0;}
void lf2_adhoc_leave(void){lf2_log_stage("network:adhoc_leave","host=%d connected=%d",g_host?1:0,g_lobby.connected?1:0);lf2_adhoc_match_end();if(g_match>=0)clear_match();memset(&g_lobby,0,sizeof(g_lobby));g_host=false;g_browser=false;g_joining=false;g_local_ready=0;g_last_logged_revision=0;}


static bool native_match_peer_addr(SceNetInAddr *out){
    if(!out||g_match<0||!g_lobby.connected)return false;
    if(!g_host){if(!g_have_join_host)return false;*out=g_join_host_addr;return true;}
    peer_t *only=NULL;
    for(int i=0;i<LF2_ADHOC_MAX_PLAYERS;i++)if(g_peers[i].used){if(only)return false;only=&g_peers[i];}
    if(!only)return false;*out=only->addr;return true;
}

static bool native_match_service_event(const queued_event_t *e){
    if(!e)return true;
    if(e->event==MATCH_DATA){process_packet(e);return true;}
    if(e->event==MATCH_LEFT||e->event==MATCH_DISCONNECT||e->event==MATCH_TIMEOUT||e->event==MATCH_REJECT||e->event==MATCH_CANCEL||e->event==MATCH_ERROR||e->event==MATCH_DATA_TIMEOUT){
        g_lobby.connected=false;g_native_match_active=false;
        lf2_logf("NET","native match peer ended event=%d addr=%08X",e->event,(unsigned)e->addr.s_addr);
        return false;
    }
    return true;
}

int lf2_adhoc_match_begin(uint32_t match_id){
    SceNetInAddr peer;
    if(!match_id||!native_match_peer_addr(&peer)){lf2_logf("NET","native match begin rejected id=%u host=%d connected=%d",(unsigned)match_id,g_host?1:0,g_lobby.connected?1:0);return -1;}
    /* Keep already queued frame 0 for this match. The joining Vita may have
       drained STARTING and the host's first input in one lobby update before
       entering the game runner. Only discard packets from older matches. */
    for(unsigned i=0;i<sizeof(g_match_inputs)/sizeof(g_match_inputs[0]);i++)
        if(g_match_inputs[i].valid&&g_match_inputs[i].match_id!=match_id)g_match_inputs[i].valid=false;
    g_native_match_id=match_id;g_native_match_frame=0;g_native_match_active=true;
    g_native_prev_physical=0;g_native_wait_total_us=0;g_native_wait_max_us=0;g_native_wait_samples=0;g_native_buffer_hits=0;
    lf2_log_stage("network:adhoc_match_begin","id=%u role=%s peer=%08X input_delay=1 redundancy=1",(unsigned)match_id,g_host?"host":"client",(unsigned)peer.s_addr);
    return 0;
}

bool lf2_adhoc_lockstep_frame(void *userdata,uint32_t local_held[4],uint32_t remote_held[4]){
    (void)userdata;
    if(!g_native_match_active||!local_held||!remote_held)return false;
    SceNetInAddr peer;if(!native_match_peer_addr(&peer)){g_native_match_active=false;return false;}

    /* One-TU pipelined lockstep. Input sampled now is tagged for frame+1.
       The current simulation frame therefore normally finds the peer input
       already buffered and avoids the small RTT-sized stall that used to
       happen on every 30 Hz tick. Frame 0 is a deterministic neutral warm-up. */
    const uint32_t frame=g_native_match_frame;
    const uint32_t physical_local=local_held[0];
    const uint32_t scheduled_local=(frame==0)?0u:g_native_prev_physical;
    const uint32_t send_frame=frame+1u;
    match_input_packet_t p;memset(&p,0,sizeof(p));
    p.h.magic=LF2_NET_MAGIC;p.h.proto=LF2_ADHOC_PROTO_VERSION;p.h.type=PKT_MATCH_INPUT;p.h.size=sizeof(p);p.h.seq=g_seq++;
    p.match_id=g_native_match_id;p.frame=send_frame;p.held=physical_local;
    p.prev_frame=(frame==0)?0xffffffffu:frame;p.prev_held=g_native_prev_physical;

    uint32_t peer_held=0;bool got=(frame==0);bool sent=false;
    uint64_t wait_start=now_us();
    if(frame>0){
        queued_event_t e;while(pop_event(&e)){if(!native_match_service_event(&e))return false;}
        if(take_match_input(g_native_match_id,frame,&peer_held)){got=true;g_native_buffer_hits++;}
    }

    /* 350 ms is intentionally far below the old five-second freeze. The
       redundant previous-frame field means one lost/delayed packet is repaired
       by the next packet while preserving deterministic lockstep. */
    uint64_t deadline=wait_start+350000ULL;
    while(now_us()<deadline){
        if(!sent){int rc=send_to(&peer,&p,sizeof(p));if(rc>=0)sent=true;else if(rc!=SCE_NET_ADHOC_MATCHING_ERROR_DATA_BUSY){g_native_match_active=false;return false;}}
        queued_event_t e;bool any=false;while(pop_event(&e)){any=true;if(!native_match_service_event(&e))return false;}
        if(!got&&frame>0&&take_match_input(g_native_match_id,frame,&peer_held))got=true;
        if(sent&&got)break;
        if(!any)sceKernelDelayThread(250);
    }
    if(!sent||!got){lf2_logf("NET","native lockstep timeout id=%u frame=%u sent=%d got=%d",(unsigned)g_native_match_id,(unsigned)frame,sent?1:0,got?1:0);g_native_match_active=false;return false;}

    uint64_t waited=now_us()-wait_start;g_native_wait_total_us+=waited;g_native_wait_samples++;if(waited>g_native_wait_max_us)g_native_wait_max_us=waited;
    memset(remote_held,0,sizeof(uint32_t)*4);
    if(g_host){local_held[0]=scheduled_local;remote_held[0]=peer_held;}
    else {local_held[0]=peer_held;remote_held[0]=scheduled_local;}
    g_native_prev_physical=physical_local;
    g_native_match_frame++;
    if(frame==0||(frame%300u)==0u){
        uint64_t avg=g_native_wait_samples?g_native_wait_total_us/g_native_wait_samples:0;
        lf2_logf("NET","native lockstep id=%u frame=%u hostmask=%08X clientmask=%08X wait_avg_us=%llu wait_max_us=%llu buffered=%u/%u",
                 (unsigned)g_native_match_id,(unsigned)frame,(unsigned)local_held[0],(unsigned)remote_held[0],
                 (unsigned long long)avg,(unsigned long long)g_native_wait_max_us,(unsigned)g_native_buffer_hits,(unsigned)g_native_wait_samples);
    }
    return true;
}

void lf2_adhoc_match_end(void){
    if(g_native_match_active)lf2_log_stage("network:adhoc_match_end","id=%u frames=%u role=%s",(unsigned)g_native_match_id,(unsigned)g_native_match_frame,g_host?"host":"client");
    g_native_match_active=false;g_native_match_id=0;g_native_match_frame=0;g_native_prev_physical=0;memset(g_match_inputs,0,sizeof(g_match_inputs));
}
