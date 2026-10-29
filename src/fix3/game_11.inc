    if(code!=14)o->vy=o->vy*(1.0f-turn)+dy/len*speed*turn;
    o->vz=o->vz*(1.0f-turn)+dz/len*speed*turn;
    if(code!=3&&fabsf(o->vx)>0.2f)o->facing=o->vx<0.0f;
}

static void update_object(object_world_t *w,world_object_t *o,fighter_t *fighters,int total){
    if(!o||!o->active)return;
    if(o->arest_ticks>0)o->arest_ticks--;for(int ri=0;ri<LF2_MAX_ACTORS;ri++)if(o->vrest_ticks[ri]>0)o->vrest_ticks[ri]--;
    lf2_frame_def_t *fr=object_frame_def(o);if(!fr){o->active=false;return;}
    if(o->pickup_item){
        /* Loose stock items use their DAT animation instead of a synthetic
           spinner. In-air frames loop until contact, then 70->71->72->64
           (light items) or 21->20 (heavy items) plays once and remains at the
           stable authored ground frame. */
        if(!o->grounded){
            o->age++;o->x+=o->vx;o->y+=o->vy;o->z+=o->vz;o->vy+=0.62f;o->vx*=0.992f;o->vz*=0.992f;
            if(o->y>=0.0f){
                float impact=sqrtf(o->vx*o->vx+o->vy*o->vy+o->vz*o->vz);
                o->y=0.0f;o->vy=o->vx=o->vz=0.0f;o->grounded=true;
                if(item_apply_drop_hurt(o,impact))return;
                object_set_frame(o,object_landing_frame(o->cache));
                lf2_logf("ITEM","land oid=%d name=%s x=%.1f z=%.1f frame=%d weapon_hp=%d",o->oid,item_name(o->oid),o->x,o->z,o->frame,o->weapon_hp);
                return;
            }
            o->ticks++;int duration=fr->wait+1;if(duration<1)duration=1;
            if(o->ticks>=duration){
                int next=fr->next;
                if(next==999||next==1000||next<0||next>=40||!o->cache->def.frames[next].present)next=0;
                object_set_frame(o,next);
            }
        }else{
            int stable=object_ground_frame(o->cache);
            if(o->frame!=stable){
                o->ticks++;int duration=fr->wait+1;if(duration<1)duration=1;
                if(o->ticks>=duration){
                    int next=fr->next;
                    if(next==999||next==1000||next<0||next>=LF2_MAX_FRAMES||
                       !o->cache->def.frames[next].present||next==0)next=stable;
                    object_set_frame(o,next);
                }
            }
        }
        return;
    }
    if(fr->state==9998){o->active=false;return;}
    o->age++;
    if(!o->opoint_fired&&fr->opoint_count>0){
        o->opoint_fired=true;
        for(int i=0;i<fr->opoint_count;i++){
            const lf2_opoint_def_t *op=&fr->opoints[i];
            int count=(op->facing>=20)?op->facing/10:1;if(count<1)count=1;if(count>8)count=8;
            for(int n=0;n<count;n++)object_spawn_child_one(w,o,op,n,count);
        }
    }
    if(o->attached){
        if(o->owner<0||o->owner>=total||!fighters[o->owner].active||fighters[o->owner].dead||fighters[o->owner].ice_sword_ticks<=0){o->active=false;return;}
        fighter_t *a=&fighters[o->owner];
        o->facing=a->facing;
        o->x=a->x+(a->facing?-o->attach_dx:o->attach_dx);o->y=a->y+o->attach_dy;o->z=a->z+o->attach_dz;
        return;
    }
    if(o->type==3){
        /* hit_j is z-axis velocity for type-3 objects: 50=0, 51=+1, 49=-1. */
        if(fr->hit_j!=0)o->vz=(float)(fr->hit_j-50);
        if(fr->hit_a>0&&o->type3_timer_code==fr->hit_a&&o->type3_timer>0){
            if(--o->type3_timer<=0){
                int timeout=fr->hit_d;
                /* -1 latches expiry while hit_d remains inside the same
                   timed flying family (Dennis/Jan/Firzen chase do this). */
                o->type3_timer=-1;
                lf2_logf("OBJECT","timer expire oid=%d frame=%d hit_d=%d",o->oid,o->frame,timeout);
                if(timeout>0&&timeout<LF2_MAX_FRAMES&&o->cache->def.frames[timeout].present)object_set_frame(o,timeout);
                else o->active=false;
                if(!o->active)return;fr=object_frame_def(o);if(!fr)return;
            }
        }
    }

    /* Type-3 hit_Fa is partly executable logic in stock LF2. Values
       5/6/8/9/11/13 are activators; the others below are chase modes. */
    if(o->type==3&&fr->hit_Fa>0){
        int hf=fr->hit_Fa;
        if(hf==5||hf==6||hf==8||hf==9||hf==11||hf==13)object_activate_hitfa(w,o,fighters,total,hf);
        else if(hf==1||hf==2||hf==4||hf==7||hf==12||hf==14){
            /* Timed chase stops steering after hit_a expires, even when hit_d
               points back into a frame carrying the same hit_Fa value. */
            if(fr->hit_a<=0||o->type3_timer>=0)object_apply_chase(o,fighters,total,hf);
        }
        /* hit_Fa 10 deliberately keeps its authored x velocity: it is the
           stock "move away" phase of John's disk rather than a chase. */
        if(!o->active)return;fr=object_frame_def(o);if(!fr)return;
    }
    o->x+=o->vx;o->y+=o->vy;o->z+=o->vz;
    if(o->type==1&&fr->state==1002)o->vy+=0.45f;
    if(o->type==2&&fr->state==2000)o->vy+=0.45f;
    if(o->type==1&&o->y>=2.0f&&fr->state==1002){
        float impact=sqrtf(o->vx*o->vx+o->vy*o->vy+o->vz*o->vz);
        o->y=0.0f;o->vx=o->vy=o->vz=0.0f;
        if(o->recoverable_weapon){
            o->pickup_item=true;o->grounded=true;o->owner=-1;o->team=0;
            if(item_apply_drop_hurt(o,impact))return;
            object_set_frame(o,object_landing_frame(o->cache));
        } else if(o->cache->def.frames[60].present)object_set_frame(o,60);else o->active=false;
    }
    if(o->type==2&&o->y>=2.0f&&fr->state==2000){
        float impact=sqrtf(o->vx*o->vx+o->vy*o->vy+o->vz*o->vz);
        o->y=0.0f;o->vx=o->vy=o->vz=0.0f;
        if(o->recoverable_weapon){
            o->pickup_item=true;o->grounded=true;o->owner=-1;o->team=0;
            if(item_apply_drop_hurt(o,impact))return;
            object_set_frame(o,object_landing_frame(o->cache));
        } else if(o->cache->def.frames[20].present)object_set_frame(o,20);else o->active=false;
    }
    if(!o->active)return;
    /* Projectiles live in world coordinates.  The old fixed 960px VS bound
       deleted every Stage projectile as soon as the camera advanced past the
       first screen (the Firzen Cannon at x~1900 was a visible example). */
    if(o->x<g_world_min_x-220.0f||o->x>g_world_max_x+220.0f||o->z<-180||o->z>180||o->y>180||o->age>420){o->active=false;return;}
    fr=object_frame_def(o);if(!fr)return;o->ticks++;int duration=fr->wait+1;if(duration<1)duration=1;if(o->ticks<duration)return;
    int next=fr->next;if(next<0){o->facing^=1;next=-next;}
    if(next==999)next=0;else if(next==1000){o->active=false;return;}
    object_set_frame(o,next);
}

static void spawn_hit_fx(object_world_t *w,const fighter_t *victim,int effect){
    if(!w||!victim)return;int kind=(effect==1)?1:((effect==2||effect==20||effect==21||effect==22)?2:((effect==3||effect==30)?3:0));
