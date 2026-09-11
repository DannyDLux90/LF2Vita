
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

