
static uint32_t net70_mix32(uint32_t x){
    x^=x>>16;x*=0x7FEB352Du;x^=x>>15;x*=0x846CA68Bu;x^=x>>16;return x;
}
static uint32_t net70_champ_sub_id(uint32_t base,uint32_t tag,int round,int pair){
    uint32_t id=net70_mix32(base^tag^((uint32_t)round*0x9E3779B9u)^((uint32_t)pair*0x85EBCA6Bu));return id?id:(tag|1u);
}
static int net70_champ_cpu_char(uint32_t seed,int index){return (int)(net70_mix32(seed^((uint32_t)(index+1)*0x45D9F3Bu))%10u);}
static int net70_champ_coin(uint32_t seed,int round,int pair){return (int)(net70_mix32(seed^0xC0FFEE11u^((uint32_t)round<<16)^(uint32_t)pair)&1u);}

static void net70_champ_round_card(const char *title,int round,int matches){
    char sub[96];snprintf(sub,sizeof(sub),"Runde %d  |  %d Begegnung%s",round,matches,matches==1?"":"en");
    mode_card(title,sub,650);
}

static int net70_run_1v1_human_match(vita2d_texture *bg,uint32_t sub_id,net70_champ_1v1_entry_t a,net70_champ_1v1_entry_t b,int round,int difficulty,bool net_cheat){
    net70_champ_1v1_entry_t human=a.owner?a:b,opp=a.owner?b:a;
    int both=(a.owner&&b.owner)?1:0;
    int player=human.fighter,cpu[1]={opp.fighter},teams[1]={2};
    int host_actor=-1,guest_actor=-1;
    if(both){
        /* Keep the host as logical actor 0 on both devices so result meaning is deterministic. */
        net70_champ_1v1_entry_t host=(a.owner==1)?a:b,guest=(a.owner==2)?a:b;
        player=host.fighter;cpu[0]=guest.fighter;host_actor=0;guest_actor=1;
    }else if(human.owner==1)host_actor=0;else guest_actor=0;
    if(lf2_adhoc_match_begin(sub_id)<0)return -41;
    char label[64];snprintf(label,sizeof(label),"AdHoc 1v1 Championship R%d",round);
    lf2_match_options_t opt;net70_native_match_options(&opt,label,host_actor,guest_actor);
    int rc=lf2_run_match_ex(g_font,bg,player,1,cpu,teams,1,difficulty,net_cheat,&opt);
    lf2_adhoc_match_end();
    if(both)return rc==LF2_MATCH_WIN?1:2; /* owner that won */
    return rc==LF2_MATCH_WIN?human.owner:0; /* 0 means CPU opponent */
}

static int net70_run_adhoc_championship_1v1(uint32_t match_id,int host_char,int guest_char,int stage,int difficulty,bool net_cheat){
    net70_champ_1v1_entry_t cur[16],next[16];int count=16;
    cur[0]=(net70_champ_1v1_entry_t){host_char,1};cur[15]=(net70_champ_1v1_entry_t){guest_char,2};
    for(int i=1;i<15;i++)cur[i]=(net70_champ_1v1_entry_t){net70_champ_cpu_char(match_id^0x1A1A1A1Au,i),0};
    vita2d_texture *bg=load_stage_texture(stage);if(!bg)return -40;
    lf2_log_stage("network:adhoc_champ1_start","id=%u host=%s guest=%s",(unsigned)match_id,lf2_roster[host_char].name,lf2_roster[guest_char].name);
    for(int round=1;count>1;round++){
        int pairs=count/2;net70_champ_round_card("AdHoc 1 on 1 Championship",round,pairs);
        for(int i=0;i<pairs;i++){
            net70_champ_1v1_entry_t a=cur[i*2],b=cur[i*2+1];
            if(!a.owner&&!b.owner){next[i]=net70_champ_coin(match_id,round,i)?a:b;continue;}
            uint32_t sub_id=net70_champ_sub_id(match_id,0x31433131u,round,i);
            int owner=net70_run_1v1_human_match(bg,sub_id,a,b,round,difficulty,net_cheat);if(owner<0){free_stage_texture(bg);return owner;}
            if(a.owner&&b.owner)next[i]=(owner==a.owner)?a:b;
            else {net70_champ_1v1_entry_t human=a.owner?a:b,cpu=a.owner?b:a;next[i]=owner?human:cpu;}
            lf2_logf("MODE","adhoc1v1 round=%d pair=%d winner=%s owner=%u",round,i,lf2_roster[next[i].fighter].name,(unsigned)next[i].owner);
        }
        memcpy(cur,next,(size_t)pairs*sizeof(cur[0]));count=pairs;
    }
    free_stage_texture(bg);
    char msg[80];snprintf(msg,sizeof(msg),"Champion: %s",lf2_roster[cur[0].fighter].name);mode_card("ADHOC CHAMPION",msg,1600);
    lf2_log_stage("network:adhoc_champ1_end","winner=%s owner=%u",lf2_roster[cur[0].fighter].name,(unsigned)cur[0].owner);return LF2_MATCH_WIN;
}

static int net70_run_2v2_human_match(vita2d_texture *bg,uint32_t sub_id,net70_champ_2v2_team_t a,net70_champ_2v2_team_t b,int round,int difficulty,bool net_cheat){
    int both=(a.owner&&b.owner)?1:0;net70_champ_2v2_team_t human=a.owner?a:b,opp=a.owner?b:a;
    int player=human.a,cpus[3]={human.b,opp.a,opp.b},teams[3]={1,2,2};int host_actor=-1,guest_actor=-1;
    if(both){
        net70_champ_2v2_team_t host=(a.owner==1)?a:b,guest=(a.owner==2)?a:b;
        player=host.a;cpus[0]=host.b;cpus[1]=guest.a;cpus[2]=guest.b;host_actor=0;guest_actor=2;
    }else if(human.owner==1)host_actor=0;else guest_actor=0;
    if(lf2_adhoc_match_begin(sub_id)<0)return -51;
    char label[64];snprintf(label,sizeof(label),"AdHoc 2v2 Championship R%d",round);
    lf2_match_options_t opt;net70_native_match_options(&opt,label,host_actor,guest_actor);
    int rc=lf2_run_match_ex(g_font,bg,player,1,cpus,teams,3,difficulty,net_cheat,&opt);lf2_adhoc_match_end();
    if(both)return rc==LF2_MATCH_WIN?1:2;return rc==LF2_MATCH_WIN?human.owner:0;
}

static int net70_run_adhoc_championship_2v2(uint32_t match_id,int host_char,int guest_char,int host_partner,int guest_partner,int stage,int difficulty,bool net_cheat){
    net70_champ_2v2_team_t cur[8],next[8];int count=8;
    if(host_partner<0||host_partner>=lf2_roster_count)host_partner=(host_char+1)%10;if(guest_partner<0||guest_partner>=lf2_roster_count)guest_partner=(guest_char+1)%10;
    cur[0]=(net70_champ_2v2_team_t){host_char,host_partner,1};cur[7]=(net70_champ_2v2_team_t){guest_char,guest_partner,2};
    for(int i=1;i<7;i++)cur[i]=(net70_champ_2v2_team_t){net70_champ_cpu_char(match_id^0x22220001u,i*2),net70_champ_cpu_char(match_id^0x22220002u,i*2+1),0};
    vita2d_texture *bg=load_stage_texture(stage);if(!bg)return -50;
    lf2_log_stage("network:adhoc_champ2_start","id=%u host=%s+%s guest=%s+%s",(unsigned)match_id,lf2_roster[host_char].name,lf2_roster[host_partner].name,lf2_roster[guest_char].name,lf2_roster[guest_partner].name);
    for(int round=1;count>1;round++){
        int pairs=count/2;net70_champ_round_card("AdHoc 2 on 2 Championship",round,pairs);
        for(int i=0;i<pairs;i++){
            net70_champ_2v2_team_t a=cur[i*2],b=cur[i*2+1];
            if(!a.owner&&!b.owner){next[i]=net70_champ_coin(match_id^0x2222u,round,i)?a:b;continue;}
            uint32_t sub_id=net70_champ_sub_id(match_id,0x32433232u,round,i);
            int owner=net70_run_2v2_human_match(bg,sub_id,a,b,round,difficulty,net_cheat);if(owner<0){free_stage_texture(bg);return owner;}
            if(a.owner&&b.owner)next[i]=(owner==a.owner)?a:b;
            else {net70_champ_2v2_team_t human=a.owner?a:b,cpu=a.owner?b:a;next[i]=owner?human:cpu;}
            lf2_logf("MODE","adhoc2v2 round=%d pair=%d winner=%s+%s owner=%u",round,i,lf2_roster[next[i].a].name,lf2_roster[next[i].b].name,(unsigned)next[i].owner);
        }
        memcpy(cur,next,(size_t)pairs*sizeof(cur[0]));count=pairs;
    }
    free_stage_texture(bg);char msg[96];snprintf(msg,sizeof(msg),"Champions: %s + %s",lf2_roster[cur[0].a].name,lf2_roster[cur[0].b].name);mode_card("ADHOC CHAMPIONS",msg,1600);
    lf2_log_stage("network:adhoc_champ2_end","winner=%s+%s owner=%u",lf2_roster[cur[0].a].name,lf2_roster[cur[0].b].name,(unsigned)cur[0].owner);return LF2_MATCH_WIN;
}

