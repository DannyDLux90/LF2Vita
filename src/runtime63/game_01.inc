
static bool in_action(const fighter_t *f) {
    int n=f->frame;
    if (n>=60 && n<110) return true;
    if (n>=220) return true;
    if (n>=240) return true;
    return false;
}

static void start_attack(fighter_t *f) {
    f->punch_variant ^= 1;
    f->hit_latched=false;
    set_frame(f,f->punch_variant?60:65);
}

static void start_special_frame(fighter_t *f,int id) {
    if (!f || id<=0 || id>=LF2_MAX_FRAMES || !f->def.frames[id].present) return;
    int cost=f->def.frames[id].mp; if(cost<0)cost=-cost; if(cost==0)cost=75;
    if(f->mp>=cost){f->mp-=cost;f->hit_latched=false;lf2_logf("GAME","%s special frame=%d label=%s cost=%d",f->def.name,id,f->def.frames[id].label,cost);set_frame(f,id);}
}

static int resolve_command_frame(fighter_t *f,const input_t *in){
    if(!f||!in)return 0;lf2_frame_def_t *base=&f->def.frames[0];
    if(in->special_pressed)return base->hit_Fa;
    if((in->attack_pressed&&in->jump)||(in->jump_pressed&&in->attack))return base->hit_ja;
    if(!in->defend)return 0;
    bool forward=f->facing?in->left:in->right;
    if(in->attack_pressed){if(in->up)return base->hit_Ua;if(in->down)return base->hit_Da;if(forward)return base->hit_Fa;}
    if(in->jump_pressed){if(in->up)return base->hit_Uj;if(in->down)return base->hit_Dj;if(forward)return base->hit_Fj;}
    return 0;
}

static void update_animation(fighter_t *f, const input_t *in) {
    lf2_frame_def_t *fr=frame_def(f);
    if (!fr) { set_frame(f,0); return; }
    f->ticks++;
    /* LF2's wait value is measured in 30 Hz time units. The simulation now
       runs at exactly 30 Hz, so no 60 Hz compensation factor belongs here. */
    int duration=fr->wait+1;
    if (duration<1) duration=1;

    if (f->frame==110 && in && in->defend) { if (f->ticks>=duration) f->ticks=0; return; }
    if (f->frame>=5 && f->frame<=11) return; /* movement drives walk/run animation */
    if (f->frame>=0 && f->frame<=3 && in && (in->left||in->right||in->up||in->down)) return;
    if (f->ticks<duration) return;

    int next=fr->next;
    if (next < 0) { f->facing ^= 1; next = -next; }
    if (next==999) {
        if (f->frame>=0 && f->frame<=3) next=(f->frame==3)?0:f->frame+1;
        else next=0;
    } else if (next==1000) {
        /* 1000 deletes non-character objects in LF2. For a playable type-0
           fighter, gracefully return to standing instead of indexing 1000. */
        next=0;
    } else if (next>=1100 && next<=1299) {
        /* Rudolf hide timing is not fully emulated yet; keep the frame chain
           safe and return to standing after the hidden interval. */
        next=0;
    }
    set_frame(f,next);
}

static input_t make_player_input(const SceCtrlData *pad, uint32_t pressed) {
    input_t in={0};
    uint32_t p=pad->buttons;
    int ax=(int)pad->lx-128, ay=(int)pad->ly-128; int dead=28,run_dead=88;
    bool analog_left=ax<-dead,analog_right=ax>dead,analog_up=ay<-dead,analog_down=ay>dead;
    in.left=((p&SCE_CTRL_LEFT)!=0)||analog_left;in.right=((p&SCE_CTRL_RIGHT)!=0)||analog_right;
    in.up=((p&SCE_CTRL_UP)!=0)||analog_up;in.down=((p&SCE_CTRL_DOWN)!=0)||analog_down;
    in.run=(abs(ax)>run_dead||abs(ay)>run_dead);
    in.attack=(p&SCE_CTRL_SQUARE)!=0;in.jump=(p&SCE_CTRL_CROSS)!=0;
    in.defend=((p&SCE_CTRL_CIRCLE)!=0)||((p&SCE_CTRL_LTRIGGER)!=0);
    in.special=((p&SCE_CTRL_TRIANGLE)!=0)||((p&SCE_CTRL_RTRIGGER)!=0);
    in.attack_pressed=(pressed&SCE_CTRL_SQUARE)!=0;in.jump_pressed=(pressed&SCE_CTRL_CROSS)!=0;
    in.defend_pressed=((pressed&SCE_CTRL_CIRCLE)!=0)||((pressed&SCE_CTRL_LTRIGGER)!=0);
    in.special_pressed=((pressed&SCE_CTRL_TRIANGLE)!=0)||((pressed&SCE_CTRL_RTRIGGER)!=0);
    return in;
}

static input_t make_ai_input(fighter_t *me, fighter_t *other, int difficulty) {
    input_t in={0};
    if (me->dead || !other || other->dead) return in;
    float dx=other->x-me->x, dz=other->z-me->z;
    static const float close_by_diff[4]={66.0f,72.0f,80.0f,90.0f};
    static const int cooldown_by_diff[4]={48,34,24,16};

    if(difficulty<LF2_DIFF_EASY)difficulty=LF2_DIFF_EASY;
    if(difficulty>LF2_DIFF_CRAZY)difficulty=LF2_DIFF_CRAZY;
    float closex=close_by_diff[difficulty];
    if(fabsf(dz)>12.0f) { if(dz<0) in.up=true; else in.down=true; }
    if(fabsf(dx)>closex) { if(dx<0) in.left=true; else in.right=true; }
    if(me->ai_cooldown>0) me->ai_cooldown--;
    if(fabsf(dx)<=closex && fabsf(dz)<25.0f && me->ai_cooldown==0 && !in_action(me)) {
        int special_mod=(difficulty==LF2_DIFF_CRAZY)?4:(difficulty==LF2_DIFF_DIFFICULT?7:12);
        if(difficulty>=LF2_DIFF_DIFFICULT && me->mp>120 && (rand()%special_mod)==0)
            in.special_pressed=in.special=true;
        else
            in.attack_pressed=in.attack=true;
        me->ai_cooldown=cooldown_by_diff[difficulty];
    }
    return in;
}

static void update_control(fighter_t *f, const input_t *in) {
    if (f->dead) {
        /* Death/lying frames still have real LF2 wait/next chains. v0.60
           returned here and froze a defeated character on the first KO frame. */
        update_animation(f, NULL);
        return;
    }
    if (f->invuln>0) f->invuln--;
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
            bool running=in->run&&f->def.frames[9].present;
            float sx=(running?f->def.running_speed:f->def.walking_speed);
            float sz=(running?f->def.running_speedz:f->def.walking_speedz);bool moving=false;
            if(in->left){f->x-=sx;f->facing=1;moving=true;}if(in->right){f->x+=sx;f->facing=0;moving=true;}
            if(in->up){f->z-=sz;moving=true;}if(in->down){f->z+=sz;moving=true;}
            if(moving){
                f->walk_phase++;int first=running?9:5,last=running?11:8,count=last-first+1;
                float rate=running?f->def.running_frame_rate:f->def.walking_frame_rate;int period=(int)(rate+0.5f);if(period<1)period=1;
                int wf=first+(f->walk_phase/period)%count;if(f->frame<first||f->frame>last||f->frame!=wf)set_frame(f,wf);
            }else if((f->frame>=5&&f->frame<=11))set_frame(f,0);
        }
    }else if(f->frame==110&&!in->defend&&f->ticks>4)set_frame(f,0);

    if(f->y<0.0f||f->vy!=0.0f){
        f->y+=f->vy;f->vy+=1.70f;f->x+=f->vx;f->z+=f->vz;f->vx*=0.90f;f->vz*=0.88f;
        if(f->y>=0.0f){f->y=0;f->vy=0;f->vx=0;f->vz=0;if(!in_action(f))set_frame(f,0);}
    }else if(in_action(f)){f->x+=f->vx;f->z+=f->vz;f->vx*=0.72f;f->vz*=0.72f;}
    f->x=clampf(f->x,40.0f,920.0f);f->z=clampf(f->z,-70.0f,75.0f);if(f->mp<500)f->mp++;update_animation(f,in);
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
    if(!cat){lf2_logf("OBJECT","unsupported oid=%d (not in stock projectile catalog)",oid);return NULL;}
    if(w->cache_count>=LF2_MAX_OBJECT_CACHE){lf2_logf("ERROR","object cache full oid=%d",oid);return NULL;}
    object_cache_entry_t *c=&w->cache[w->cache_count++];memset(c,0,sizeof(*c));c->used=true;c->oid=oid;c->type=cat->type;
    int rc=lf2_load_character(GAME_ROOT,cat->dat,&c->def);
    if(rc<0){lf2_logf("ERROR","object DAT load oid=%d dat=%s rc=%d",oid,cat->dat,rc);c->used=false;return NULL;}
    if(!c->def.name[0])snprintf(c->def.name,sizeof(c->def.name),"OID %d",oid);
    rc=lf2_load_character_textures(GAME_ROOT,&c->def);
    if(rc<0){lf2_logf("ERROR","object texture load oid=%d dat=%s rc=%d",oid,cat->dat,rc);lf2_free_character_textures(&c->def);c->used=false;return NULL;}
    lf2_logf("OBJECT","cache oid=%d type=%d dat=%s sheets=%d",oid,c->type,cat->dat,c->def.sheet_count);lf2_log_memory("object cached");
