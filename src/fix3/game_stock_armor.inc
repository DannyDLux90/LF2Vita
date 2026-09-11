/* Stock hard-coded character armor. This is intentionally separate from
   state-7 defense: armor checks the victim's bdefend counter BEFORE this hit,
   and can absorb ordinary >60 bdefend attacks. bdefend:100 is the special
   universal defense/armor bypass and is never absorbed here. */
static int g_stock_attack_oid=-1;
static void stock_base_object_apply_hits(object_world_t *w,world_object_t *o,fighter_t *fighters,int total,int difficulty);

static int stock_armor_threshold(const fighter_t *b){
    if(!b)return -1;
    /* data ids: Louis=6 -> roster 4, Knight=37 -> 17, Julian=52 -> 22. */
    if(b->roster_idx==4)return 1;
    if(b->roster_idx==17||b->roster_idx==22)return 15;
    return -1;
}

static int stock_armor_element_bypass(int effect){
    /* Louis/Knight metallic armor does not stop stock fire/ice families,
       including the documented effect-23 exception. */
    return effect==2||effect==3||effect==20||effect==21||effect==22||effect==23||effect==30;
}

static int stock_armor_state_disabled(int state){
    return state==8||state==11||state==12||state==13||state==14||state==16||state==18;
}

static int stock_try_armor_hit(fighter_t *b,const lf2_itr_def_t *it,int injury,int *damage_out){
    if(!g_runtime_stock_compat||!b||!it)return 0;
    int threshold=stock_armor_threshold(b);
    if(threshold<0||b->bdefend_points>threshold||it->bdefend==100)return 0;
    if(g_stock_attack_oid==208||g_stock_attack_oid==214)return 0;
    lf2_frame_def_t *bf=frame_def(b);if(!bf)return 0;
    int state=bf->state;
    if(stock_armor_state_disabled(state))return 0;
    if(b->roster_idx==4 && !(b->frame>=0&&b->frame<=19) && state!=4&&state!=5&&state!=7)return 0;
    if((b->roster_idx==4||b->roster_idx==17)&&stock_armor_element_bypass(it->effect))return 0;

    if(it->bdefend>0)b->bdefend_points+=it->bdefend;
    if(damage_out)*damage_out=injury>0?injury/10:0;

    /* Armor also protects while defending. Normally state 7 was already
       handled by the regular block path; these transitions cover the stock
       armor exceptions for >60 attacks and the frame-110 back-hit quirk. */
    if(state==7&&it->bdefend>60&&b->def.frames[112].present)set_frame(b,112);
    else if(b->frame==110&&b->def.frames[111].present)set_frame(b,111);
    b->hit_flash=3;
    lf2_logf("ARMOR","%s absorb oid=%d injury=%d damage=%d add=%d total=%d threshold=%d state=%d",
             b->def.name,g_stock_attack_oid,injury,damage_out?*damage_out:0,it->bdefend,b->bdefend_points,threshold,state);
    return 1;
}

/* First let normal state-7 defense run (and establish its 5-TU victim
   shaking). Armor is the fallback for a hit that defense did not absorb. */
static int stock_try_defend_hit(fighter_t *b,int attacker_facing,const lf2_itr_def_t *it,int injury,int *damage_out){
    int defended=stock_hitlag_try_defend_hit(b,attacker_facing,it,injury,damage_out);
    if(defended)return 1;
    int armored=stock_try_armor_hit(b,it,injury,damage_out);
    if(armored)stock_hitlag_set_victim(b,1);
    return armored;
}

/* Object ids matter for two stock armor exceptions (208/214). Scope the id to
   one object hit pass; direct type-0 and held-weapon hits leave it at -1. */
static void object_apply_hits(object_world_t *w,world_object_t *o,fighter_t *fighters,int total,int difficulty){
    int previous=g_stock_attack_oid;
    g_stock_attack_oid=o?o->oid:-1;
    stock_base_object_apply_hits(w,o,fighters,total,difficulty);
    g_stock_attack_oid=previous;
}
