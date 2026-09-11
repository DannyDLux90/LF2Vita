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
    }else{
        /* LF2 permits explicitly authored command links while an attack/catch
           animation is running.  Only the current frame is consulted here;
           neutral frame-0 shortcuts must not interrupt arbitrary actions. */
        int chained=resolve_chained_command_frame(f,in);
        if(chained>0)start_special_frame(f,chained);
        else if(f->frame==110&&!in->defend&&f->ticks>4)set_frame(f,0);
    }

    /* Stock state 19 (Firen burn-run) and state 301 (Deep z-movement) keep
       accepting vertical lane control while the attack animation is locked. */
    lf2_frame_def_t *move_fr=frame_def(f);
    if(move_fr&&(move_fr->state==19||move_fr->state==301)){
        float rz=f->def.running_speedz;if(rz<0)rz=-rz;if(rz<0.1f)rz=1.0f;
        if(in->up)f->z-=rz;if(in->down)f->z+=rz;
    }

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
