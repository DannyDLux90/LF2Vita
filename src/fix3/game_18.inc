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
