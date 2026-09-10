    if (id<0 || id>=LF2_MAX_FRAMES || !f->def.frames[id].present) id=0;
    f->frame=id; f->ticks=0;
    lf2_frame_def_t *fr=frame_def(f);
    if (fr) {
        if (fr->dvx == 550) f->vx = 0.0f;
        else if (fr->dvx) f->vx = (float)(f->facing ? -fr->dvx : fr->dvx);
        if (fr->dvy == 550) f->vy = 0.0f;
        else if (fr->dvy) f->vy = lf2_axis_value(fr->dvy);
        if (fr->dvz == 550) f->vz = 0.0f;
        else if (fr->dvz) f->vz = lf2_axis_value(fr->dvz);
        if (fr->sound[0]) lf2_audio_play(fr->sound);
    }
}

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
    if (f->dead) return;
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
