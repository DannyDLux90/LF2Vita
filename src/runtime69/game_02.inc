    if(!me||id<=0||id>=LF2_MAX_FRAMES||!me->def.frames[id].present)return 0;
    if(me->roster_idx==3&&id==295&&me->transform_target<0)return 0;
    int raw=me->def.frames[id].mp;
    int cost=raw>0?mp_component(raw):0;
    int hp_cost=raw>0?hp_component(raw):0;
    return me->mp>=cost && (hp_cost<=0||me->hp>hp_cost);
}

static int ai_choose_special(fighter_t *me,input_t *in,float dx,float dz,int difficulty){
    if(!me||!in)return 0;lf2_frame_def_t *b=&me->def.frames[0];
    enum { C_FA,C_FJ,C_UA,C_DA,C_UJ,C_DJ,C_JA };
    int ids[7]={b->hit_Fa,b->hit_Fj,b->hit_Ua,b->hit_Da,b->hit_Uj,b->hit_Dj,b->hit_ja};
    int valid[7],n=0;for(int i=0;i<7;i++)if(ai_frame_affordable(me,ids[i]))valid[n++]=i;if(!n)return 0;
    int choice=valid[rand()%n];
    /* At range prefer forward projectile/dash commands when they exist. */
    if(fabsf(dx)>105.0f){if(ai_frame_affordable(me,b->hit_Fa))choice=C_FA;else if(ai_frame_affordable(me,b->hit_Fj))choice=C_FJ;}
    bool forward_left=me->facing!=0;
    in->defend=true;
    switch(choice){
        case C_FA: in->attack=in->attack_pressed=true;if(forward_left)in->left=true;else in->right=true;break;
        case C_FJ: in->jump=in->jump_pressed=true;if(forward_left)in->left=true;else in->right=true;break;
        case C_UA: in->attack=in->attack_pressed=true;in->up=true;break;
        case C_DA: in->attack=in->attack_pressed=true;in->down=true;break;
        case C_UJ: in->jump=in->jump_pressed=true;in->up=true;break;
        case C_DJ: in->jump=in->jump_pressed=true;in->down=true;break;
        case C_JA: in->defend=false;in->jump=in->jump_pressed=true;in->attack=in->attack_pressed=true;break;
    }
    lf2_logf("AI","%s skill frame=%d distance=%.1f dz=%.1f difficulty=%d",me->def.name,ids[choice],fabsf(dx),fabsf(dz),difficulty);
    return 1;
}

static input_t make_ai_input(fighter_t *me, fighter_t *other, int difficulty) {
    input_t in={0};
    if (me->dead || !other || other->dead) return in;
    float dx=other->x-me->x, dz=other->z-me->z;
    static const float close_by_diff[4]={66.0f,72.0f,80.0f,90.0f};
    static const int cooldown_by_diff[4]={50,36,25,17};
    static const int skill_mod_by_diff[4]={13,7,4,2};
    if(difficulty<LF2_DIFF_EASY)difficulty=LF2_DIFF_EASY;if(difficulty>LF2_DIFF_CRAZY)difficulty=LF2_DIFF_CRAZY;
    float closex=close_by_diff[difficulty];
    if(fabsf(dz)>12.0f){if(dz<0)in.up=true;else in.down=true;}
    if(fabsf(dx)>closex){if(dx<0)in.left=true;else in.right=true;if(fabsf(dx)>180.0f)in.run=true;}
    if(me->ai_cooldown>0)me->ai_cooldown--;
    if(me->ai_cooldown==0&&!in_action(me)&&me->frame!=110){
        int can_reach=fabsf(dx)<390.0f&&fabsf(dz)<48.0f;
        int try_skill=can_reach&&((rand()%skill_mod_by_diff[difficulty])==0);
        /* Ranged attacks must happen before the CPU has already walked into
           punch distance; Normal AI now visibly uses LF2's own DAT commands. */
        if(try_skill&&ai_choose_special(me,&in,dx,dz,difficulty))me->ai_cooldown=cooldown_by_diff[difficulty]+12;
        else if(fabsf(dx)<=closex&&fabsf(dz)<25.0f){in.attack_pressed=in.attack=true;me->ai_cooldown=cooldown_by_diff[difficulty];}
    }
    return in;
}

static int item_is_bottle(int oid){return oid==122||oid==123;}
static int item_is_heavy(int oid){return oid==150||oid==151;}
static int item_is_pickup_oid(int oid){return oid==100||oid==101||oid==120||oid==121||oid==122||oid==123||oid==124||oid==150||oid==151;}
static const char *item_name(int oid){
    switch(oid){
        case 100:return "Stick";case 101:return "Hoe";case 120:return "Knife";
        case 121:return "Baseball";case 122:return "Milk";case 123:return "Beer";
        case 124:return "Boomerang";case 150:return "Stone";case 151:return "Wooden Box";
        default:return "Item";
    }
}
static int item_units_for_oid(int oid){
    switch(oid){
        case 122:return 100; /* stock bottle HP/time units */
        case 123:return 125;
        case 100:return 12;case 101:return 11;case 120:return 10;case 121:return 8;
        case 124:return 12;case 150:return 7;case 151:return 6;default:return 8;
    }
}
static int item_weapon_bonus(int oid){
    switch(oid){
        case 100:return 22;case 101:return 28;case 120:return 30;case 121:return 20;
        case 124:return 28;case 150:return 34;case 151:return 40;default:return 0;
    }
}
static const char *item_hit_sound(int oid){
    switch(oid){
        case 100:return "data/011.wav";case 101:return "data/079.wav";case 120:return "data/027.wav";
        case 121:return "data/017.wav";case 124:return "data/027.wav";case 150:return "data/079.wav";
        case 151:return "data/079.wav";default:return "data/001.wav";
    }
}
static void finish_drink(fighter_t *f,int empty){
    if(!f)return;
    int oid=f->held_oid;f->drinking=false;f->drink_tick=0;
    if(empty){f->held_oid=0;f->held_units=0;lf2_audio_play("data/040.wav");lf2_logf("ITEM","%s finished %s",f->def.name,item_name(oid));}
    if(f->def.frames[0].present)set_frame(f,0);
}

static void fighter_physics_step(fighter_t *f){
    if(!f)return;
    if(f->y<0.0f||f->vy!=0.0f){
        f->y+=f->vy;f->vy+=1.70f;f->x+=f->vx;f->z+=f->vz;f->vx*=0.90f;f->vz*=0.88f;
        if(f->y>=0.0f){
            f->y=0;f->vy=0;f->vx=0;f->vz=0;f->thrown_by=-1;
            if(f->falling||f->dead){
                f->falling=false;
                int lie=(f->facing&&f->def.frames[231].present)?231:230;
                set_frame(f,lie);
            }else if(!in_action(f))set_frame(f,0);
        }
    }else if(in_action(f)){
        f->x+=f->vx;f->z+=f->vz;f->vx*=0.84f;f->vz*=0.82f;
    }
    f->x=clampf(f->x,g_world_min_x,g_world_max_x);
    f->z=clampf(f->z,-70.0f,75.0f);
}

static void update_control(fighter_t *f, const input_t *in) {
    static const input_t zero={0};
    if(!in)in=&zero;
    if(f->fall_points>0)f->fall_points--;
    if(f->hit_flash>0)f->hit_flash--;
    if(f->arest_ticks>0)f->arest_ticks--;
    for(int ri=0;ri<=LF2_MAX_CPUS;ri++)if(f->vrest_ticks[ri]>0)f->vrest_ticks[ri]--;
    if(f->sonata_damage_cooldown>0)f->sonata_damage_cooldown--;
    if(f->ice_sword_ticks>0)f->ice_sword_ticks--;
    if(f->drinking&&item_is_bottle(f->held_oid)&&!f->dead){
        /* Bottle healing is intentionally time-based rather than an instant
           pickup bonus. It follows the original 2.00a behaviour closely:
           milk mainly restores HP plus some MP, beer only restores MP. */
        if(!in->attack&&f->drink_tick>3){finish_drink(f,0);return;}
        f->drink_tick++;
        if(f->held_oid==122){
            if((f->drink_tick%5)==0 && f->hp<f->max_hp){f->hp+=8;if(f->hp>f->max_hp)f->hp=f->max_hp;}
            if((f->drink_tick%3)==0 && f->mp<500){f->mp+=5;if(f->mp>500)f->mp=500;}
        }else if(f->mp<500){f->mp+=6;if(f->mp>500)f->mp=500;}
        if(f->held_units>0)f->held_units--;
        if(f->frame<55||f->frame>58){if(f->def.frames[55].present)set_frame(f,55);}
        else update_animation(f,in);
        if(f->held_units<=0){finish_drink(f,1);return;}
        f->x=clampf(f->x,g_world_min_x,g_world_max_x);f->z=clampf(f->z,-70.0f,75.0f);
        return;
    }
    if(f->caught_by>=0 && !f->dead){
        /* The catcher owns position/frame progression via cpoint. The caught
           fighter cannot independently walk/attack until released. */
        f->vx=f->vy=f->vz=0.0f;
        return;
    }

    /* A dead fighter still has physics and a complete knock-down sequence.
       v0.64 stopped simulating the whole match as soon as HP reached zero,
       which left corpses suspended in the exact hit pose. */
    if(f->dead){
        f->frozen_ticks=0;f->burn_ticks=0;f->sonata_ticks=0;
        if(f->y<0.0f||f->vy!=0.0f||f->falling){
            if(!f->falling){f->falling=true;if(f->def.frames[180].present)set_frame(f,180);}
            fighter_physics_step(f);
            if(f->falling)update_animation(f,NULL);
        } else {
            /* Keep the final lying frame; do not let 230/231 walk through
               frame 219 back into a standing pose after death. */
            if(f->frame!=230&&f->frame!=231){
                int lie=(f->facing&&f->def.frames[231].present)?231:230;
                if(f->def.frames[lie].present)set_frame(f,lie);
            }
        }
        return;
    }

    if(f->sonata_ticks>0){
        f->sonata_ticks--;f->status_age++;
        /* Hover/lift toward roughly one character height above the floor. */
        if(f->y>-118.0f)f->y-=2.35f;
        if(f->y<-118.0f)f->y=-118.0f;
        f->vx*=0.70f;f->vz*=0.75f;f->vy=0.0f;
        int fr=180+((f->status_age/2)&3);
        if(f->def.frames[fr].present&&f->frame!=fr)set_frame(f,fr);
        if(f->sonata_ticks==0){
            f->falling=true;f->vy=0.0f;
            if(f->def.frames[180].present)set_frame(f,180);
            lf2_logf("STATUS","%s sonata released y=%.1f",f->def.name,f->y);
        }
        f->x=clampf(f->x,g_world_min_x,g_world_max_x);f->z=clampf(f->z,-70.0f,75.0f);
        return;
    }

    if(f->frozen_ticks>0){
        f->frozen_ticks--;f->status_age++;
        /* First three ticks use the ice impact frame, then the authored
           frozen frame. A frozen airborne fighter still falls as a block. */
        if(f->status_age==3&&f->def.frames[201].present)set_frame(f,201);
        if(f->y<0.0f||f->vy!=0.0f)fighter_physics_step(f);
        if(f->frozen_ticks==0){
            f->falling=true;
            if(f->def.frames[202].present)set_frame(f,202);
            else if(f->def.frames[182].present)set_frame(f,182);
            if(f->vy==0.0f)f->vy=-3.0f;
            lf2_logf("STATUS","%s thaw -> falling",f->def.name);
        }
        return;
    }

    if(f->burn_ticks>0){
        f->burn_ticks--;f->status_age++;
        fighter_physics_step(f);
        update_animation(f,NULL); /* 203<->204 or 205<->206 while burning */
        if(f->burn_ticks==0){
            f->falling=true;
            if(f->def.frames[180].present)set_frame(f,180);
            if(f->vy==0.0f)f->vy=-5.0f;
            lf2_logf("STATUS","%s burn ended -> falling",f->def.name);
        }
        return;
    }

    bool locked=in_action(f)||f->frame==110||f->y<0.0f;
    if(!locked){
        int cmd=resolve_command_frame(f,in);
        if(cmd>0)start_special_frame(f,cmd);
        else if(in->attack_pressed)start_attack(f);
        else if(in->defend_pressed)set_frame(f,110);
        else if(in->jump_pressed){
            set_frame(f,210);f->vy=f->def.jump_height;
            if(in->left)f->vx=-f->def.jump_distance;if(in->right)f->vx=f->def.jump_distance;
            if(in->up)f->vz=-f->def.jump_distancez;if(in->down)f->vz=f->def.jump_distancez;
        } else {
            bool heavy=f->held_oid&&item_is_heavy(f->held_oid);
            bool running=in->run&&f->def.frames[heavy?16:9].present;
            float sx=heavy?(running?f->def.heavy_running_speed:f->def.heavy_walking_speed):(running?f->def.running_speed:f->def.walking_speed);
            float sz=heavy?(running?f->def.heavy_running_speedz:f->def.heavy_walking_speedz):(running?f->def.running_speedz:f->def.walking_speedz);bool moving=false;
            if(in->left){f->x-=sx;f->facing=1;moving=true;}if(in->right){f->x+=sx;f->facing=0;moving=true;}
            if(in->up){f->z-=sz;moving=true;}if(in->down){f->z+=sz;moving=true;}
            if(moving){
                f->walk_phase++;int first=heavy?(running?16:12):(running?9:5),last=heavy?(running?18:15):(running?11:8),count=last-first+1;
                float rate=running?f->def.running_frame_rate:f->def.walking_frame_rate;int period=(int)(rate+0.5f);if(period<1)period=1;
                int wf=first+(f->walk_phase/period)%count;if(f->frame<first||f->frame>last||f->frame!=wf)set_frame(f,wf);
            }else if(heavy&&f->frame>=16&&f->frame<=18&&f->def.frames[19].present)set_frame(f,19);
            else if((f->frame>=5&&f->frame<=19))set_frame(f,0);
        }
    }else if(f->frame==110&&!in->defend&&f->ticks>4)set_frame(f,0);

    fighter_physics_step(f);
    /* Keep regeneration deliberately slower than the continuous costs. */
    if(f->mp<500 && ++f->mp_regen_tick>=2){f->mp++;f->mp_regen_tick=0;}
    update_animation(f,in);
}

static bool rect_overlap_i(int ax,int ay,int aw,int ah,int bx,int by,int bw,int bh) {
    return lf2_rect_overlap(ax,ay,(unsigned)aw,(unsigned)ah,bx,by,(unsigned)bw,(unsigned)bh);
}

static void region_world(const fighter_t *f,int rx,int ry,int rw,int rh,int *x,int *y) {
    lf2_frame_def_t *fr=(lf2_frame_def_t*)&f->def.frames[f->frame];
    if (!f->facing) *x=(int)f->x-fr->centerx+rx;
    else *x=(int)f->x+1+fr->centerx-rw-rx;
    *y=(int)f->y-fr->centery+ry;
}

static void cpoint_world_xy(const fighter_t *f,const lf2_cpoint_def_t *cp,float *x,float *y){
    if(!f||!cp||!x||!y)return;const lf2_frame_def_t *fr=&f->def.frames[f->frame];
    *x=!f->facing?f->x-(float)fr->centerx+(float)cp->x:f->x+(float)fr->centerx-(float)cp->x;
    *y=f->y-(float)fr->centery+(float)cp->y;
}

static void release_caught_pair(fighter_t *fighters,int total,int holder_idx,int victim_idx){
    if(!fighters||holder_idx<0||holder_idx>=total||victim_idx<0||victim_idx>=total)return;
    fighter_t *h=&fighters[holder_idx],*v=&fighters[victim_idx];
    if(h->holding_slot==victim_idx)h->holding_slot=-1;
    if(v->caught_by==holder_idx)v->caught_by=-1;
}

static int caught_is_hurtable(fighter_t *fighters,int total,int victim_idx,int attacker_idx){
    if(!fighters||victim_idx<0||victim_idx>=total)return 1;fighter_t *v=&fighters[victim_idx];
    if(v->caught_by<0||v->caught_by>=total)return 1;
    if(attacker_idx==v->caught_by)return 0; /* catch damage is cpoint-driven */
    fighter_t *h=&fighters[v->caught_by];lf2_frame_def_t *hf=frame_def(h);
    return hf&&hf->cpoint.present&&hf->cpoint.kind==1&&hf->cpoint.hurtable!=0;
}

static void sync_catch_pair(fighter_t *fighters,int total,int holder_idx){
    if(!fighters||holder_idx<0||holder_idx>=total)return;fighter_t *h=&fighters[holder_idx];int vi=h->holding_slot;
    if(vi<0||vi>=total)return;fighter_t *v=&fighters[vi];
    if(!h->active||h->dead||!v->active||v->dead||v->caught_by!=holder_idx){release_caught_pair(fighters,total,holder_idx,vi);return;}
    lf2_frame_def_t *hf=frame_def(h);if(!hf||!hf->cpoint.present||hf->cpoint.kind!=1){release_caught_pair(fighters,total,holder_idx,vi);return;}
    lf2_cpoint_def_t *hc=&hf->cpoint;
    if(hc->vaction){int va=hc->vaction<0?-hc->vaction:hc->vaction;if(va>0&&va<LF2_MAX_FRAMES&&v->def.frames[va].present&&v->frame!=va)set_frame_signed(v,hc->vaction);}
    lf2_frame_def_t *vf=frame_def(v);float tx=h->x+(h->facing?-36.0f:36.0f),ty=h->y-38.0f;
    cpoint_world_xy(h,hc,&tx,&ty);
    if(vf&&vf->cpoint.present&&vf->cpoint.kind==2){
        lf2_cpoint_def_t *vc=&vf->cpoint;
        v->x=!v->facing?tx+(float)vf->centerx-(float)vc->x:tx-(float)vf->centerx+(float)vc->x;
        v->y=ty+(float)vf->centery-(float)vc->y;
    }else{v->x=tx;v->y=ty;}
    v->z=h->z;v->vx=v->vy=v->vz=0.0f;
}

static void sync_all_catches(fighter_t *fighters,int total){for(int i=0;i<total;i++)if(fighters[i].active&&fighters[i].holding_slot>=0)sync_catch_pair(fighters,total,i);}

static void handle_catch_input(fighter_t *fighters,int total,int holder_idx,const input_t *in){
    if(!fighters||!in||holder_idx<0||holder_idx>=total)return;fighter_t *h=&fighters[holder_idx];int vi=h->holding_slot;
    if(vi<0||vi>=total||fighters[vi].caught_by!=holder_idx)return;lf2_frame_def_t *hf=frame_def(h);
    if(!hf||!hf->cpoint.present||hf->cpoint.kind!=1)return;lf2_cpoint_def_t *cp=&hf->cpoint;
    if(cp->dircontrol){if(in->left)h->facing=1;else if(in->right)h->facing=0;}
    int dir=in->left||in->right||in->up||in->down;
    if(in->attack_pressed&&dir&&cp->taction)set_frame_signed(h,cp->taction);
    else if(in->attack_pressed&&cp->aaction)set_frame_signed(h,cp->aaction);
    else if(in->jump_pressed&&cp->jaction)set_frame_signed(h,cp->jaction);
}

static void process_cpoint_event(fighter_t *fighters,int total,int holder_idx){
    if(!fighters||holder_idx<0||holder_idx>=total)return;fighter_t *h=&fighters[holder_idx];int vi=h->holding_slot;
    if(vi<0||vi>=total)return;fighter_t *v=&fighters[vi];if(v->caught_by!=holder_idx)return;
    lf2_frame_def_t *hf=frame_def(h);if(!hf||!hf->cpoint.present||hf->cpoint.kind!=1)return;lf2_cpoint_def_t *cp=&hf->cpoint;
    if(cp->injury>0){int old=v->hp;v->hp-=cp->injury;if(v->hp<0)v->hp=0;if(v->hp==0)v->dead=true;lf2_logf("GRAB","%s catch hit %s hp=%d->%d frame=%d",h->def.name,v->def.name,old,v->hp,h->frame);}
    if(v->dead){release_caught_pair(fighters,total,holder_idx,vi);return;}
    if(cp->throwvx||cp->throwvy){
        if(cp->vaction)set_frame_signed(v,cp->vaction);
        if(cp->throwinjury>0){v->hp-=cp->throwinjury;if(v->hp<0)v->hp=0;if(v->hp==0)v->dead=true;}
        v->falling=true;v->thrown_by=holder_idx;v->vx=(float)(h->facing?-cp->throwvx:cp->throwvx);v->vy=(float)cp->throwvy;
        if(cp->throwvz){int z=abs(cp->throwvz);v->vz=(float)((rand()%(z*2+1))-z);}else v->vz=0.0f;
        release_caught_pair(fighters,total,holder_idx,vi);
        lf2_logf("GRAB","%s threw %s frame=%d vel=%.1f,%.1f,%.1f injury=%d",h->def.name,v->def.name,h->frame,v->vx,v->vy,v->vz,cp->throwinjury);
    }
}

static int are_enemies(const fighter_t *a,const fighter_t *b){
    if(!a||!b||a==b)return 0;
    /* Independent fighters are hostile to everybody. Team members only spare
       fighters that share the same numbered team. */
    return a->team==0 || b->team==0 || a->team!=b->team;
}

static const object_catalog_entry_t *object_catalog_find(int oid){
    for(size_t i=0;i<sizeof(g_object_catalog)/sizeof(g_object_catalog[0]);i++)
        if(g_object_catalog[i].oid==oid)return &g_object_catalog[i];
    return NULL;
}

static object_cache_entry_t *object_cache_get(object_world_t *w,int oid){
    if(!w)return NULL;
    for(int i=0;i<w->cache_count;i++)if(w->cache[i].used&&w->cache[i].oid==oid)return &w->cache[i];
    const object_catalog_entry_t *cat=object_catalog_find(oid);
