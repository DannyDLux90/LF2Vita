    f->roster_idx=roster_idx;f->transform_origin=-1;f->transform_target=-1;f->fusion_partner_slot=-1;f->holding_slot=-1;f->caught_by=-1;f->thrown_by=-1;
    if(hp<=0)hp=500; f->hp=f->max_hp=hp; f->mp=500;
    lf2_log_stage("match:character_parse","slot=%s%d roster=%d name=%s team=%d dat=%s",ai?"CPU":"P",ai?slot:1,roster_idx,lf2_roster[roster_idx].name,team,lf2_roster[roster_idx].dat);
    int rc=lf2_load_character(GAME_ROOT,lf2_roster[roster_idx].dat,&f->def);if(rc<0){lf2_logf("ERROR","character parse failed rc=%d",rc);return -1;}
    rc=lf2_load_character_textures(GAME_ROOT,&f->def);if(rc<0){lf2_logf("ERROR","character textures failed rc=%d",rc);lf2_free_character_textures(&f->def);return -2;}
    if(f->def.small[0]){f->hud_face=lf2_load_bmp_colorkey(f->def.small);if(f->hud_face)vita2d_texture_set_filters(f->hud_face,SCE_GXM_TEXTURE_FILTER_POINT,SCE_GXM_TEXTURE_FILTER_POINT);else lf2_logf("WARN","HUD portrait missing char=%s path=%s",f->def.name,f->def.small);}
    set_frame(f,0); lf2_logf("INFO","fighter ready slot=%s%d name=%s team=%d sheets=%d",ai?"CPU":"P",ai?slot:1,f->def.name,team,f->def.sheet_count); return 0;
}

static int lf2_id_to_roster(int id){
    switch(id){
        case 1:return 0;case 2:return 1;case 4:return 2;case 5:return 3;case 6:return 4;
        case 7:return 5;case 8:return 6;case 9:return 7;case 10:return 8;case 11:return 9;
        case 30:return 10;case 31:return 11;case 32:return 12;case 33:return 13;case 34:return 14;
        case 35:return 15;case 36:return 16;case 37:return 17;case 38:return 19;case 39:return 18;
        case 50:return 20;case 51:return 21;case 52:return 22;default:return -1;
    }
}

/* Reload only the authored type-0 data/texture set. Runtime ownership, team,
   position, HP/MP and transformation metadata stay attached to the same slot. */
static int fighter_change_form(fighter_t *f,int roster_idx,int start_frame,const char *reason){
    if(!f||roster_idx<0||roster_idx>=lf2_roster_count)return -1;
    char old_name[32];snprintf(old_name,sizeof(old_name),"%s",f->def.name[0]?f->def.name:"?");
    if(f->owns_assets){
        vita2d_wait_rendering_done();
        if(f->hud_face){vita2d_free_texture(f->hud_face);f->hud_face=NULL;}
        lf2_free_character_textures(&f->def);
    }else{
        /* Stage slots normally reference shared preloaded assets. Detach the
           shallow copy before loading a private transformed form. */
        f->hud_face=NULL;
    }
    memset(&f->def,0,sizeof(f->def));f->owns_assets=true;
    int rc=lf2_load_character(GAME_ROOT,lf2_roster[roster_idx].dat,&f->def);
    if(rc<0){lf2_logf("ERROR","FORM parse failed %s -> %s rc=%d",old_name,lf2_roster[roster_idx].name,rc);return -2;}
    rc=lf2_load_character_textures(GAME_ROOT,&f->def);
    if(rc<0){lf2_logf("ERROR","FORM textures failed %s rc=%d",lf2_roster[roster_idx].name,rc);return -3;}
    if(f->def.small[0]){f->hud_face=lf2_load_bmp_colorkey(f->def.small);if(f->hud_face)vita2d_texture_set_filters(f->hud_face,SCE_GXM_TEXTURE_FILTER_POINT,SCE_GXM_TEXTURE_FILTER_POINT);}
    f->roster_idx=roster_idx;f->dead=false;f->falling=false;f->frozen_ticks=f->burn_ticks=f->sonata_ticks=0;
    f->hit_latched=false;f->opoint_fired=false;f->invuln=10;f->status_age=0;
    if(start_frame<0||start_frame>=LF2_MAX_FRAMES||!f->def.frames[start_frame].present)start_frame=0;
    set_frame(f,start_frame);f->state_processed_serial=0;
    lf2_logf("FORM","%s -> %s frame=%d reason=%s hp=%d mp=%d",old_name,f->def.name,start_frame,reason?reason:"-",f->hp,f->mp);
    return 0;
}

static void spawn_louis_armor(object_world_t *world,fighter_t *f,int owner_idx){
    if(!world||!f||!f->active)return;
    lf2_frame_def_t *fr=frame_def(f);if(!fr)return;
    /* State 9996 is hard-coded in LF2: four id-217 armour pieces around the
       fighter and one id-218 piece in the centre. Give them small outward
       impulses so the familiar armour burst remains visible on Vita. */
    const int oid[5]={217,217,217,217,218};
    const int dx[5]={-24,24,-24,24,0};
    const int dy[5]={-38,-38,-12,-12,-26};
    const int vx[5]={-5,5,-4,4,0};
    const int vy[5]={-8,-8,-6,-6,-9};
    for(int i=0;i<5;i++){
        lf2_opoint_def_t op;memset(&op,0,sizeof(op));
        op.kind=1;op.oid=oid[i];op.action=0;op.facing=0;
        op.x=fr->centerx+dx[i];op.y=fr->centery+dy[i];op.dvx=vx[i];op.dvy=vy[i];
        object_spawn_one(world,f,owner_idx,&op,0,1);
    }
    lf2_logf("FORM","Louis armour burst state9996 owner=%s",f->def.name);
}

static fighter_t *farthest_living_teammate(fighter_t *me,fighter_t *fighters,int total){
    fighter_t *best=NULL;float bd=-1.0f;
    for(int i=0;i<total;i++){fighter_t *c=&fighters[i];if(!c->active||c==me||c->dead||me->team==0||c->team!=me->team)continue;float dx=c->x-me->x,dz=c->z-me->z,d=dx*dx+dz*dz;if(d>bd){bd=d;best=c;}}
    return best;
}

static void process_frame_state(fighter_t *fighters,int total,int idx,object_world_t *world){
    if(!fighters||idx<0||idx>=total)return;fighter_t *f=&fighters[idx];if(!f->active||f->dead)return;
    if(f->state_processed_serial==f->frame_serial)return;f->state_processed_serial=f->frame_serial;
    lf2_frame_def_t *fr=frame_def(f);if(!fr)return;int st=fr->state;
    release_held_throw(world,f,idx);
    process_cpoint_event(fighters,total,idx);
    if(st==400){
        fighter_t *t=nearest_living_enemy(f,fighters,total);if(t){float side=f->x<t->x?-1.0f:1.0f;f->x=clampf(t->x+side*120.0f,g_world_min_x,g_world_max_x);f->z=t->z;f->y=0;f->vx=f->vy=f->vz=0;lf2_logf("STATE","%s teleport enemy=%s x=%.1f z=%.1f",f->def.name,t->def.name,f->x,f->z);}return;
    }
    if(st==401){
        fighter_t *t=farthest_living_teammate(f,fighters,total);if(t){float side=f->x<t->x?-1.0f:1.0f;f->x=clampf(t->x+side*60.0f,g_world_min_x,g_world_max_x);f->z=t->z;f->y=0;f->vx=f->vy=f->vz=0;lf2_logf("STATE","%s teleport teammate=%s x=%.1f z=%.1f",f->def.name,t->def.name,f->x,f->z);}return;
    }
    if(st==1700){int old=f->hp;f->hp+=100;if(f->hp>f->max_hp)f->hp=f->max_hp;lf2_logf("STATE","%s heal state1700 hp=%d->%d",f->def.name,old,f->hp);return;}
    /* Rudolf's first transform is the caught-target move on frames 235-240.
       The normal walking grab records the victim roster; frame 239 carries
       throwinjury:-1 in the original cpoint and is the actual transform cue. */
    if(f->roster_idx==3 && f->frame==239 && f->transform_target>=0){
        int target=f->transform_target;f->transform_origin=3;
        fighter_change_form(f,target,0,"Rudolf caught-target transform");return;
    }
    /* state 500/501 is Rudolf's second transformation. It is only enabled
       after a target was learned by the first caught transformation. */
    if(st==500 && f->roster_idx==3){
        if(f->transform_target>=0)lf2_logf("FORM","Rudolf second-transform armed roster=%d",f->transform_target);
        else lf2_logf("FORM","Rudolf second-transform locked: no captured target");
        return;
    }
    if(st==501 && f->roster_idx==3 && f->transform_target>=0){int target=f->transform_target;f->transform_origin=3;fighter_change_form(f,target,0,"Rudolf second transform");return;}
    if(st==9996 && f->roster_idx==4){spawn_louis_armor(world,f,idx);return;}
    if(st==9995 && f->roster_idx==4){fighter_change_form(f,20,0,"Louis -> LouisEX");return;}
    if(st>=8000&&st<9000){int r=lf2_id_to_roster(st-8000);if(r>=0&&r!=f->roster_idx)fighter_change_form(f,r,0,"state 8xxx");return;}
}

static int defuse_firzen(fighter_t *fighters,int total,int controller,const char *reason){
    if(!fighters||controller<0||controller>=total)return 0;fighter_t *f=&fighters[controller];
    if(!f->active||f->roster_idx!=21||f->fusion_return_roster<0||f->fusion_partner_roster<0)return 0;
    int ret=f->fusion_return_roster,pr=f->fusion_partner_roster,slot=f->fusion_partner_slot;
    int php=f->fusion_partner_hp,pmp=f->fusion_partner_mp,team=f->fusion_team;bool pai=f->fusion_partner_ai;
    float x=f->x,z=f->z;int facing=f->facing;
    f->fusion_ticks=0;f->fusion_return_roster=-1;f->fusion_partner_roster=-1;f->fusion_partner_slot=-1;
    fighter_change_form(f,ret,112,"Firzen defusion");
    if(slot>0&&slot<total&&!fighters[slot].active){
        float px=clampf(x+(facing?-52.0f:52.0f),g_world_min_x,g_world_max_x);
        if(load_fighter(&fighters[slot],pr,pai,team,px,z,facing^1,slot,php>0?php:1)==0){fighters[slot].mp=pmp;if(fighters[slot].def.frames[112].present)set_frame(&fighters[slot],112);}
    }
    lf2_logf("FORM","Firzen defuse reason=%s controller=%s partner=%s",reason?reason:"-",f->def.name,(slot>0&&slot<total&&fighters[slot].active)?fighters[slot].def.name:"missing");
    return 1;
}

static int handle_form_input(fighter_t *fighters,int total,int idx,const input_t *in){
    if(!fighters||!in||idx<0||idx>=total)return 0;fighter_t *f=&fighters[idx];
    int dja=in->defend && ((in->attack_pressed&&in->jump)||(in->jump_pressed&&in->attack));
    if(!dja)return 0;
    if(f->roster_idx==21&&f->fusion_ticks>0)return defuse_firzen(fighters,total,idx,"DJA");
    if(f->transform_origin==3&&f->roster_idx!=3){
        fighter_change_form(f,3,245,"Rudolf transform back");f->transform_origin=-1;return 1;
    }
    return 0;
}

static void check_firzen_fusion(fighter_t *fighters,int total,object_world_t *world){
    if(!fighters)return;
    for(int i=0;i<total;i++)for(int j=i+1;j<total;j++){
        fighter_t *a=&fighters[i],*b=&fighters[j];if(!a->active||!b->active||a->dead||b->dead)continue;
        int pair=((a->roster_idx==5&&b->roster_idx==6)||(a->roster_idx==6&&b->roster_idx==5));if(!pair)continue;
        if(a->team==0||a->team!=b->team)continue;
        lf2_frame_def_t *af=frame_def(a),*bf=frame_def(b);if(!af||!bf||af->state!=2||bf->state!=2)continue;
        if(!g_runtime_cheat&&(a->hp>176||b->hp>176))continue;
        if(fabsf(a->x-b->x)>50.0f||fabsf(a->z-b->z)>22.0f)continue;
        int controller=(i==0||j==0)?0:(a->x>=b->x?i:j);int partner=controller==i?j:i;
        fighter_t *c=&fighters[controller],*p=&fighters[partner];
        int ret=c->roster_idx,pr=p->roster_idx;int php=p->hp,pmp=p->mp;bool pai=p->ai;int team=c->team;
        float mx=(c->x+p->x)*0.5f,mz=(c->z+p->z)*0.5f;
        unload_fighter_slot(p);c->x=mx;c->z=mz;
        c->fusion_return_roster=ret;c->fusion_partner_roster=pr;c->fusion_partner_slot=partner;c->fusion_partner_hp=php;c->fusion_partner_mp=pmp;c->fusion_partner_ai=pai;c->fusion_team=team;c->fusion_ticks=4500;
        if(fighter_change_form(c,21,290,"Firen + Freeze fusion")==0){if(world)spawn_hit_fx(world,c,2);lf2_logf("FORM","FUSION slots=%d+%d controller=%d Firzen ticks=4500 hp=%d",i,j,controller,c->hp);}return;
    }
}

static void tick_fusion(fighter_t *fighters,int total){
    for(int i=0;i<total;i++){fighter_t *f=&fighters[i];if(!f->active||f->roster_idx!=21||f->fusion_ticks<=0)continue;if(--f->fusion_ticks<=0){defuse_firzen(fighters,total,i,"4500tu timeout");return;}}
}

static const float spawn_x[LF2_MAX_CPUS+1]={165.0f,795.0f,705.0f,615.0f,525.0f,435.0f,345.0f,255.0f};
static const float spawn_z[LF2_MAX_CPUS+1]={0.0f,-52.0f,-18.0f,18.0f,52.0f,-35.0f,35.0f,0.0f};

static void reset_fighter_state(fighter_t *f,float x,float z,int facing){
    f->x=x;f->z=z;f->y=0;f->vx=f->vy=f->vz=0;f->facing=facing;f->hp=f->max_hp;f->mp=500;f->dead=false;f->invuln=0;f->ai_cooldown=0;f->hit_latched=false;f->fall_points=0;f->falling=false;f->hit_flash=0;f->frozen_ticks=0;f->burn_ticks=0;f->sonata_ticks=0;f->sonata_damage_cooldown=0;f->status_age=0;f->mp_regen_tick=0;f->grabbed_ticks=0;f->ice_sword_ticks=0;f->held_oid=0;f->held_units=0;f->drinking=false;f->drink_tick=0;f->fusion_ticks=0;f->fusion_return_roster=-1;f->fusion_partner_roster=-1;f->fusion_partner_slot=-1;f->holding_slot=-1;f->caught_by=-1;f->thrown_by=-1;f->arest_ticks=0;memset(f->vrest_ticks,0,sizeof(f->vrest_ticks));set_frame(f,0);
}

static void reset_match(fighter_t *fighters,int cpu_count){
    reset_fighter_state(&fighters[0],spawn_x[0],spawn_z[0],0);
    for(int i=0;i<cpu_count;i++)reset_fighter_state(&fighters[i+1],spawn_x[i+1],spawn_z[i+1],1);
    for(int i=cpu_count+1;i<=LF2_MAX_CPUS;i++)if(fighters[i].active)unload_fighter_slot(&fighters[i]);
}

int lf2_run_match_ex(vita2d_pgf *font, vita2d_texture *stage_bg,
                     int player_index, int player_team,
                     const int *cpu_indices, const int *cpu_teams, int cpu_count,
                     int difficulty, bool cheat_enabled,
                     const lf2_match_options_t *options) {
    lf2_audio_music_stop();
    g_runtime_cheat=cheat_enabled;
    g_world_min_x=40.0f;g_world_max_x=920.0f;g_camera_x=0.0f;
    lf2_match_options_t defaults={0};
    defaults.allow_rematch=true;
    if(!options)options=&defaults;
    if(player_index<0||player_index>=lf2_roster_count||!cpu_indices||!cpu_teams||cpu_count<1||cpu_count>LF2_MAX_CPUS)return -1;
    if(player_team<0||player_team>4)return -1;
    if(difficulty<LF2_DIFF_EASY||difficulty>LF2_DIFF_CRAZY)return -1;
    for(int i=0;i<cpu_count;i++)if(cpu_indices[i]<0||cpu_indices[i]>=lf2_roster_count||cpu_teams[i]<0||cpu_teams[i]>4)return -1;
    lf2_log_stage("match:alloc","fighter_size=%u p1=%s team=%d cpus=%d difficulty=%s",(unsigned)sizeof(fighter_t),lf2_roster[player_index].name,player_team,cpu_count,difficulty_name(difficulty));
    int total=LF2_MAX_CPUS+1;fighter_t *fighters=(fighter_t*)calloc((size_t)total,sizeof(*fighters));
    if(!fighters){lf2_logf("ERROR","fighter heap allocation failed count=%d",total);return -10;}
    object_world_t *world=(object_world_t*)calloc(1,sizeof(*world));
    if(!world){lf2_logf("ERROR","object world allocation failed bytes=%u",(unsigned)sizeof(*world));free(fighters);return -11;}
    lf2_log_memory("fighters + object world allocated");
    for(int i=cpu_count+1;i<total;i++)fighters[i].dead=true;
    int pmax=options->player_max_hp>0?options->player_max_hp:500;
    if(load_fighter(&fighters[0],player_index,options->player_ai,player_team,spawn_x[0],spawn_z[0],0,1,pmax)<0){free(world);free(fighters);return -2;}
    if(options->player_hp>0&&options->player_hp<pmax)fighters[0].hp=options->player_hp;
    fighters[0].mp=(options->player_mp>0&&options->player_mp<=500)?options->player_mp:500;
    int loaded=1;
    for(int i=0;i<cpu_count;i++){
        int chp=(options->cpu_hp&&options->cpu_hp[i]>0)?options->cpu_hp[i]:500;
        if(load_fighter(&fighters[i+1],cpu_indices[i],true,cpu_teams[i],spawn_x[i+1],spawn_z[i+1],1,i+1,chp)<0){
            vita2d_wait_rendering_done();for(int j=0;j<loaded;j++){if(fighters[j].hud_face)vita2d_free_texture(fighters[j].hud_face);lf2_free_character_textures(&fighters[j].def);}world_shutdown(world);free(world);free(fighters);return -3-i;
        }loaded++;
    }
    lf2_log_stage("match:gpu_ready","p1=%s cpus=%d diff=%s cheat=%d pool_free=%u",fighters[0].def.name,cpu_count,difficulty_name(difficulty),cheat_enabled?1:0,(unsigned)vita2d_pool_free_space());
    lf2_log_stage("match:first_frame_submit","pool_free=%u",(unsigned)vita2d_pool_free_space());
    draw_match(font,stage_bg,fighters,world,cpu_count,difficulty,cheat_enabled,false,options);vita2d_wait_rendering_done();
    lf2_log_stage("match:first_frame_complete","pool_free=%u",(unsigned)vita2d_pool_free_space());
    uint32_t prev=0;SceCtrlData first={0};sceCtrlPeekBufferPositive(0,&first,1);prev=first.buttons;
    uint32_t pending_pressed=0;
    bool paused=false;int result=0;
    srand(0x4c463256u+(unsigned)player_index*17u+(unsigned)cpu_count*47u+(unsigned)difficulty*101u);
    int item_drop_timer=240+(rand()%180);
    /* LF2 logic is based on 30 Hz time units. Rendering remains vblank-paced
       at 60 Hz, while gameplay advances at a fixed 30 Hz. This keeps wait,
       walking speeds, gravity, attack windows and animation timing in the
       same time domain as the original data files. */
    const uint64_t sim_step_us=33333;
    uint64_t last_us=sceKernelGetSystemTimeWide(),acc_us=0;
    lf2_logf("ANIM","fixed simulation rate=30Hz render_vblank=60Hz");
    uint64_t result_since=0;
    for(;;){
        SceCtrlData pad={0};sceCtrlPeekBufferPositive(0,&pad,1);
        uint32_t raw_pressed=pad.buttons&~prev;prev=pad.buttons;pending_pressed|=raw_pressed;
        uint64_t now_us=sceKernelGetSystemTimeWide();uint64_t dt_us=now_us-last_us;last_us=now_us;
        if(dt_us>100000)dt_us=100000; /* debugger / scheduling stall guard */

        if(options->player_ai && options->allow_cancel && (raw_pressed&SCE_CTRL_CIRCLE)){result=LF2_MATCH_CANCEL;break;}
        if(raw_pressed&SCE_CTRL_START){paused=!paused;acc_us=0;pending_pressed=0;lf2_logf("GAME","pause=%d",paused?1:0);}
        if(paused){
            if(raw_pressed&SCE_CTRL_CIRCLE){result=LF2_MATCH_CANCEL;break;}
            draw_match(font,stage_bg,fighters,world,cpu_count,difficulty,cheat_enabled,paused,options);continue;
        }

        int side_alive=player_side_alive(fighters,total);
        int enemies_alive=player_has_enemy(fighters,total);
        if(!side_alive||!enemies_alive){
            int outcome=(side_alive&&!enemies_alive)?LF2_MATCH_WIN:LF2_MATCH_LOSS;
            pending_pressed=0;
            if(!result_since){result_since=now_us;lf2_logf("GAME","round over outcome=%d; settling KO physics",outcome);}
            /* Keep the 30 Hz simulation alive after KO. No new attacks are
               allowed, but bodies, projectiles and hit particles finish their
               authored motion so a fighter never freezes in mid-air. */
            acc_us+=dt_us;int settle_guard=0;input_t zero={0};
            while(acc_us>=sim_step_us&&settle_guard++<4){
                for(int i=0;i<total;i++)update_control(&fighters[i],&zero);
                for(int i=0;i<LF2_MAX_ACTIVE_OBJECTS;i++)if(world->objects[i].active)update_object(world,&world->objects[i],fighters,total);
                update_hit_fx(world);acc_us-=sim_step_us;
            }
            if(options->auto_continue){
                if(dead_bodies_settled(fighters,total)&&now_us-result_since>=650000ULL){result=outcome;break;}
