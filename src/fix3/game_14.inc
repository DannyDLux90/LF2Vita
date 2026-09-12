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
            int authored=resolved.injury>0?resolved.injury:0;
            int dmg=g_runtime_stock_compat?authored:(resolved.injury>0?resolved.injury:1);
            if(!g_runtime_stock_compat&&a->ai){
                if(difficulty==LF2_DIFF_EASY)dmg=dmg*4/5;
                else if(difficulty==LF2_DIFF_DIFFICULT)dmg=dmg*11/10;
                else if(difficulty==LF2_DIFF_CRAZY)dmg=dmg*5/4;
            }
            int defended=stock_try_defend_hit(b,a->facing,&resolved,authored,&dmg);
            if(!g_runtime_stock_compat&&b->frame==110)dmg=(dmg+3)/4;
            if(!g_runtime_stock_compat&&dmg<1)dmg=1;
            int old=b->hp;b->hp-=dmg;if(b->hp<0)b->hp=0;if(b->hp==0)b->dead=true;
            if(!defended){
                stock_note_unblocked_hit(b);
                if(!b->dead)apply_hit_reaction(b,a->facing,&resolved);
                lf2_audio_play(item_hit_sound(oid));spawn_hit_fx(world,b,0);
            }else if(b->dead){
                apply_hit_reaction(b,a->facing,&resolved);
            }
            if(a->held_units>0)a->held_units--;
            if(a->held_units<=0){
                lf2_logf("ITEM","%s weapon broke oid=%d name=%s",a->def.name,oid,item_name(oid));
                a->held_oid=0;a->held_units=0;lf2_audio_play("data/094.wav");
            }
            if(resolved.arest>0)a->arest_ticks=resolved.arest;
            if(resolved.vrest>0)a->vrest_ticks[bidx]=resolved.vrest;
            lf2_logf("ITEM","weapon hit attacker=%s oid=%d target=%s hp=%d->%d frame=%d defend=%d",
                     a->def.name,oid,b->def.name,old,b->hp,wf_id,defended);
            return 1;
        }
    }
    return 0;
}

static void apply_hits(object_world_t *world,fighter_t *fighters,int total,int aidx,int bidx,int difficulty) {
    if(!fighters||aidx<0||bidx<0||aidx>=total||bidx>=total||aidx==bidx)return;
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
        /* kind 6 is passive: it marks a vulnerable/super-punch target region
           and must never execute as an attack of its owning fighter. Treating
           it as active causes state-16 frames (for example Firen 226/227) to
           deal zero-damage repeat hits and bounce a falling victim forever
           between state 12 and injury frames. */
        if(it->kind!=0&&it->kind!=4&&it->kind!=5&&!sustained&&!catch_itr)continue;
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
