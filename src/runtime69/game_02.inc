
static void start_attack(fighter_t *f) {
    f->punch_variant ^= 1;
    f->hit_latched=false;
    /* Light weapons have dedicated type-0 attack frames whose wpoint selects
       the correct weapon picture for every swing. Heavy objects still use the
       conservative punch path until their throw/release state is emulated. */
    if(f->held_oid&&!item_is_bottle(f->held_oid)&&!item_is_heavy(f->held_oid)){
        int wf=f->punch_variant?20:25;
        if(f->y<0.0f&&f->def.frames[30].present)wf=30;
        else if((f->frame>=9&&f->frame<=11)&&f->def.frames[35].present)wf=35;
        else if((f->frame==213||f->frame==214||f->frame==216||f->frame==217)&&f->def.frames[40].present)wf=40;
        if(f->def.frames[wf].present){set_frame(f,wf);return;}
    }
    set_frame(f,f->punch_variant?60:65);
}

static int mp_component(int raw){
    int v=raw<0?-raw:raw;
    int mp=v%1000;
    return mp;
}

static int hp_component(int raw){
    int v=raw<0?-raw:raw;
    return (v/1000)*10;
}

/* Positive MP on the command frame is paid once when the move starts.
   Negative MP is different in LF2: it is paid every time that authored frame
   is entered (Firen burn_run uses mp:-10 on its looping frames). */
static void start_special_frame(fighter_t *f,int id) {
    if (!f || id<=0 || id>=LF2_MAX_FRAMES || !f->def.frames[id].present) return;
    lf2_frame_def_t *fr=&f->def.frames[id];
    /* Louis' DJA transform is an ID-specific LF2 rule, not encoded in mp:.
       2.00a permits it at <=177 HP; lf2.net removes that restriction. */
    if(f->roster_idx==4 && id==300 && f->hp>177 && !g_runtime_cheat){
        lf2_logf("FORM","LouisEX transform denied hp=%d threshold=177",f->hp);
        return;
    }
    if(f->roster_idx==3 && id==295 && f->transform_target<0){
        lf2_logf("FORM","Rudolf second-transform denied: no captured target");
        return;
    }
    int cost=fr->mp>0?mp_component(fr->mp):0;
    int hp_cost=fr->mp>0?hp_component(fr->mp):0;
    if(f->mp<cost || (hp_cost>0 && f->hp<=hp_cost)){
        lf2_logf("MP","%s special denied frame=%d mp=%d cost=%d hp_cost=%d",f->def.name,id,f->mp,cost,hp_cost);
        return;
    }
    int before=f->mp;
    f->mp-=cost;
    if(hp_cost>0){f->hp-=hp_cost;if(f->hp<1)f->hp=1;}
    f->hit_latched=false;
    lf2_logf("GAME","%s special frame=%d label=%s cost=%d mp=%d->%d",f->def.name,id,fr->label,cost,before,f->mp);
    set_frame(f,id);
}

/* Apply the negative `mp:` semantics used by looping special frames. The
   authored hit_d transition is LF2's escape path when the fighter can no
   longer afford the continuation. */
static int prepare_chained_frame(fighter_t *f,int id){
    if(!f||id<0||id>=LF2_MAX_FRAMES||!f->def.frames[id].present)return id;
    lf2_frame_def_t *fr=&f->def.frames[id];
    if(fr->mp>=0)return id;
    int cost=mp_component(fr->mp);
    int hp_cost=hp_component(fr->mp);
    if(cost<=0&&hp_cost<=0)return id;
    if(f->mp<cost || (hp_cost>0&&f->hp<=hp_cost)){
        int escape=(fr->hit_d>0&&fr->hit_d<LF2_MAX_FRAMES&&f->def.frames[fr->hit_d].present)?fr->hit_d:0;
        lf2_logf("MP","%s continuous exhausted frame=%d mp=%d cost=%d -> %d",f->def.name,id,f->mp,cost,escape);
        f->mp=0;
        return escape;
    }
    int before=f->mp;f->mp-=cost;
    if(hp_cost>0){f->hp-=hp_cost;if(f->hp<1)f->hp=1;}
    lf2_logf("MP","%s continuous frame=%d cost=%d mp=%d->%d",f->def.name,id,cost,before,f->mp);
    return id;
}

static int resolve_command_frame(fighter_t *f,const input_t *in){
    if(!f||!in)return 0;lf2_frame_def_t *base=&f->def.frames[0],*cur=frame_def(f);
    if(!cur)cur=base;
#define LF2_CMD(field) ((cur->field>0)?cur->field:base->field)
    bool forward=f->facing?in->left:in->right;

    /* Vita convenience shortcuts map onto the original LF2 command fields,
       not one hard-coded move. Triangle selects the attack-special family and
       R selects the jump-special family. This matters for hidden characters:
       Firzen and Jan intentionally have no hit_Fa root in the stock DAT. */
    if(in->special_attack_pressed){
        int id=0;
        if(in->up)id=LF2_CMD(hit_Ua);else if(in->down)id=LF2_CMD(hit_Da);else if(forward)id=LF2_CMD(hit_Fa);
        if(!id)id=LF2_CMD(hit_Fa);if(!id)id=LF2_CMD(hit_Ua);if(!id)id=LF2_CMD(hit_Da);
        return id;
    }
    if(in->special_jump_pressed){
        int id=0;
        if(in->up)id=LF2_CMD(hit_Uj);else if(in->down)id=LF2_CMD(hit_Dj);else if(forward)id=LF2_CMD(hit_Fj);
        if(!id)id=LF2_CMD(hit_Fj);if(!id)id=LF2_CMD(hit_Uj);if(!id)id=LF2_CMD(hit_Dj);if(!id)id=LF2_CMD(hit_ja);
        return id;
    }

    /* Original PC-style commands remain available unchanged. */
    if((in->attack_pressed&&in->jump)||(in->jump_pressed&&in->attack))return LF2_CMD(hit_ja);
    if(!in->defend)return 0;
    if(in->attack_pressed){if(in->up)return LF2_CMD(hit_Ua);if(in->down)return LF2_CMD(hit_Da);if(forward)return LF2_CMD(hit_Fa);}
    if(in->jump_pressed){if(in->up)return LF2_CMD(hit_Uj);if(in->down)return LF2_CMD(hit_Dj);if(forward)return LF2_CMD(hit_Fj);}
#undef LF2_CMD
    return 0;
}

static void update_animation(fighter_t *f, const input_t *in) {
    lf2_frame_def_t *fr=frame_def(f);
    if (!fr) { set_frame(f,0); return; }
    f->ticks++;
    if(f->falling && f->frame>=180 && f->frame<=191){
        if(f->ticks>=2){int n=f->frame+1;if(n>191)n=180;set_frame(f,n);}return;
    }
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
    next=prepare_chained_frame(f,next);
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
    in.defend=(p&SCE_CTRL_CIRCLE)!=0;
    in.special_attack=(p&SCE_CTRL_TRIANGLE)!=0;in.special_jump=(p&SCE_CTRL_RTRIGGER)!=0;
    in.pickup=(p&SCE_CTRL_LTRIGGER)!=0;
    in.attack_pressed=(pressed&SCE_CTRL_SQUARE)!=0;in.jump_pressed=(pressed&SCE_CTRL_CROSS)!=0;
    in.defend_pressed=(pressed&SCE_CTRL_CIRCLE)!=0;
    in.special_attack_pressed=(pressed&SCE_CTRL_TRIANGLE)!=0;in.special_jump_pressed=(pressed&SCE_CTRL_RTRIGGER)!=0;
    in.pickup_pressed=(pressed&SCE_CTRL_LTRIGGER)!=0;
    return in;
}

static int ai_frame_affordable(fighter_t *me,int id){
    if(!me||id<=0||id>=LF2_MAX_FRAMES||!me->def.frames[id].present)return 0;
    if(me->roster_idx==3&&id==295&&me->transform_target<0)return 0;
    int raw=me->def.frames[id].mp;
    int cost=raw>0?mp_component(raw):0;
    int hp_cost=raw>0?hp_component(raw):0;
    return me->mp>=cost && (hp_cost<=0||me->hp>hp_cost);
}

static int ai_choose_special(fighter_t *me,input_t *in,float dx,float dz,int difficulty){
    if(!me||!in)return 0;lf2_frame_def_t *b=&me->def.frames[0];
    enum { C_FA,C_FJ,C_UA,C_DA,C_UJ,C_DJ,C_JA };
    int ids[7]={b->hit_Fa,b->hit_Fj,b->hit_Ua,b->hit_Da,b->hit_Uj,b->hit_Dj,b->hit_ja};
    int valid[7],n=0;for(int i=0;i<7;i++)if(ai_frame_affordable(me,ids[i]))valid[n++]=i;if(!n)return 0;
    int choice=valid[rand()%n];
    /* At range prefer forward projectile/dash commands when they exist. */
    if(fabsf(dx)>105.0f){if(ai_frame_affordable(me,b->hit_Fa))choice=C_FA;else if(ai_frame_affordable(me,b->hit_Fj))choice=C_FJ;}
    bool forward_left=me->facing!=0;
    in->defend=true;
    switch(choice){
        case C_FA: in->attack=in->attack_pressed=true;if(forward_left)in->left=true;else in->right=true;break;
        case C_FJ: in->jump=in->jump_pressed=true;if(forward_left)in->left=true;else in->right=true;break;
        case C_UA: in->attack=in->attack_pressed=true;in->up=true;break;
        case C_DA: in->attack=in->attack_pressed=true;in->down=true;break;
        case C_UJ: in->jump=in->jump_pressed=true;in->up=true;break;
        case C_DJ: in->jump=in->jump_pressed=true;in->down=true;break;
        case C_JA: in->defend=false;in->jump=in->jump_pressed=true;in->attack=in->attack_pressed=true;break;
    }
    lf2_logf("AI","%s skill frame=%d distance=%.1f dz=%.1f difficulty=%d",me->def.name,ids[choice],fabsf(dx),fabsf(dz),difficulty);
    return 1;
}

static input_t make_ai_input(fighter_t *me, fighter_t *other, int difficulty) {
    input_t in={0};
    if (me->dead || !other || other->dead) return in;
    float dx=other->x-me->x, dz=other->z-me->z;
    static const float close_by_diff[4]={66.0f,72.0f,80.0f,90.0f};
    static const int cooldown_by_diff[4]={50,36,25,17};
    static const int skill_mod_by_diff[4]={13,7,4,2};
    if(difficulty<LF2_DIFF_EASY)difficulty=LF2_DIFF_EASY;if(difficulty>LF2_DIFF_CRAZY)difficulty=LF2_DIFF_CRAZY;
    float closex=close_by_diff[difficulty];
    if(fabsf(dz)>12.0f){if(dz<0)in.up=true;else in.down=true;}
    if(fabsf(dx)>closex){if(dx<0)in.left=true;else in.right=true;if(fabsf(dx)>180.0f)in.run=true;}
    if(me->ai_cooldown>0)me->ai_cooldown--;
    if(me->ai_cooldown==0&&!in_action(me)&&me->frame!=110){
        int can_reach=fabsf(dx)<390.0f&&fabsf(dz)<48.0f;
        int try_skill=can_reach&&((rand()%skill_mod_by_diff[difficulty])==0);
        /* Ranged attacks must happen before the CPU has already walked into
           punch distance; Normal AI now visibly uses LF2's own DAT commands. */
        if(try_skill&&ai_choose_special(me,&in,dx,dz,difficulty))me->ai_cooldown=cooldown_by_diff[difficulty]+12;
        else if(fabsf(dx)<=closex&&fabsf(dz)<25.0f){in.attack_pressed=in.attack=true;me->ai_cooldown=cooldown_by_diff[difficulty];}
    }
    return in;
}

static int item_is_bottle(int oid){return oid==122||oid==123;}
static int item_is_heavy(int oid){return oid==150||oid==151;}
static int item_is_pickup_oid(int oid){return oid==100||oid==101||oid==120||oid==121||oid==122||oid==123||oid==124||oid==150||oid==151;}
static const char *item_name(int oid){
    switch(oid){
        case 100:return "Stick";case 101:return "Hoe";case 120:return "Knife";
        case 121:return "Baseball";case 122:return "Milk";case 123:return "Beer";
        case 124:return "Boomerang";case 150:return "Stone";case 151:return "Wooden Box";
        default:return "Item";
    }
}
static int item_units_for_oid(int oid){
    switch(oid){
        case 122:return 100; /* stock bottle HP/time units */
        case 123:return 125;
        case 100:return 12;case 101:return 11;case 120:return 10;case 121:return 8;
        case 124:return 12;case 150:return 7;case 151:return 6;default:return 8;
    }
}
static int item_weapon_bonus(int oid){
    switch(oid){
        case 100:return 22;case 101:return 28;case 120:return 30;case 121:return 20;
        case 124:return 28;case 150:return 34;case 151:return 40;default:return 0;
    }
}
static const char *item_hit_sound(int oid){
    switch(oid){
        case 100:return "data/011.wav";case 101:return "data/079.wav";case 120:return "data/027.wav";
        case 121:return "data/017.wav";case 124:return "data/027.wav";case 150:return "data/079.wav";
        case 151:return "data/079.wav";default:return "data/001.wav";
    }
}
static void finish_drink(fighter_t *f,int empty){
    if(!f)return;
    int oid=f->held_oid;f->drinking=false;f->drink_tick=0;
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

