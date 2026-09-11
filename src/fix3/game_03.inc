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

static int resolve_command_frame_mode(fighter_t *f,const input_t *in,int allow_base){
    if(!f||!in)return 0;lf2_frame_def_t *base=&f->def.frames[0],*cur=frame_def(f);
    if(!cur)cur=base;
#define LF2_CMD(field) ((cur->field>0)?cur->field:(allow_base?base->field:0))
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

static int resolve_command_frame(fighter_t *f,const input_t *in){return resolve_command_frame_mode(f,in,1);}
static int resolve_chained_command_frame(fighter_t *f,const input_t *in){return resolve_command_frame_mode(f,in,0);}

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
