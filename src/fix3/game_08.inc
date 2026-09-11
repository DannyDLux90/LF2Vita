    f->held_oid=0;f->held_units=0;f->drinking=false;f->drink_tick=0;
    lf2_logf("ITEM","%s threw oid=%d name=%s fighter_frame=%d object_frame=%d vel=%.1f,%.1f,%.1f",
             f->def.name,oid,item_name(oid),f->frame,o->frame,o->vx,o->vy,o->vz);
    return 1;
}

static int count_loose_items(const object_world_t *w){
    int n=0;if(!w)return 0;for(int i=0;i<LF2_MAX_ACTIVE_OBJECTS;i++)if(w->objects[i].active&&w->objects[i].pickup_item)n++;return n;
}

static world_object_t *nearest_loose_item(object_world_t *w,const fighter_t *f){
    world_object_t *best=NULL;float bd=1.0e30f;if(!w||!f)return NULL;
    for(int i=0;i<LF2_MAX_ACTIVE_OBJECTS;i++){
        world_object_t *o=&w->objects[i];if(!o->active||!o->pickup_item||!o->grounded)continue;
        float dx=o->x-f->x,dz=o->z-f->z,d=dx*dx+dz*dz;
        if(fabsf(dx)<=78.0f&&fabsf(dz)<=46.0f&&d<bd){best=o;bd=d;}
    }
    return best;
}

static int item_is_simple_throw(int oid){return item_is_heavy(oid)||oid==121;}
static int item_is_direction_throw(int oid){return oid==120||oid==124;}
static int fighter_is_dash_frame(const fighter_t *f){return f&&(f->frame==213||f->frame==214||f->frame==216||f->frame==217);}

/* PC LF2 has a small throw-command hierarchy. Heavy objects/baseballs throw
   on Attack. Knife/boomerang throw on direction+Attack. Bottles and ordinary
   light weapons require a run/jump/dash throw command; otherwise Square keeps
   its normal swing/drink meaning. Pickup remains a dedicated Vita action on L. */
static int fighter_handle_item_input(object_world_t *w,fighter_t *f,input_t *in){
    if(!w||!f||!in||f->dead||!f->active)return 0;
    if(f->held_oid){
        /* Once a stock throw chain (45..54) has started, it owns the held
           item until the authored wpoint release frame.  fix2 re-entered the
           bottle drinking branch on the very next 30 Hz tick while Square was
           still held, replacing frame 45 before frame 47 could release it. */
        if(f->frame>=45&&f->frame<=54){return 1;}
        int dir=in->left||in->right||in->up||in->down;
        int airborne=f->y<0.0f||f->vy!=0.0f;
        int dash=fighter_is_dash_frame(f);
        int throw_now=0;
        /* Vita explicit throw command: while an item is held, L+Square always
           enters the stock LF2 throw frame chain. L by itself remains pickup
           only when the fighter has empty hands. */
        if((in->pickup&&in->attack_pressed)||(in->pickup_pressed&&in->attack))throw_now=1;
        if(!throw_now&&in->attack_pressed){
            if(item_is_simple_throw(f->held_oid))throw_now=1;
            else if(item_is_direction_throw(f->held_oid)&&dir)throw_now=1;
            else if(item_is_bottle(f->held_oid)){if(airborne||dash||(in->run&&dir))throw_now=1;}
            else if(dir&&(airborne||dash||in->run))throw_now=1;
        }
        if(throw_now&&!f->drinking){
            int tf=item_is_heavy(f->held_oid)?50:(airborne||dash?52:45);
            if(tf>=0&&tf<LF2_MAX_FRAMES&&f->def.frames[tf].present){
                set_frame(f,tf);
                lf2_logf("ITEM","%s throw start oid=%d name=%s frame=%d",f->def.name,f->held_oid,item_name(f->held_oid),tf);
                return 1;
            }
        }
        if(item_is_bottle(f->held_oid)&&in->attack&&!f->drinking){
            f->drinking=true;f->drink_tick=0;
            if(f->def.frames[55].present)set_frame(f,55);
            lf2_logf("ITEM","%s starts drinking %s units=%d hp=%d mp=%d",f->def.name,item_name(f->held_oid),f->held_units,f->hp,f->mp);
            return 1;
        }
        return 0;
    }
    /* L is deliberately separate from Square. Accept held L as well as its
       edge so entering the pickup radius while holding L is reliable. */
    if(!in->pickup || in_action(f) || f->falling || f->y<0.0f)return 0;
    world_object_t *o=nearest_loose_item(w,f);if(!o)return 0;
    f->held_oid=o->oid;
    f->held_units=o->durability>0?o->durability:(item_is_bottle(o->oid)?item_units_for_oid(o->oid):(o->cache&&o->cache->def.weapon_hp>0?o->cache->def.weapon_hp:item_units_for_oid(o->oid)));
    f->drinking=false;f->drink_tick=0;f->stat_picking++;
    o->active=false;
    int pick_frame=item_is_heavy(f->held_oid)?116:115;if(pick_frame<LF2_MAX_FRAMES&&f->def.frames[pick_frame].present)set_frame(f,pick_frame);
    lf2_audio_play("data/009.wav");
    lf2_logf("ITEM","%s picked oid=%d name=%s units=%d",f->def.name,f->held_oid,item_name(f->held_oid),f->held_units);
    return 1;
}

static void item_director_tick(object_world_t *w,int *timer){
    if(!w||!timer)return;
    if(*timer>0){(*timer)--;return;}
    if(count_loose_items(w)>=4){*timer=180;return;}
    static const int ids[]={100,101,120,121,122,123,124,150,151};
    int oid=ids[rand()%(int)(sizeof(ids)/sizeof(ids[0]))];
    float span=g_world_max_x-g_world_min_x-160.0f;if(span<80.0f)span=80.0f;
    float x=g_world_min_x+80.0f+(float)(rand()%1000)/1000.0f*span;
    float z=-55.0f+(float)(rand()%120);
    spawn_pickup_item(w,oid,x,z,-220.0f-(float)(rand()%80));
    *timer=420+(rand()%360); /* roughly every 14..26 seconds at 30 Hz */
}

static lf2_frame_def_t *object_frame_def(world_object_t *o){
    if(!o||!o->active||!o->cache||o->frame<0||o->frame>=LF2_MAX_FRAMES||!o->cache->def.frames[o->frame].present)return NULL;
    return &o->cache->def.frames[o->frame];
}

static int type3_hit_a_duration(int code){
    /* Exact published stock mapping where known; values above 10 continue the
       original inverse-duration scale.  Stock 2.00a also uses 14/25/60. */
    switch(code){
        case 1:return 400;case 2:return 255;case 3:return 180;case 4:return 135;
        case 5:return 100;case 6:return 95;case 7:return 75;case 8:return 65;
        case 9:return 55;case 10:return 52;case 20:return 24;case 30:return 16;
        case 40:return 12;case 50:return 9;
        default: if(code>0){int t=(500+code/2)/code;return t<1?1:t;} return 0;
    }
}

static void object_prepare_type3_timer(world_object_t *o,const lf2_frame_def_t *fr){
    if(!o||!fr||o->type!=3)return;
    if(fr->hit_a<=0){o->type3_timer=0;o->type3_timer_code=0;return;}
    if(o->type3_timer_code!=fr->hit_a){
        o->type3_timer_code=fr->hit_a;o->type3_timer=type3_hit_a_duration(fr->hit_a);
