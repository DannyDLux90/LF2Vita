void lf2_adhoc_lobby_update(void){
    queued_event_t e;while(pop_event(&e)){
        if(g_host&&e.event==MATCH_JOIN)host_join_request(&e);
        else if(g_host&&e.event==MATCH_COMPLETE)host_complete(&e);
        else if(g_host&&(e.event==MATCH_LEFT||e.event==MATCH_DISCONNECT||e.event==MATCH_TIMEOUT))host_remove(e.addr.s_addr);
        else if(!g_host&&g_joining&&(e.event==MATCH_ACCEPT||e.event==MATCH_COMPLETE)){g_lobby.active=true;g_lobby.connected=true;}
        else if(!g_host&&(e.event==MATCH_LEFT||e.event==MATCH_DISCONNECT||e.event==MATCH_TIMEOUT)){g_lobby.connected=false;g_joining=false;lf2_logf("NET","host connection ended event=%d addr=%08X",e.event,(unsigned)e.addr.s_addr);}
        else if(e.event==MATCH_DATA)process_packet(&e);
        else if(e.event==MATCH_DATA_TIMEOUT){/* lobby packets are periodically refreshed below */}
    }
    uint64_t n=now_us();
    /* MatchingSendData can legitimately report DATA_BUSY. Periodic state
       refresh makes lobby START/ready/character changes eventually reliable
       without increasing the lobby revision for duplicate client updates. */
    if(g_host&&g_lobby.connected&&n-g_last_lobby_broadcast>=250000ULL){broadcast_lobby();g_last_lobby_broadcast=n;}
    if(!g_host&&g_lobby.connected&&g_have_join_host&&n-g_last_player_refresh>=250000ULL){send_player_update();g_last_player_refresh=n;}
    if(!g_host&&g_lobby.connected&&g_have_join_host&&n-g_last_ping>1000000ULL){ping_packet_t p;memset(&p,0,sizeof(p));p.h.magic=LF2_NET_MAGIC;p.h.proto=LF2_ADHOC_PROTO_VERSION;p.h.type=PKT_PING;p.h.size=sizeof(p);p.h.seq=g_seq++;p.stamp_us=n;send_to(&g_join_host_addr,&p,sizeof(p));g_last_ping=n;}
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
    /* DATA_TIMEOUT describes one MatchingSendData attempt, not the peer
       connection. The lockstep packet has its own resend/deadline policy. */
    if(e->event==MATCH_DATA_TIMEOUT){g_native_data_timeouts++;return true;}
    if(e->event==MATCH_LEFT||e->event==MATCH_DISCONNECT||e->event==MATCH_TIMEOUT||e->event==MATCH_REJECT||e->event==MATCH_CANCEL||e->event==MATCH_ERROR){
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
    g_native_retransmits=0;g_native_data_timeouts=0;g_native_soft_drops=0;g_native_peer_highest=0;g_native_state_tu=0;g_native_state_digest=0;g_native_peer_state_tu=0;g_native_peer_state_digest=0;g_native_desyncs=0;g_native_desync=false;memset(g_local_inputs,0,sizeof(g_local_inputs));
    cache_input_one(g_local_inputs,match_id,0,0);
    lf2_log_stage("network:adhoc_match_begin","id=%u role=%s peer=%08X input_delay=1 window=%d barrier=1 resend_ms=8 timeout_ms=1500",(unsigned)match_id,g_host?"host":"client",(unsigned)peer.s_addr,LF2_MATCH_INPUT_WINDOW);
    lf2_log_set_network_fast_path(1);
    return 0;
}

bool lf2_adhoc_lockstep_frame(void *userdata,uint32_t local_held[4],uint32_t remote_held[4]){
    (void)userdata;
    if(!g_native_match_active||g_native_desync||!local_held||!remote_held)return false;
    SceNetInAddr peer;if(!native_match_peer_addr(&peer)){g_native_match_active=false;return false;}

    const uint32_t frame=g_native_match_frame;
    const uint32_t physical_local=local_held[0];
    const uint32_t send_frame=frame+1u;
    uint32_t scheduled_local=0;
    if(!peek_input_one(g_local_inputs,g_native_match_id,frame,&scheduled_local))scheduled_local=(frame==0)?0u:g_native_prev_physical;
    cache_input_one(g_local_inputs,g_native_match_id,send_frame,physical_local);

    match_input_packet_t p;memset(&p,0,sizeof(p));
    p.h.magic=LF2_NET_MAGIC;p.h.proto=LF2_ADHOC_PROTO_VERSION;p.h.type=PKT_MATCH_INPUT;p.h.size=sizeof(p);
    p.match_id=g_native_match_id;p.newest_frame=send_frame;p.first_frame=(send_frame+1u>LF2_MATCH_INPUT_WINDOW)?send_frame-(LF2_MATCH_INPUT_WINDOW-1u):0u;
    p.count=(uint8_t)(p.newest_frame-p.first_frame+1u);p.ack_frame=frame?frame-1u:0xffffffffu;p.state_tu=g_native_state_tu;p.state_digest=g_native_state_digest;
    for(uint32_t i=0;i<(uint32_t)p.count;i++){
        uint32_t h=0;peek_input_one(g_local_inputs,g_native_match_id,p.first_frame+i,&h);p.held[i]=h;
    }

    uint32_t peer_held=0;bool got=false;bool sent=false;
    uint64_t wait_start=now_us(),last_send=0;uint32_t frame_resends=0;
    queued_event_t e;while(pop_event(&e)){if(!native_match_service_event(&e))return false;}
    if(take_match_input(g_native_match_id,frame,&peer_held)){got=true;g_native_buffer_hits++;}

    const uint64_t resend_us=(frame==0)?LF2_NATIVE_BARRIER_RESEND_US:LF2_NATIVE_RESEND_US;
    const uint64_t timeout_us=(frame==0)?LF2_NATIVE_BARRIER_TIMEOUT_US:LF2_NATIVE_FRAME_TIMEOUT_US;
    const uint64_t deadline=wait_start+timeout_us;
    while(now_us()<deadline){
        uint64_t n=now_us();
        if(!sent||(!got&&n-last_send>=resend_us)){
            p.h.seq=g_seq++;
            int rc=send_to(&peer,&p,sizeof(p));
            if(rc>=0){if(sent){frame_resends++;g_native_retransmits++;}sent=true;last_send=n;}
            else if(rc!=SCE_NET_ADHOC_MATCHING_ERROR_DATA_BUSY){g_native_match_active=false;return false;}
        }
        bool any=false;while(pop_event(&e)){any=true;if(!native_match_service_event(&e))return false;}
        if(!got&&take_match_input(g_native_match_id,frame,&peer_held))got=true;
        if(sent&&got)break;
        if(!any)sceKernelDelayThread(frame==0?1000:300);
    }
    if(!sent||!got){
        lf2_logf("NET","native lockstep timeout id=%u frame=%u sent=%d got=%d wait_us=%llu resends=%u peer_highest=%u data_timeouts=%u queue_drops=%u",
                 (unsigned)g_native_match_id,(unsigned)frame,sent?1:0,got?1:0,(unsigned long long)(now_us()-wait_start),
                 (unsigned)frame_resends,(unsigned)g_native_peer_highest,(unsigned)g_native_data_timeouts,(unsigned)g_event_drops);
        g_native_match_active=false;return false;
    }

    uint64_t waited=now_us()-wait_start;g_native_wait_total_us+=waited;g_native_wait_samples++;if(waited>g_native_wait_max_us)g_native_wait_max_us=waited;
    memset(remote_held,0,sizeof(uint32_t)*4);
    if(g_host){local_held[0]=scheduled_local;remote_held[0]=peer_held;}
    else {local_held[0]=peer_held;remote_held[0]=scheduled_local;}
    g_native_prev_physical=physical_local;
    g_native_match_frame++;
    if(frame==0||(frame%300u)==0u){
        uint64_t avg=g_native_wait_samples?g_native_wait_total_us/g_native_wait_samples:0;
        lf2_logf("NET","native lockstep id=%u frame=%u hostmask=%08X clientmask=%08X wait_avg_us=%llu wait_max_us=%llu buffered=%u/%u frame_resends=%u total_resends=%u data_timeouts=%u queue=%u/%u",
                 (unsigned)g_native_match_id,(unsigned)frame,(unsigned)local_held[0],(unsigned)remote_held[0],
                 (unsigned long long)avg,(unsigned long long)g_native_wait_max_us,(unsigned)g_native_buffer_hits,(unsigned)g_native_wait_samples,
                 (unsigned)frame_resends,(unsigned)g_native_retransmits,(unsigned)g_native_data_timeouts,(unsigned)g_event_peak,(unsigned)g_event_drops);
    }
    return true;
}

void lf2_adhoc_match_report_state(void *userdata,uint32_t tu,uint32_t digest){
    (void)userdata;if(!g_native_match_active||!tu)return;
    if(tu>=g_native_state_tu){g_native_state_tu=tu;g_native_state_digest=digest;native_compare_state_if_ready();}
}

void lf2_adhoc_match_end(void){
    uint64_t avg=g_native_wait_samples?g_native_wait_total_us/g_native_wait_samples:0;
    if(g_native_match_id)lf2_log_stage("network:adhoc_match_end","id=%u frames=%u role=%s wait_avg_us=%llu wait_max_us=%llu buffered=%u/%u retransmits=%u data_timeouts=%u desyncs=%u queue_peak=%u queue_drops=%u",
        (unsigned)g_native_match_id,(unsigned)g_native_match_frame,g_host?"host":"client",(unsigned long long)avg,(unsigned long long)g_native_wait_max_us,
        (unsigned)g_native_buffer_hits,(unsigned)g_native_wait_samples,(unsigned)g_native_retransmits,(unsigned)g_native_data_timeouts,(unsigned)g_native_desyncs,(unsigned)g_event_peak,(unsigned)g_event_drops);
    g_native_match_active=false;g_native_match_id=0;g_native_match_frame=0;g_native_prev_physical=0;memset(g_match_inputs,0,sizeof(g_match_inputs));memset(g_local_inputs,0,sizeof(g_local_inputs));
    lf2_log_set_network_fast_path(0);
}
