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
static int local_lobby_slot(void){
    int slot=g_host?0:g_lobby.local_slot;
    if(!g_host&&slot<0)for(int i=1;i<LF2_ADHOC_MAX_PLAYERS;i++)if(g_lobby.players[i].present&&g_lobby.players[i].peer_id==g_local_peer_id){slot=i;g_lobby.local_slot=i;break;}
    return slot;
}
static void local_choice_changed(void){g_local_ready=0;int slot=local_lobby_slot();if(slot>=0&&slot<LF2_ADHOC_MAX_PLAYERS)g_lobby.players[slot].ready=0;}
int lf2_adhoc_lobby_set_character(int c){
    if(!g_lobby.active)return -1;if(c<0)c=0;if(c>255)c=255;g_local_character=(uint8_t)c;local_choice_changed();
    if(g_host){g_lobby.players[0].character=(uint8_t)c;g_lobby.revision++;lf2_logf("NET","host character=%d ready=0 rev=%u",c,g_lobby.revision);broadcast_lobby();}
    else {int slot=local_lobby_slot();if(slot>=0)g_lobby.players[slot].character=(uint8_t)c;send_player_update();}return 0;
}
int lf2_adhoc_lobby_set_partner(int c){
    if(!g_lobby.active)return -1;if(c<0)c=0;if(c>255)c=255;g_local_partner=(uint8_t)c;local_choice_changed();
    if(g_host){g_lobby.players[0].partner_character=(uint8_t)c;g_lobby.revision++;lf2_logf("NET","host partner=%d ready=0 rev=%u",c,g_lobby.revision);broadcast_lobby();}
    else {int slot=local_lobby_slot();if(slot>=0)g_lobby.players[slot].partner_character=(uint8_t)c;send_player_update();}return 0;
}
int lf2_adhoc_lobby_set_battle_follower(int c){
    if(!g_lobby.active)return -1;if(c<10)c=10;if(c>17)c=17;g_local_battle_follower=(uint8_t)c;local_choice_changed();
    if(g_host){g_lobby.players[0].battle_follower=(uint8_t)c;g_lobby.revision++;lf2_logf("NET","host battle follower=%d ready=0 rev=%u",c,g_lobby.revision);broadcast_lobby();}
    else {int slot=local_lobby_slot();if(slot>=0)g_lobby.players[slot].battle_follower=(uint8_t)c;send_player_update();}return 0;
}
int lf2_adhoc_lobby_set_battle_count(int count){
    if(!g_lobby.active)return -1;if(count<0)count=0;if(count>3)count=3;g_local_battle_count=(uint8_t)count;local_choice_changed();
    if(g_host){g_lobby.players[0].battle_count=(uint8_t)count;g_lobby.revision++;lf2_logf("NET","host battle count=%d ready=0 rev=%u",count,g_lobby.revision);broadcast_lobby();}
    else {int slot=local_lobby_slot();if(slot>=0)g_lobby.players[slot].battle_count=(uint8_t)count;send_player_update();}return 0;
}
int lf2_adhoc_lobby_set_ready(bool r){if(!g_lobby.active)return -1;g_local_ready=r?1:0;int slot=local_lobby_slot();if(slot>=0)g_lobby.players[slot].ready=g_local_ready;if(g_host){g_lobby.revision++;lf2_logf("NET","host ready=%u rev=%u",g_local_ready,g_lobby.revision);broadcast_lobby();}else send_player_update();return 0;}
int lf2_adhoc_lobby_host_settings(const lf2_net_settings_t *s){if(!g_host||!g_lobby.active)return -1;g_lobby.settings=*s;if(g_lobby.settings.max_players<2)g_lobby.settings.max_players=2;if(g_lobby.settings.max_players>LF2_ADHOC_MAX_PLAYERS)g_lobby.settings.max_players=LF2_ADHOC_MAX_PLAYERS;if(g_lobby.settings.battle_follower<10||g_lobby.settings.battle_follower>17)g_lobby.settings.battle_follower=10;if(g_lobby.settings.battle_count>3)g_lobby.settings.battle_count=3;g_local_ready=0;for(int i=0;i<LF2_ADHOC_MAX_PLAYERS;i++)if(g_lobby.players[i].present)g_lobby.players[i].ready=0;g_lobby.revision++;lf2_logf("NET","host settings rev=%u mode=%s max=%u stage=%u difficulty=%u ready_reset=1",g_lobby.revision,lf2_net_mode_name(g_lobby.settings.mode),g_lobby.settings.max_players,g_lobby.settings.stage,g_lobby.settings.difficulty);broadcast_lobby();return 0;}
int lf2_adhoc_lobby_host_status(lf2_net_status_t st){if(!g_host)return -1;g_lobby.settings.status=(uint8_t)st;g_lobby.revision++;lf2_logf("NET","host status=%s rev=%u",lf2_net_status_name(st),g_lobby.revision);broadcast_lobby();return 0;}
void lf2_adhoc_leave(void){lf2_log_stage("network:adhoc_leave","host=%d connected=%d",g_host?1:0,g_lobby.connected?1:0);lf2_adhoc_match_end();if(g_match>=0)clear_match();memset(&g_lobby,0,sizeof(g_lobby));g_host=false;g_browser=false;g_joining=false;g_local_ready=0;g_last_logged_revision=0;}

static int native_active_guest_slot(void){for(int i=1;i<LF2_ADHOC_MAX_PLAYERS;i++)if(g_lobby.players[i].present)return i;return -1;}

static bool native_match_peer_addr(SceNetInAddr *out){
    if(!out||g_match<0||!g_lobby.connected)return false;
    if(!g_host){if(!g_have_join_host)return false;*out=g_join_host_addr;return true;}
    int active_slot=native_active_guest_slot();if(active_slot<1)return false;
    for(int i=0;i<LF2_ADHOC_MAX_PLAYERS;i++)if(g_peers[i].used&&g_peers[i].slot==active_slot){*out=g_peers[i].addr;return true;}
    return false;
}

static bool native_event_is_active_peer(const queued_event_t *e){
    if(!e||!g_native_have_peer)return false;
    return e->addr.s_addr==g_native_peer_addr.s_addr;
}

static void native_match_deactivate(void){
    if(g_input_mutex_ready)sceKernelLockLwMutex(&g_input_mutex,1,NULL);
    g_native_match_active=false;g_native_have_peer=false;memset(&g_native_peer_addr,0,sizeof(g_native_peer_addr));
    if(g_input_mutex_ready)sceKernelUnlockLwMutex(&g_input_mutex,1);
}

static bool native_match_service_event(const queued_event_t *e){
    if(!e)return true;
    if(e->event==MATCH_DATA){process_packet(e);return true;}
    /* DATA_TIMEOUT describes one MatchingSendData attempt, not the peer
       connection. The lockstep packet has its own resend/deadline policy. */
    if(e->event==MATCH_DATA_TIMEOUT){if(native_event_is_active_peer(e))g_native_data_timeouts++;return true;}
    if(e->event==MATCH_LEFT||e->event==MATCH_DISCONNECT||e->event==MATCH_TIMEOUT||e->event==MATCH_REJECT||e->event==MATCH_CANCEL||e->event==MATCH_ERROR){
        if(!native_event_is_active_peer(e)){
            if(g_host)host_remove(e->addr.s_addr);
            lf2_logf("NET","spectator peer event ignored during match event=%d addr=%08X",e->event,(unsigned)e->addr.s_addr);
            return true;
        }
        if(!g_host)g_lobby.connected=false;native_match_deactivate();
        lf2_logf("NET","native match peer ended event=%d addr=%08X",e->event,(unsigned)e->addr.s_addr);
        return false;
    }
    return true;
}

int lf2_adhoc_match_begin(uint32_t match_id){
    SceNetInAddr peer;
    if(!match_id||!native_match_peer_addr(&peer)){lf2_logf("NET","native match begin rejected id=%u host=%d connected=%d",(unsigned)match_id,g_host?1:0,g_lobby.connected?1:0);return -1;}
    /* Reset main-thread diagnostics before making the new match visible to the
       callback worker. This prevents a just-arrived frame-0 packet from being
       counted/cached and then partially reset by the game thread. */
    g_native_match_frame=0;g_native_prev_physical=0;g_native_wait_total_us=0;g_native_wait_max_us=0;g_native_wait_samples=0;g_native_buffer_hits=0;
    g_native_retransmits=0;g_native_data_timeouts=0;g_native_soft_drops=0;g_native_fast_rx=0;g_native_start_refreshes=0;g_native_peer_highest=0;g_native_peer_ack=0;g_native_peer_highest_valid=false;g_native_peer_ack_valid=false;g_native_prefetch_deferred=0;g_native_ack_suppressed=0;g_native_barrier_ack_waits=0;g_native_state_tu=0;g_native_state_digest=0;g_native_desyncs=0;g_native_desync=false;memset(g_local_inputs,0,sizeof(g_local_inputs));
    /* Keep already queued frame 0 for this match. The joining Vita may have
       drained STARTING and the host's first input in one lobby update before
       entering the game runner. Only discard packets from older matches. */
    if(g_input_mutex_ready)sceKernelLockLwMutex(&g_input_mutex,1,NULL);
    for(unsigned i=0;i<sizeof(g_match_inputs)/sizeof(g_match_inputs[0]);i++){
        if(g_match_inputs[i].valid&&g_match_inputs[i].match_id!=match_id)g_match_inputs[i].valid=false;
        else if(g_match_inputs[i].valid&&g_match_inputs[i].match_id==match_id&&(!g_native_peer_highest_valid||g_match_inputs[i].frame>g_native_peer_highest)){
            g_native_peer_highest=g_match_inputs[i].frame;g_native_peer_highest_valid=true;
        }
    }
    g_native_peer_state_tu=0;g_native_peer_state_digest=0;
    g_native_match_id=match_id;g_native_peer_addr=peer;g_native_have_peer=true;g_native_match_active=true;
    if(g_input_mutex_ready)sceKernelUnlockLwMutex(&g_input_mutex,1);
    cache_input_one(g_local_inputs,match_id,0,0);
    lf2_log_stage("network:adhoc_match_begin","id=%u role=%s peer=%08X input_delay=1 window=%d barrier=1 resend_ms=8 timeout_ms=1500 fast_rx=callback",(unsigned)match_id,g_host?"host":"client",(unsigned)peer.s_addr,LF2_MATCH_INPUT_WINDOW);
    lf2_log_set_network_fast_path(1);
    return 0;
}

bool lf2_adhoc_lockstep_frame(void *userdata,uint32_t local_held[4],uint32_t remote_held[4]){
    (void)userdata;
    if(g_native_desync||!local_held||!remote_held)return false;
    SceNetInAddr peer;bool active=false;
    if(g_input_mutex_ready)sceKernelLockLwMutex(&g_input_mutex,1,NULL);
    if(g_native_match_active&&g_native_have_peer){peer=g_native_peer_addr;active=true;}
    if(g_input_mutex_ready)sceKernelUnlockLwMutex(&g_input_mutex,1);
    if(!active)return false;

    const uint32_t frame=g_native_match_frame;
    const uint32_t physical_local=local_held[0];
    const uint32_t send_frame=frame+1u;
    uint32_t scheduled_local=0;
    if(!peek_input_one(g_local_inputs,g_native_match_id,frame,&scheduled_local))scheduled_local=(frame==0)?0u:g_native_prev_physical;
    cache_input_one(g_local_inputs,g_native_match_id,send_frame,physical_local);

    match_input_packet_t p;memset(&p,0,sizeof(p));
    p.h.magic=LF2_NET_MAGIC;p.h.proto=LF2_ADHOC_PROTO_VERSION;p.h.type=PKT_MATCH_INPUT;p.h.size=sizeof(p);
    p.match_id=g_native_match_id;p.newest_frame=send_frame;p.first_frame=(send_frame+1u>LF2_MATCH_INPUT_WINDOW)?send_frame-(LF2_MATCH_INPUT_WINDOW-1u):0u;
    p.count=(uint8_t)(p.newest_frame-p.first_frame+1u);p.ack_frame=native_ack_frame_snapshot();p.state_tu=g_native_state_tu;p.state_digest=g_native_state_digest;
    for(uint32_t i=0;i<(uint32_t)p.count;i++){
        uint32_t h=0;peek_input_one(g_local_inputs,g_native_match_id,p.first_frame+i,&h);p.held[i]=h;
    }

    uint32_t peer_held=0;bool got=false;bool sent=false;bool peer_acked_next=false;
    uint64_t wait_start=now_us(),last_send=0,last_start_refresh=0;uint32_t frame_resends=0;
    /* Send the next-TU input before servicing non-critical lobby/system events.
       This is a prefetch for the NEXT simulation step. After frame 0, failure
       to enqueue this prefetch must not stall a CURRENT step whose peer input
       is already buffered: the next packet still carries an 8-frame history. */
    p.ack_frame=native_ack_frame_snapshot();p.h.seq=g_seq++;int initial_rc=send_to(&peer,&p,sizeof(p));
    if(initial_rc>=0){sent=true;last_send=now_us();}
    else if(initial_rc==SCE_NET_ADHOC_MATCHING_ERROR_DATA_BUSY){g_native_prefetch_deferred++;}
    else {native_match_deactivate();return false;}
    queued_event_t e;while(pop_event(&e)){if(!native_match_service_event(&e))return false;}
    if(take_match_input(g_native_match_id,frame,&peer_held)){got=true;g_native_buffer_hits++;}
    peer_acked_next=native_peer_acked(send_frame);

    const uint64_t resend_us=(frame==0)?LF2_NATIVE_BARRIER_RESEND_US:LF2_NATIVE_RESEND_US;
    const uint64_t timeout_us=(frame==0)?LF2_NATIVE_BARRIER_TIMEOUT_US:LF2_NATIVE_FRAME_TIMEOUT_US;
    const uint64_t deadline=wait_start+timeout_us;
    while(now_us()<deadline){
        /* Frame 0 is a true two-way barrier: besides receiving the peer's
           neutral frame, wait until the peer explicitly ACKs our frame-1
           prefetch. Later frames block only on the input required NOW. */
        if(got&&(frame!=0||peer_acked_next))break;
        uint64_t n=now_us();
        if(frame==0&&g_host&&(!last_start_refresh||n-last_start_refresh>=100000ULL)){
            /* STARTING is part of the frame-0 reliability contract. If the
               original lobby transition was lost, this refresh pulls the
               client out of the lobby while the host is still at the barrier. */
            send_lobby_to(&peer);last_start_refresh=n;g_native_start_refreshes++;
        }
        if(!peer_acked_next&&(!sent||n-last_send>=resend_us)){
            p.ack_frame=native_ack_frame_snapshot();p.h.seq=g_seq++;
            int rc=send_to(&peer,&p,sizeof(p));
            if(rc>=0){if(sent){frame_resends++;g_native_retransmits++;}sent=true;last_send=n;}
            else if(rc==SCE_NET_ADHOC_MATCHING_ERROR_DATA_BUSY){g_native_prefetch_deferred++;}
            else {native_match_deactivate();return false;}
        }else if(peer_acked_next&&!got)g_native_ack_suppressed++;
        bool any=false;while(pop_event(&e)){any=true;if(!native_match_service_event(&e))return false;}
        if(!got&&take_match_input(g_native_match_id,frame,&peer_held))got=true;
        bool ack_now=native_peer_acked(send_frame);if(frame==0&&!peer_acked_next&&ack_now)g_native_barrier_ack_waits++;
        peer_acked_next=ack_now;
        if(!any){uint64_t elapsed=now_us()-wait_start;sceKernelDelayThread(frame==0?1000:(elapsed<2000ULL?100:300));}
    }
    bool complete=got&&(frame!=0||peer_acked_next);
    if(!complete){
        uint32_t peer_high=0,peer_ack=0;bool high_valid=false,ack_valid=false;
        if(g_input_mutex_ready)sceKernelLockLwMutex(&g_input_mutex,1,NULL);
        high_valid=g_native_peer_highest_valid;peer_high=g_native_peer_highest;ack_valid=g_native_peer_ack_valid;peer_ack=g_native_peer_ack;
        if(g_input_mutex_ready)sceKernelUnlockLwMutex(&g_input_mutex,1);
        lf2_logf("NET","native lockstep timeout id=%u frame=%u sent=%d got=%d ack_next=%d wait_us=%llu resends=%u peer_highest=%s%u peer_ack=%s%u data_timeouts=%u queue_drops=%u",
                 (unsigned)g_native_match_id,(unsigned)frame,sent?1:0,got?1:0,peer_acked_next?1:0,(unsigned long long)(now_us()-wait_start),
                 (unsigned)frame_resends,high_valid?"":"-",(unsigned)peer_high,ack_valid?"":"-",(unsigned)peer_ack,(unsigned)g_native_data_timeouts,(unsigned)g_event_drops);
        native_match_deactivate();return false;
    }

    uint64_t waited=now_us()-wait_start;g_native_wait_total_us+=waited;g_native_wait_samples++;if(waited>g_native_wait_max_us)g_native_wait_max_us=waited;
    memset(remote_held,0,sizeof(uint32_t)*4);
    if(g_host){local_held[0]=scheduled_local;remote_held[0]=peer_held;}
    else {local_held[0]=peer_held;remote_held[0]=scheduled_local;}
    g_native_prev_physical=physical_local;
    g_native_match_frame++;
    if(frame==0||(frame%300u)==0u){
        uint64_t avg=g_native_wait_samples?g_native_wait_total_us/g_native_wait_samples:0;
        lf2_logf("NET","native lockstep id=%u frame=%u hostmask=%08X clientmask=%08X wait_avg_us=%llu wait_max_us=%llu buffered=%u/%u frame_resends=%u total_resends=%u deferred=%u ack_suppressed=%u barrier_ack=%u data_timeouts=%u fast_rx=%u start_refresh=%u queue=%u/%u",
                 (unsigned)g_native_match_id,(unsigned)frame,(unsigned)local_held[0],(unsigned)remote_held[0],
                 (unsigned long long)avg,(unsigned long long)g_native_wait_max_us,(unsigned)g_native_buffer_hits,(unsigned)g_native_wait_samples,
                 (unsigned)frame_resends,(unsigned)g_native_retransmits,(unsigned)g_native_prefetch_deferred,(unsigned)g_native_ack_suppressed,(unsigned)g_native_barrier_ack_waits,(unsigned)g_native_data_timeouts,(unsigned)g_native_fast_rx,(unsigned)g_native_start_refreshes,(unsigned)g_event_peak,(unsigned)g_event_drops);
    }
    return true;
}

void lf2_adhoc_match_report_state(void *userdata,uint32_t tu,uint32_t digest){
    (void)userdata;if(!g_native_match_active||!tu)return;
    if(tu>=g_native_state_tu){g_native_state_tu=tu;g_native_state_digest=digest;native_compare_state_if_ready();}
}

void lf2_adhoc_match_end(void){
    uint64_t avg=g_native_wait_samples?g_native_wait_total_us/g_native_wait_samples:0;
    if(g_native_match_id)lf2_log_stage("network:adhoc_match_end","id=%u frames=%u role=%s wait_avg_us=%llu wait_max_us=%llu buffered=%u/%u retransmits=%u deferred=%u ack_suppressed=%u barrier_ack=%u data_timeouts=%u fast_rx=%u start_refresh=%u desyncs=%u queue_peak=%u queue_drops=%u",
        (unsigned)g_native_match_id,(unsigned)g_native_match_frame,g_host?"host":"client",(unsigned long long)avg,(unsigned long long)g_native_wait_max_us,
        (unsigned)g_native_buffer_hits,(unsigned)g_native_wait_samples,(unsigned)g_native_retransmits,(unsigned)g_native_prefetch_deferred,(unsigned)g_native_ack_suppressed,(unsigned)g_native_barrier_ack_waits,(unsigned)g_native_data_timeouts,(unsigned)g_native_fast_rx,(unsigned)g_native_start_refreshes,(unsigned)g_native_desyncs,(unsigned)g_event_peak,(unsigned)g_event_drops);
    g_native_match_frame=0;g_native_prev_physical=0;
    if(g_input_mutex_ready)sceKernelLockLwMutex(&g_input_mutex,1,NULL);
    g_native_match_active=false;g_native_have_peer=false;memset(&g_native_peer_addr,0,sizeof(g_native_peer_addr));g_native_match_id=0;
    memset(g_match_inputs,0,sizeof(g_match_inputs));g_native_peer_highest_valid=false;g_native_peer_ack_valid=false;g_native_peer_state_tu=0;g_native_peer_state_digest=0;
    if(g_input_mutex_ready)sceKernelUnlockLwMutex(&g_input_mutex,1);
    memset(g_local_inputs,0,sizeof(g_local_inputs));
    lf2_log_set_network_fast_path(0);
}