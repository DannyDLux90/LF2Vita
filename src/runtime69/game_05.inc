
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
                if(a->roster_idx==3)a->transform_target=b->roster_idx;
                lf2_logf("GRAB","%s caught %s catching=%d caught=%d transform_target=%d",a->def.name,b->def.name,ca,cb,a->transform_target);
                return;
            }
            if(it->kind==10||it->kind==11){
                apply_sonata(b,it,it->kind);
                return;
            }
            if(it->kind==15){
                b->vx+=(a->x<b->x)?2.5f:-2.5f;b->vy=-2.0f;b->falling=true;
                if(b->def.frames[180].present)set_frame(b,180);
                lf2_logf("STATUS","%s wind lift by %s",b->def.name,a->def.name);
                return;
            }
            if(it->kind==16){
                apply_elemental_status(b,3,a->facing,it);
                if(it->arest>0)a->arest_ticks=it->arest;if(it->vrest>0)a->vrest_ticks[bidx]=it->vrest;
                return;
            }

            int effect=it->effect;
            int dmg=it->injury>0?it->injury:18;
            /* Freeze's summoned Ice Sword is a held-weapon mechanic in the
               PC executable. The Vita runtime keeps the sword attached and
               gives melee contact its ice property while it is equipped. */
            if(a->ice_sword_ticks>0 && effect==0){effect=3;dmg+=10;}
            if(a->ai){
                if(difficulty==LF2_DIFF_EASY)dmg=(dmg*4)/5;
                else if(difficulty==LF2_DIFF_DIFFICULT)dmg=(dmg*11)/10;
                else if(difficulty==LF2_DIFF_CRAZY)dmg=(dmg*5)/4;
            }
            if (b->frame==110) dmg=(dmg+3)/4;
            if(dmg<1)dmg=1;
            b->hp-=dmg; if(b->hp<0)b->hp=0;
            if(b->hp==0)b->dead=true;
            int status_applied=(!b->dead)?apply_elemental_status(b,effect,a->facing,it):0;
            if(!status_applied&&!apply_caught_hurt_frame(b,a->facing))apply_hit_reaction(b,a->facing,it);
            play_hit_sound(effect,it->fall?it->fall:20,dmg);
            spawn_hit_fx(world,b,effect);
            if(it->arest>0)a->arest_ticks=it->arest;if(it->vrest>0)a->vrest_ticks[bidx]=it->vrest;
            return;
        }
    }
}
static void draw_character_frame(const lf2_character_def_t *def,int frame,int facing,
                                 float wx,float wy,float wz,int hit_flash,const char *name) {
    if(!def||frame<0||frame>=LF2_MAX_FRAMES||!def->frames[frame].present)return;
    lf2_frame_def_t *fr=(lf2_frame_def_t*)&def->frames[frame];
    lf2_sheet_def_t *s=lf2_sheet_for_pic((lf2_character_def_t*)def,fr->pic);if(!s||!s->texture)return;

    int local=fr->pic-s->first_pic;
    int across=s->row>0?s->row:1;
    int down=s->col>0?s->col:1;
    int col=local%across,row=local/across;
    if(local<0||col<0||col>=across||row<0||row>=down){
        static int warned=0;
        if(warned<12){lf2_logf("RENDER","invalid sprite cell char=%s frame=%d pic=%d local=%d layout=%dx%d",name?name:def->name,frame,fr->pic,local,across,down);warned++;}
        return;
    }

    int pitchx=s->w+1,pitchy=s->h+1;
    float tx=(float)(col*pitchx),ty=(float)(row*pitchy);
    float sw=(float)s->w,sh=(float)s->h;
    float tw=(float)vita2d_texture_get_width(s->texture),th=(float)vita2d_texture_get_height(s->texture);
    if(sw<=0||sh<=0||tw<=0||th<=0)return;
    if(tx+sw>tw || ty+sh>th){
        static int bounds_warned=0;
        if(bounds_warned<16){lf2_logf("RENDER","sprite rect outside bitmap char=%s frame=%d pic=%d rect=%.0f,%.0f %.0fx%.0f tex=%.0fx%.0f",name?name:def->name,frame,fr->pic,tx,ty,sw,sh,tw,th);bounds_warned++;}
        return;
    }

    float u0=(tx+0.5f)/tw,u1=(tx+sw-0.5f)/tw;
    float v0=(ty+0.5f)/th,v1=(ty+sh-0.5f)/th;
    if(facing){float t=u0;u0=u1;u1=t;}
    float anchor_x=facing?((sw-1.0f)-(float)fr->centerx):(float)fr->centerx;
    float x=floorf(screen_x(wx)-anchor_x*SPRITE_SCALE+0.5f);
    float y=floorf(GROUND_Y+wz+wy-(float)fr->centery*SPRITE_SCALE+0.5f);
    float x1=x+sw*SPRITE_SCALE,y1=y+sh*SPRITE_SCALE;

    vita2d_texture_vertex *v=(vita2d_texture_vertex*)vita2d_pool_memalign(6*sizeof(*v),sizeof(*v));
    if(!v){
        static int pool_warned=0;
        if(!pool_warned){lf2_logf("ERROR","vita2d vertex pool exhausted while drawing %s",name?name:def->name);pool_warned=1;}
        return;
    }
    v[0]=(vita2d_texture_vertex){x, y, 0.5f, u0, v0};
    v[1]=(vita2d_texture_vertex){x, y1,0.5f, u0, v1};
    v[2]=(vita2d_texture_vertex){x1,y, 0.5f, u1, v0};
    v[3]=(vita2d_texture_vertex){x, y1,0.5f, u0, v1};
    v[4]=(vita2d_texture_vertex){x1,y1,0.5f, u1, v1};
    v[5]=(vita2d_texture_vertex){x1,y, 0.5f, u1, v0};
    unsigned tint=hit_flash?RGBA8(255,205,205,255):RGBA8(255,255,255,255);
    vita2d_draw_array_textured(s->texture,SCE_GXM_PRIMITIVE_TRIANGLES,v,6,tint);
}
