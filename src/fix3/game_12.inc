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

static void object_apply_reflections(object_world_t *w){
    if(!w)return;
    for(int si=0;si<LF2_MAX_ACTIVE_OBJECTS;si++){
        world_object_t *shield=&w->objects[si];
        if(!shield->active||shield->type!=3||shield->pickup_item||shield->attached)continue;
        lf2_frame_def_t *sf=object_frame_def(shield);if(!sf||sf->itr_count<=0)continue;
        for(int ii=0;ii<sf->itr_count;ii++){
            lf2_itr_def_t *it=&sf->itrs[ii];if(it->kind!=9)continue;
            int sx,sy;object_region_world(shield,it->x,it->y,it->w,it->h,&sx,&sy);
            for(int pi=0;pi<LF2_MAX_ACTIVE_OBJECTS;pi++){
                world_object_t *p=&w->objects[pi];
                if(pi==si||!p->active||p->type!=3||p->pickup_item||p->attached)continue;
                if(shield->team!=0&&p->team==shield->team)continue;
                if(fabsf(shield->z-p->z)>24.0f)continue;
                lf2_frame_def_t *pf=object_frame_def(p);if(!pf||pf->bdy_count<=0)continue;
                int reflected=0;
                for(int bi=0;bi<pf->bdy_count&&!reflected;bi++){
