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
void lf2_adhoc_leave(void){lf2_log_stage("network:adhoc_leave","host=%d connected=%d",g_host?1:0,g_lobby.connected?1:0);if(g_match>=0)clear_match();memset(&g_lobby,0,sizeof(g_lobby));g_host=false;g_browser=false;g_joining=false;g_local_ready=0;g_last_logged_revision=0;}
