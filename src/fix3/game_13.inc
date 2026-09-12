                    lf2_bdy_def_t *bd=&pf->bdys[bi];int px,py;object_region_world(p,bd->x,bd->y,bd->w,bd->h,&px,&py);
                    if(rect_overlap_i(sx,sy,it->w,it->h,px,py,bd->w,bd->h))reflected=1;
                }
                if(!reflected)continue;
                int old_owner=p->owner;p->owner=shield->owner;p->team=shield->team;p->target_slot=-1;p->facing^=1;
                if(p->cache->def.frames[30].present)object_set_frame(p,30);else p->active=false;
                lf2_logf("OBJECT","reflect shield_oid=%d projectile_oid=%d owner=%d->%d frame=%d",
                         shield->oid,p->oid,old_owner,p->owner,p->active?p->frame:-1);
            }
        }
    }
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
            if(it->kind!=0&&it->kind!=5&&it->kind!=8&&it->kind!=9&&it->kind!=10&&it->kind!=11&&it->kind!=14&&it->kind!=15&&it->kind!=16)continue;
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
                if(it->kind==14){
                    /* Solid 3D obstacle (Freeze column / heavy-object style).
                       Resolve the smallest horizontal penetration and cancel
                       movement into the obstacle; kind14 itself deals no HP. */
                    int left_push=(ax+it->w)-bx;
                    int right_push=(bx+bd->w)-ax;
                    if(left_push>0&&right_push>0){
                        if(left_push<right_push){b->x+=(float)left_push+0.5f;if(b->vx<0)b->vx=0;}
                        else {b->x-=(float)right_push+0.5f;if(b->vx>0)b->vx=0;}
                        b->x=clampf(b->x,g_world_min_x,g_world_max_x);
                    }
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
                    int authored=it->injury>0?it->injury:0;
                    int dmg=g_runtime_stock_compat?authored:(it->injury>0?it->injury:1);
                    if(!g_runtime_stock_compat&&o->owner>=0&&o->owner<total&&fighters[o->owner].ai){
                        if(difficulty==LF2_DIFF_EASY)dmg=dmg*4/5;
                        else if(difficulty==LF2_DIFF_DIFFICULT)dmg=dmg*11/10;
                        else if(difficulty==LF2_DIFF_CRAZY)dmg=dmg*5/4;
                    }
                    int defended=stock_try_defend_hit(b,o->facing,it,authored,&dmg);
                    if(!g_runtime_stock_compat&&b->frame==110)dmg=(dmg+3)/4;
                    if(!g_runtime_stock_compat&&dmg<1)dmg=1;
                    int old=b->hp;b->hp-=dmg;if(b->hp<0)b->hp=0;
                    if(b->hp==0)b->dead=true;
                    if(!defended){
                        stock_note_unblocked_hit(b);
                        int status_applied=(!b->dead)?apply_elemental_status(b,it->effect,o->facing,it):0;
                        if(!status_applied&&!apply_caught_hurt_frame(b,o->facing))apply_hit_reaction(b,o->facing,it);
                        play_hit_sound(it->effect,it->fall?it->fall:20,dmg);spawn_hit_fx(w,b,it->effect);
                    }else if(b->dead){
                        apply_hit_reaction(b,o->facing,it);
                    }
                    lf2_logf("OBJECT","hit oid=%d owner=%d target=%s hp=%d->%d effect=%d state=%d defend=%d",o->oid,o->owner,b->def.name,old,b->hp,it->effect,of->state,defended);
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
/* Stock weapon victims are real objects with bdy regions. Heavy weapons on
   the ground can be punched; light/throw/drink objects on the ground are only
   attackable by another held weapon. Airborne loose weapons may be intercepted. */
static int item_accepts_attack(const world_object_t *target,const lf2_itr_def_t *it,int attacker_is_held_weapon){
    if(!target||!target->active||!target->cache||!it||target->attached||(!target->pickup_item&&!target->recoverable_weapon))return 0;
    lf2_frame_def_t *tf=object_frame_def((world_object_t*)target);if(!tf||tf->bdy_count<=0)return 0;
    if(target->type==2){
        if(target->grounded||tf->state==2004)return 1;
        return tf->state==2000&&it->fall>=60;
    }
    if(target->grounded)return attacker_is_held_weapon;
    return 1;
}

static int item_itr_overlaps(world_object_t *target,const lf2_itr_def_t *it,int ax,int ay,float attacker_z){
    if(!target||!it)return 0;lf2_frame_def_t *tf=object_frame_def(target);if(!tf)return 0;
    int zw=it->zwidth?it->zwidth:14;if(fabsf(attacker_z-target->z)>=zw)return 0;
    for(int bi=0;bi<tf->bdy_count;bi++){
        lf2_bdy_def_t *bd=&tf->bdys[bi];int bx,by;object_region_world(target,bd->x,bd->y,bd->w,bd->h,&bx,&by);
        if(rect_overlap_i(ax,ay,it->w,it->h,bx,by,bd->w,bd->h))return 1;
    }
    return 0;
}

static void item_react_to_hit(world_object_t *target,const lf2_itr_def_t *it,int facing){
    if(!target||!target->active||!it)return;
    if(target->type==2&&target->grounded){
        if(it->fall>=60){target->grounded=false;target->pickup_item=false;target->recoverable_weapon=true;target->vx=(float)(facing?-3.0f:3.0f);target->vy=-3.7f;target->vz=0.0f;object_set_frame(target,0);}
        else if(it->fall>=30){target->grounded=false;target->pickup_item=false;target->recoverable_weapon=true;target->vy=-2.0f;object_set_frame(target,0);}
    }else if(!target->grounded){
        target->vx*=-0.4f;target->vy*=-2.0f;target->vz*=-0.4f;
    }
}

static void object_apply_hits_to_items(object_world_t *w,world_object_t *attacker){
    if(!w||!attacker||!attacker->active||attacker->attached||attacker->pickup_item||attacker->arest_ticks>0)return;
    lf2_frame_def_t *af=object_frame_def(attacker);if(!af||af->itr_count<=0)return;
    for(int ii=0;ii<af->itr_count;ii++){
        lf2_itr_def_t *it=&af->itrs[ii];if(it->kind!=0&&it->kind!=5&&it->kind!=9)continue;
        int ax,ay;object_region_world(attacker,it->x,it->y,it->w,it->h,&ax,&ay);
        for(int oi=0;oi<LF2_MAX_ACTIVE_OBJECTS;oi++){
            world_object_t *target=&w->objects[oi];if(target==attacker||!item_accepts_attack(target,it,0))continue;
            if(!item_itr_overlaps(target,it,ax,ay,attacker->z))continue;
            if(item_damage_object(target,it->injury,it->bdefend,"object attack"))return;
            item_react_to_hit(target,it,attacker->facing);
            if(it->arest>0)attacker->arest_ticks=it->arest;
            return;
        }
    }
}

static void world_clear_active(object_world_t *w){if(!w)return;memset(w->objects,0,sizeof(w->objects));memset(w->fx,0,sizeof(w->fx));}
static void world_shutdown(object_world_t *w){if(!w)return;vita2d_wait_rendering_done();for(int i=0;i<w->cache_count;i++)if(w->cache[i].used)lf2_free_character_textures(&w->cache[i].def);memset(w,0,sizeof(*w));}

static int apply_held_weapon_hit(object_world_t *world,fighter_t *a,fighter_t *b,int aidx,int bidx,int difficulty){
    if(!world||!a||!b||!a->held_oid||item_is_bottle(a->held_oid)||a->arest_ticks>0||bidx<0||bidx>=LF2_MAX_ACTORS||a->vrest_ticks[bidx]>0)return 0;
    lf2_frame_def_t *af=frame_def(a),*bf=frame_def(b);
    if(!af||!bf||bf->bdy_count<=0||!af->wpoint.present||af->wpoint.attacking<=0)return 0;
