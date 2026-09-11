        o->x=a->x+(a->facing?-o->attach_dx:o->attach_dx);o->y=a->y+o->attach_dy;o->z=a->z+o->attach_dz;
        return;
    }
    /* hit_Fa on a ball is LF2's chase flag. Apply a conservative homing turn
       rather than teleporting so Dennis/John-style chase objects remain visible. */
    if(o->type==3&&fr->hit_Fa>0){fighter_t *t=object_nearest_enemy(o,fighters,total);if(t){float dx=t->x-o->x,dy=(t->y-35.0f)-o->y,dz=t->z-o->z;float len=sqrtf(dx*dx+dy*dy+dz*dz);if(len>1.0f){float speed=sqrtf(o->vx*o->vx+o->vy*o->vy+o->vz*o->vz);if(speed<4.0f)speed=7.0f;float turn=fr->hit_Fa>=7?0.20f:0.11f;o->vx=o->vx*(1.0f-turn)+dx/len*speed*turn;o->vy=o->vy*(1.0f-turn)+dy/len*speed*turn;o->vz=o->vz*(1.0f-turn)+dz/len*speed*turn;}}}
    o->x+=o->vx;o->y+=o->vy;o->z+=o->vz;
    if(o->type==1&&fr->state==1002)o->vy+=0.45f;
    if(o->type==2&&fr->state==2000)o->vy+=0.45f;
    if(o->type==1&&o->y>=2.0f&&fr->state==1002){
        o->y=0.0f;o->vx=o->vy=o->vz=0.0f;
        if(o->recoverable_weapon){o->pickup_item=true;o->grounded=true;o->owner=-1;o->team=0;object_set_frame(o,object_landing_frame(o->cache));}
        else if(o->cache->def.frames[60].present)object_set_frame(o,60);else o->active=false;
    }
    if(o->type==2&&o->y>=2.0f&&fr->state==2000){
        o->y=0.0f;o->vx=o->vy=o->vz=0.0f;
        if(o->recoverable_weapon){o->pickup_item=true;o->grounded=true;o->owner=-1;o->team=0;object_set_frame(o,object_landing_frame(o->cache));}
        else if(o->cache->def.frames[20].present)object_set_frame(o,20);else o->active=false;
    }
    if(!o->active)return;
    if(o->x<-180||o->x>1140||o->z<-180||o->z>180||o->y>180||o->age>420){o->active=false;return;}
    fr=object_frame_def(o);if(!fr)return;o->ticks++;int duration=fr->wait+1;if(duration<1)duration=1;if(o->ticks<duration)return;
    int next=fr->next;if(next<0){o->facing^=1;next=-next;}
    if(next==999)next=0;else if(next==1000){o->active=false;return;}
    object_set_frame(o,next);
}

static void spawn_hit_fx(object_world_t *w,const fighter_t *victim,int effect){
    if(!w||!victim)return;int kind=(effect==1)?1:((effect==2||effect==20||effect==21||effect==22)?2:((effect==3||effect==30)?3:0));
    int count=kind==1?10:6;for(int n=0;n<count;n++){hit_fx_t *fx=NULL;for(int i=0;i<LF2_MAX_HIT_FX;i++)if(!w->fx[i].active){fx=&w->fx[i];break;}if(!fx)break;
        fx->active=true;fx->kind=kind;fx->x=victim->x+(float)((rand()%17)-8);fx->y=victim->y-43.0f+(float)((rand()%17)-8);fx->z=victim->z;
        fx->vx=(float)((rand()%101)-50)/18.0f;fx->vy=-(float)(25+rand()%60)/14.0f;fx->life=kind==1?18:12;}
}

static void play_hit_sound(int effect,int fall,int injury){
    const char *snd="data/001.wav";
    if(effect==1)snd=(fall>=60||injury>=60)?"data/033.wav":"data/032.wav";
    else if(effect==2||effect==20||effect==21||effect==22)snd=(injury>=60)?"data/071.wav":"data/070.wav";
    else if(effect==3||effect==30)snd="data/065.wav";
    else snd=(fall>=60||injury>=60)?"data/006.wav":"data/001.wav";
    lf2_audio_play(snd);
}

static int is_fire_effect(int effect){return effect==2||effect==20||effect==21||effect==22;}
static int is_ice_effect(int effect){return effect==3||effect==30;}

static int apply_elemental_status(fighter_t *b,int effect,int attacker_facing,const lf2_itr_def_t *it){
    if(!b||!it)return 0;
    if(is_ice_effect(effect)){
        b->burn_ticks=0;b->sonata_ticks=0;b->status_age=0;
        b->frozen_ticks=94; /* 200 intro + 201's authored ~90 TU hold */
        b->falling=false;b->vx=0;b->vz=0;
        if(b->def.frames[200].present)set_frame(b,200);
        lf2_logf("STATUS","%s frozen ticks=%d effect=%d",b->def.name,b->frozen_ticks,effect);
        return 1;
    }
    if(is_fire_effect(effect)){
        b->frozen_ticks=0;b->sonata_ticks=0;b->status_age=0;
        b->burn_ticks=26;
        b->falling=false;
        float push=(float)(it->dvx?it->dvx:6);
        b->vx=attacker_facing?-fabsf(push):fabsf(push);
        b->vy=(float)(it->dvy?it->dvy:-6);
        int fr=(b->facing==attacker_facing)?203:205;
        if(!b->def.frames[fr].present)fr=b->def.frames[203].present?203:220;
        set_frame(b,fr);
        lf2_logf("STATUS","%s burning ticks=%d effect=%d frame=%d",b->def.name,b->burn_ticks,effect,fr);
        return 1;
    }
    return 0;
}

/* Henry's Sonata of Death uses itr kinds 10/11 rather than ordinary injury
   boxes. Kind 10 lifts/holds targets; kind 11 maintains already affected
   targets. The short refresh timer means gravity resumes naturally when the
   flute's active frame ends. */
static void apply_sonata(fighter_t *b,const lf2_itr_def_t *it,int kind){
    if(!b||!it||b->dead)return;
    if(kind==11 && b->sonata_ticks<=0)return;
    b->frozen_ticks=0;b->burn_ticks=0;
    b->sonata_ticks=5;b->falling=false;b->status_age++;
    if(b->sonata_damage_cooldown<=0 && kind==10){
        int dmg=it->injury>0?it->injury:1;
        int old=b->hp;b->hp-=dmg;if(b->hp<0)b->hp=0;
        b->sonata_damage_cooldown=4;
        if(b->hp==0)b->dead=true;
        lf2_logf("STATUS","%s sonata hp=%d->%d",b->def.name,old,b->hp);
    }
    if(!b->dead){
        /* LF2 shows victims in a falling-like pose while the flute carries
           them upward. Keep the pose moving so it never looks frozen. */
        int fr=180+((b->status_age/2)&3);
        if(b->def.frames[fr].present && b->frame!=fr)set_frame(b,fr);
        b->vy=0.0f;b->vx*=0.70f;b->vz*=0.75f;
    }
}

static void apply_hit_reaction(fighter_t *b,int attacker_facing,const lf2_itr_def_t *it){
    if(!b||!it)return;int fall=it->fall?it->fall:20;if(fall>0)b->fall_points+=fall;
    int reaction=220;bool airborne=b->y<0.0f||b->vy!=0.0f;
    if(b->hp<=0||fall>60||b->fall_points>60){reaction=180;b->falling=true;b->fall_points=0;}
    else if(airborne&&fall>20){reaction=180;b->falling=true;}
    else if(b->fall_points>40){reaction=b->def.frames[226].present?226:220;b->fall_points=60;}
    else if(b->fall_points>20){bool from_front=(b->facing!=(attacker_facing?1:0));reaction=from_front&&b->def.frames[222].present?222:(b->def.frames[224].present?224:220);b->fall_points=40;}
    else if(fall<0)return;else b->fall_points=20;
    set_frame(b,reaction);
    float push=(float)(it->dvx?it->dvx:3);b->vx=(attacker_facing?-push:push);
    if(it->dvy)b->vy=(float)it->dvy;else if(b->falling)b->vy=-7.5f;
    b->hit_flash=5;
}

static void update_hit_fx(object_world_t *w){if(!w)return;for(int i=0;i<LF2_MAX_HIT_FX;i++){hit_fx_t *f=&w->fx[i];if(!f->active)continue;f->x+=f->vx;f->y+=f->vy;f->vy+=0.42f;f->life--;if(f->life<=0)f->active=false;}}
static void draw_hit_fx(object_world_t *w){if(!w)return;for(int i=0;i<LF2_MAX_HIT_FX;i++){hit_fx_t *f=&w->fx[i];if(!f->active)continue;unsigned c=f->kind==1?RGBA8(175,0,12,230):(f->kind==2?RGBA8(255,120,22,230):(f->kind==3?RGBA8(90,200,255,235):RGBA8(255,245,190,235)));float sz=f->kind==1?3.0f:4.0f;vita2d_draw_rectangle(screen_x(f->x),GROUND_Y+f->z+f->y,sz,sz,c);}}

static int apply_caught_hurt_frame(fighter_t *b,int attacker_facing){
    if(!b||b->caught_by<0)return 0;lf2_frame_def_t *bf=frame_def(b);
    if(!bf||!bf->cpoint.present||bf->cpoint.kind!=2)return 0;
    int front=(b->facing!=(attacker_facing?1:0));
    int act=front?bf->cpoint.fronthurtact:bf->cpoint.backhurtact;
    if(act>0&&act<LF2_MAX_FRAMES&&b->def.frames[act].present){set_frame(b,act);return 1;}
    return 0;
}

static void object_apply_hits(object_world_t *w,world_object_t *o,fighter_t *fighters,int total,int difficulty){
    if(!o||!o->active||o->attached||o->pickup_item)return;lf2_frame_def_t *of=object_frame_def(o);if(!of||of->itr_count<=0)return;
    for(int target=0;target<total;target++){
        fighter_t *b=&fighters[target];if(b->dead||o->arest_ticks>0||o->vrest_ticks[target]>0)continue;
        if(o->team!=0&&b->team==o->team)continue;
        lf2_frame_def_t *bf=frame_def(b);if(!bf||bf->bdy_count<=0)continue;
        if(!caught_is_hurtable(fighters,total,target,o->owner))continue;
        for(int ii=0;ii<of->itr_count;ii++){
            lf2_itr_def_t *it=&of->itrs[ii];
            if(it->kind!=0&&it->kind!=5&&it->kind!=8&&it->kind!=9&&it->kind!=10&&it->kind!=11&&it->kind!=15&&it->kind!=16)continue;
            bool healing=(it->kind==8);
            if(healing){if(target!=o->owner&&(o->team==0||b->team!=o->team))continue;}
            else {if(target==o->owner)continue;if(o->team!=0&&b->team==o->team)continue;}
            int zw=it->zwidth?it->zwidth:20;if(fabsf(o->z-b->z)>=zw)continue;
            int ax,ay;object_region_world(o,it->x,it->y,it->w,it->h,&ax,&ay);
            for(int j=0;j<bf->bdy_count;j++){
                lf2_bdy_def_t *bd=&bf->bdys[j];int bx,by;region_world(b,bd->x,bd->y,bd->w,bd->h,&bx,&by);
                if(!rect_overlap_i(ax,ay,it->w,it->h,bx,by,bd->w,bd->h))continue;

                if(it->kind==10||it->kind==11){
                    apply_sonata(b,it,it->kind); /* sustained fields do not consume the object hit mask */
                    break;
                }
                if(it->kind==15){
                    b->vx+=(o->x<b->x)?-2.5f:2.5f;b->vy=-2.0f;b->falling=true;
                    if(b->def.frames[180].present)set_frame(b,180);
                    lf2_logf("STATUS","%s wind lift oid=%d",b->def.name,o->oid);break;
                }
                if(it->kind==16){
                    apply_elemental_status(b,3,o->facing,it);
                    if(it->arest>0)o->arest_ticks=it->arest;if(it->vrest>0)o->vrest_ticks[target]=it->vrest;break;
                }

                if(bf->state==12&&!healing&&it->fall<41)continue;
                if(healing){
                    int heal=it->injury>0?it->injury:50;int old=b->hp;b->hp+=heal;if(b->hp>b->max_hp)b->hp=b->max_hp;
                    lf2_logf("OBJECT","heal oid=%d target=%s hp=%d->%d",o->oid,b->def.name,old,b->hp);
                } else {
                    int dmg=it->injury>0?it->injury:1;
                    if(o->owner>=0&&o->owner<total&&fighters[o->owner].ai){
                        if(difficulty==LF2_DIFF_EASY)dmg=dmg*4/5;
                        else if(difficulty==LF2_DIFF_DIFFICULT)dmg=dmg*11/10;
                        else if(difficulty==LF2_DIFF_CRAZY)dmg=dmg*5/4;
                    }
                    if(b->frame==110)dmg=(dmg+3)/4;if(dmg<1)dmg=1;
                    int old=b->hp;b->hp-=dmg;if(b->hp<0)b->hp=0;
                    if(b->hp==0)b->dead=true;
                    int status_applied=(!b->dead)?apply_elemental_status(b,it->effect,o->facing,it):0;
                    if(!status_applied&&!apply_caught_hurt_frame(b,o->facing))apply_hit_reaction(b,o->facing,it);
                    play_hit_sound(it->effect,it->fall?it->fall:20,dmg);spawn_hit_fx(w,b,it->effect);
                    lf2_logf("OBJECT","hit oid=%d owner=%d target=%s hp=%d->%d effect=%d state=%d",o->oid,o->owner,b->def.name,old,b->hp,it->effect,of->state);
                }
                if(it->arest>0)o->arest_ticks=it->arest;if(it->vrest>0)o->vrest_ticks[target]=it->vrest;
                if(o->type==1){
                    if(o->recoverable_weapon){o->vx*=-0.25f;o->vy=-2.0f;if(o->cache->def.frames[0].present)object_set_frame(o,0);return;}
                    o->active=false;return;
                }
                if(of->state==3000){if(o->cache->def.frames[10].present)object_set_frame(o,10);else o->active=false;return;}
                break;
            }
        }
    }
}
static void world_clear_active(object_world_t *w){if(!w)return;memset(w->objects,0,sizeof(w->objects));memset(w->fx,0,sizeof(w->fx));}
static void world_shutdown(object_world_t *w){if(!w)return;vita2d_wait_rendering_done();for(int i=0;i<w->cache_count;i++)if(w->cache[i].used)lf2_free_character_textures(&w->cache[i].def);memset(w,0,sizeof(*w));}

static int apply_held_weapon_hit(object_world_t *world,fighter_t *a,fighter_t *b,int aidx,int bidx,int difficulty){
    if(!world||!a||!b||!a->held_oid||item_is_bottle(a->held_oid)||a->arest_ticks>0||bidx<0||bidx>LF2_MAX_CPUS||a->vrest_ticks[bidx]>0)return 0;
    lf2_frame_def_t *af=frame_def(a),*bf=frame_def(b);
    if(!af||!bf||bf->bdy_count<=0||!af->wpoint.present||af->wpoint.attacking<=0)return 0;
    object_cache_entry_t *c=NULL;int wf_id=0;float wx=0,wy=0;
    if(!held_item_frame_and_pose(world,a,&c,&wf_id,&wx,&wy,NULL)||!c)return 0;
    lf2_frame_def_t *wf=&c->def.frames[wf_id];
    world_object_t tmp;memset(&tmp,0,sizeof(tmp));tmp.active=true;tmp.cache=c;tmp.oid=a->held_oid;tmp.type=c->type;
    tmp.frame=wf_id;tmp.facing=a->facing;tmp.x=wx;tmp.y=wy;tmp.z=a->z;
    for(int ii=0;ii<wf->itr_count;ii++){
        lf2_itr_def_t *it=&wf->itrs[ii];if(it->kind!=5)continue;
        int zw=it->zwidth?it->zwidth:14;if(fabsf(a->z-b->z)>=zw)continue;
        int ax,ay;object_region_world(&tmp,it->x,it->y,it->w,it->h,&ax,&ay);
        for(int j=0;j<bf->bdy_count;j++){
            lf2_bdy_def_t *bd=&bf->bdys[j];int bx,by;region_world(b,bd->x,bd->y,bd->w,bd->h,&bx,&by);
            if(!rect_overlap_i(ax,ay,it->w,it->h,bx,by,bd->w,bd->h))continue;
            int oid=a->held_oid;lf2_itr_def_t resolved=*it;
            int strength_idx=af->wpoint.attacking-1;
            if(it->injury==789&&strength_idx>=0&&strength_idx<4&&c->def.weapon_strength_present[strength_idx]){
                const lf2_itr_def_t *ws=&c->def.weapon_strength[strength_idx];
                resolved.dvx=ws->dvx;resolved.dvy=ws->dvy;resolved.fall=ws->fall;resolved.arest=ws->arest;resolved.vrest=ws->vrest;
                resolved.bdefend=ws->bdefend;resolved.injury=ws->injury;resolved.effect=ws->effect;
            }
            int dmg=resolved.injury>0?resolved.injury:1;
            if(a->ai){
                if(difficulty==LF2_DIFF_EASY)dmg=dmg*4/5;
                else if(difficulty==LF2_DIFF_DIFFICULT)dmg=dmg*11/10;
                else if(difficulty==LF2_DIFF_CRAZY)dmg=dmg*5/4;
            }
            if(b->frame==110)dmg=(dmg+3)/4;if(dmg<1)dmg=1;
            int old=b->hp;b->hp-=dmg;if(b->hp<0)b->hp=0;if(b->hp==0)b->dead=true;
            if(!b->dead)apply_hit_reaction(b,a->facing,&resolved);
            lf2_audio_play(item_hit_sound(oid));spawn_hit_fx(world,b,0);
            if(a->held_units>0)a->held_units--;
            if(a->held_units<=0){
                lf2_logf("ITEM","%s weapon broke oid=%d name=%s",a->def.name,oid,item_name(oid));
                a->held_oid=0;a->held_units=0;lf2_audio_play("data/094.wav");
            }
            if(resolved.arest>0)a->arest_ticks=resolved.arest;
            if(resolved.vrest>0)a->vrest_ticks[bidx]=resolved.vrest;
            lf2_logf("ITEM","weapon hit attacker=%s oid=%d target=%s hp=%d->%d frame=%d",
                     a->def.name,oid,b->def.name,old,b->hp,wf_id);
            return 1;
        }
    }
    return 0;
}

static void apply_hits(object_world_t *world,fighter_t *fighters,int total,int aidx,int bidx,int difficulty) {
    fighter_t *a=&fighters[aidx],*b=&fighters[bidx];
    if (a->dead || b->dead) return;
    lf2_frame_def_t *af=frame_def(a), *bf=frame_def(b);
    int body_attack=0;if(af)for(int ki=0;ki<af->itr_count;ki++)if(af->itrs[ki].kind==4){body_attack=1;break;}
    if(!are_enemies(a,b)&&!body_attack)return;
    if (!af || !bf || bf->bdy_count==0) return;
    if(!caught_is_hurtable(fighters,total,bidx,aidx))return;
    if(apply_held_weapon_hit(world,a,b,aidx,bidx,difficulty))return;
    if(af->itr_count==0)return;
    for (int i=0;i<af->itr_count;i++) {
        lf2_itr_def_t *it=&af->itrs[i];
        int sustained=(it->kind==10||it->kind==11||it->kind==15||it->kind==16);
        int catch_itr=((it->kind==3||it->kind==1) && (it->catchingact>0||it->caughtact>0));
        /* kind:1 is the normal walking grab and only catches a stunned
           state-16 victim. kind:3 is the explicit special grab (Louis). */
        if(it->kind==1&&catch_itr&&bf->state!=16)continue;
        if(it->kind!=0&&it->kind!=4&&it->kind!=5&&it->kind!=6&&!sustained&&!catch_itr)continue;
        if(it->kind==4&&bidx==a->thrown_by)continue;
        /* Original LF2 repeat-hit control: arest blocks this attacker for a
           few TU, while vrest blocks only this attacker/victim pair. There is
           no victim-wide invulnerability timer. */
        if(!sustained&&(a->arest_ticks>0||a->vrest_ticks[bidx]>0))continue;
        int zw=it->zwidth?it->zwidth:14;
        if (fabsf(a->z-b->z)>=zw) continue;
        /* State 12 is intentionally protected from weak hits in PC LF2. This
           is not the old Vita invulnerability bug: only fall>=41 may connect. */
        if(bf->state==12 && it->kind==0 && it->fall<41)continue;
        /* itr kind 6 is passive. A normal punch touching it upgrades to the
           stock super-punch chain (frame 70) instead of dealing this punch. */
        if((a->frame==60||a->frame==65)&&it->kind==0&&a->def.frames[70].present){
            for(int vi=0;vi<bf->itr_count;vi++)if(bf->itrs[vi].kind==6){
                lf2_itr_def_t *v=&bf->itrs[vi];int ax6,ay6,bx6,by6;
                region_world(a,it->x,it->y,it->w,it->h,&ax6,&ay6);region_world(b,v->x,v->y,v->w,v->h,&bx6,&by6);
                if(rect_overlap_i(ax6,ay6,it->w,it->h,bx6,by6,v->w,v->h)){set_frame(a,70);return;}
            }
        }
        int ax,ay; region_world(a,it->x,it->y,it->w,it->h,&ax,&ay);
        for (int j=0;j<bf->bdy_count;j++) {
            lf2_bdy_def_t *bd=&bf->bdys[j]; int bx,by;
            region_world(b,bd->x,bd->y,bd->w,bd->h,&bx,&by);
            if (!rect_overlap_i(ax,ay,it->w,it->h,bx,by,bd->w,bd->h)) continue;

            if(catch_itr){
                if(a->holding_slot>=0||b->caught_by>=0)continue;
                int ca=it->catchingact,cb=it->caughtact;
                if(ca>0&&ca<LF2_MAX_FRAMES&&a->def.frames[ca].present)set_frame(a,ca);
                if(cb>0&&cb<LF2_MAX_FRAMES&&b->def.frames[cb].present)set_frame(b,cb);
                a->holding_slot=bidx;b->caught_by=aidx;b->grabbed_ticks=0;b->vx=b->vy=b->vz=0.0f;b->z=a->z;
                b->x=a->x+(a->facing?-38.0f:38.0f);
