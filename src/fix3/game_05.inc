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
    for(int ri=0;ri<LF2_MAX_ACTORS;ri++)if(f->vrest_ticks[ri]>0)f->vrest_ticks[ri]--;
    if(f->sonata_damage_cooldown>0)f->sonata_damage_cooldown--;
    if(f->ice_sword_ticks>0)f->ice_sword_ticks--;
    if(f->jan_heal_ticks>0){
        f->jan_heal_ticks--;f->jan_heal_phase++;
        if((f->jan_heal_phase==4||((f->jan_heal_phase>4)&&((f->jan_heal_phase-4)%8)==0))&&f->hp<f->max_hp){
            int old=f->hp;f->hp+=8;if(f->hp>f->max_hp)f->hp=f->max_hp;
            lf2_logf("STATUS","%s Jan regen hp=%d->%d left=%d",f->def.name,old,f->hp,f->jan_heal_ticks);
        }
        if(f->jan_heal_ticks==0)f->jan_heal_phase=0;
    }
    if(f->drinking&&item_is_bottle(f->held_oid)&&!f->dead){
        /* Bottle healing is intentionally time-based rather than an instant
           pickup bonus. It follows the original 2.00a behaviour closely:
           milk mainly restores HP plus some MP, beer only restores MP. */
        /* PC state 17 can be interrupted by Defend. Releasing Square also
           stops the drink but keeps the remaining bottle units. */
        if(in->defend_pressed||(!in->attack&&f->drink_tick>3)){finish_drink(f,0);return;}
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
