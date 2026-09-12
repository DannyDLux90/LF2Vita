static int net70_active_pair_chars(const lf2_adhoc_lobby_t *l,int *host_char,int *guest_char,int *guest_slot){
    if(!l||!l->connected||!l->players[0].present)return -1;
    int guest=-1;for(int i=1;i<LF2_ADHOC_MAX_PLAYERS;i++)if(l->players[i].present){guest=i;break;}
    if(guest<1)return -2;
    int hc=l->players[0].character,gc=l->players[guest].character;
    if(hc<0||hc>=lf2_roster_count||gc<0||gc>=lf2_roster_count)return -3;
    if(host_char)*host_char=hc;if(guest_char)*guest_char=gc;if(guest_slot)*guest_slot=guest;return 0;
}

static int net70_build_network_stage(lf2_stage_runtime_t *rt,int major,int host_char,int guest_char,int difficulty,uint32_t seed,int host_hp,int host_mp,int guest_hp,int guest_mp,int *host_hp_out,int *host_mp_out,int *guest_hp_out,int *guest_mp_out){
    if(!rt||major<0||major>=5)return -1;(void)host_char;memset(rt,0,sizeof(*rt));
    rt->player_hp=host_hp;rt->player_mp=host_mp;rt->player_hp_out=host_hp_out;rt->player_mp_out=host_mp_out;
    rt->ally_count=1;rt->ally_roster[0]=guest_char;rt->ally_hp[0]=guest_hp;rt->ally_mp[0]=guest_mp;rt->ally_hp_out=guest_hp_out;rt->ally_mp_out=guest_mp_out;
    rt->lockstep_actor_map_enabled=true;rt->lockstep_native_vita_input=true;rt->lockstep_actor_control[0]=1;rt->lockstep_actor_control[1]=5;rt->lockstep_frame=lf2_adhoc_lockstep_frame;rt->lockstep_state=lf2_adhoc_match_report_state;
    static char labels[5][24]={{0}};snprintf(labels[major],sizeof(labels[major]),"AdHoc Stage %d",major+1);rt->hud_prefix=labels[major];
    unsigned rng=seed^0x53544147u^(unsigned)(major*0x9E37u);int last_bound=0;
    for(int sub=0;sub<5;sub++){
        int stage_id=major*10+sub;lf2_stage_t st;if(lf2_stage_load(stage_id,&st)<0)return -2;if(st.phase_count<1)continue;
        int first=st.phases[0].bound,offset=(sub==0)?0:(last_bound+360-first);
        for(int ph=0;ph<st.phase_count&&rt->phase_count<LF2_STAGE_RUNTIME_MAX_PHASES;ph++){
            lf2_stage_runtime_phase_t *dst=&rt->phases[rt->phase_count++];dst->bound=st.phases[ph].bound+offset;if(dst->bound<=last_bound)dst->bound=last_bound+220;
            dst->enemy_count=lf2_stage_build_phase(&st.phases[ph],difficulty,dst->enemies,dst->hp,LF2_STAGE_RUNTIME_MAX_ENEMIES,&rng);
            dst->item_count=lf2_stage_build_items(&st.phases[ph],difficulty,dst->item_oid,dst->item_x,LF2_STAGE_RUNTIME_MAX_ITEMS,&rng);last_bound=dst->bound;
        }
    }
    return rt->phase_count>0?0:-3;
}

static int net70_run_adhoc_stage(uint32_t match_id,int host_char,int guest_char,int start_major,int difficulty,bool net_cheat){
    int host_hp=500,host_mp=500,guest_hp=500,guest_mp=500;
    if(start_major<0||start_major>4)start_major=0;
    for(int major=start_major;major<5;major++){
        lf2_stage_runtime_t rt;int brc=net70_build_network_stage(&rt,major,host_char,guest_char,difficulty,match_id,host_hp,host_mp,guest_hp,guest_mp,&host_hp,&host_mp,&guest_hp,&guest_mp);if(brc<0)return -20+brc;
        vita2d_texture *bg=load_stage_texture(stage_background_for_major(major));if(!bg)return -30;
        uint32_t sub_id=match_id*16u+(uint32_t)(major+1);if(!sub_id)sub_id=(uint32_t)(major+1);
        if(lf2_adhoc_match_begin(sub_id)<0){free_stage_texture(bg);return -31;}
        lf2_log_stage("network:adhoc_stage_launch","id=%u major=%d host=%s guest=%s phases=%d",(unsigned)sub_id,major+1,lf2_roster[host_char].name,lf2_roster[guest_char].name,rt.phase_count);
        int rc=lf2_run_stage_section(g_font,bg,host_char,difficulty,net_cheat,&rt);free_stage_texture(bg);lf2_adhoc_match_end();if(rc!=LF2_MATCH_WIN)return rc;
        int heal=(difficulty==LF2_DIFF_EASY)?200:(difficulty==LF2_DIFF_NORMAL?150:(difficulty==LF2_DIFF_DIFFICULT?100:50));
        host_hp+=heal;if(host_hp>500)host_hp=500;guest_hp+=heal;if(guest_hp>500)guest_hp=500;host_mp=guest_mp=500;
    }
    return LF2_MATCH_WIN;
}

static int net70_run_adhoc_mode(uint32_t match_id){
    const lf2_adhoc_lobby_t *l=lf2_adhoc_lobby();int host_char=0,guest_char=0,guest_slot=0;
    int pc=net70_active_pair_chars(l,&host_char,&guest_char,&guest_slot);if(pc<0)return pc;
    int host_partner=l->players[0].partner_character,guest_partner=l->players[guest_slot].partner_character;
    int host_follower=l->players[0].battle_follower,host_count=l->players[0].battle_count,guest_follower=l->players[guest_slot].battle_follower,guest_count=l->players[guest_slot].battle_count;
    int mode=l->settings.mode;int stage=(l->settings.stage<STAGE_COUNT)?l->settings.stage:0;int difficulty=l->settings.difficulty;
    if(difficulty<LF2_DIFF_EASY||difficulty>LF2_DIFF_CRAZY)difficulty=LF2_DIFF_NORMAL;bool net_cheat=l->settings.allow_cheat!=0;
    lf2_log_stage("network:adhoc_match_launch","id=%u role=%s mode=%s active_guest_slot=%d host_char=%s client_char=%s host_partner=%d client_partner=%d stage=%d diff=%s cheat=%d",
                  (unsigned)match_id,l->host?"host":"client",lf2_net_mode_name(mode),guest_slot,lf2_roster[host_char].name,lf2_roster[guest_char].name,host_partner,guest_partner,stage,ui_diff_name(difficulty),net_cheat?1:0);
    int rc;if(mode==LF2_NET_MODE_STAGE)rc=net70_run_adhoc_stage(match_id,host_char,guest_char,stage%5,difficulty,net_cheat);else if(mode==LF2_NET_MODE_1V1)rc=net70_run_adhoc_championship_1v1(match_id,host_char,guest_char,stage,difficulty,net_cheat);else if(mode==LF2_NET_MODE_2V2)rc=net70_run_adhoc_championship_2v2(match_id,host_char,guest_char,host_partner,guest_partner,stage,difficulty,net_cheat);else rc=net70_run_adhoc_arena_match(match_id,mode,host_char,guest_char,host_partner,guest_partner,host_follower,host_count,guest_follower,guest_count,stage,difficulty,net_cheat);
    lf2_log_stage("network:adhoc_match_return","id=%u role=%s mode=%s rc=%d",(unsigned)match_id,l->host?"host":"client",lf2_net_mode_name(mode),rc);return rc;
}

