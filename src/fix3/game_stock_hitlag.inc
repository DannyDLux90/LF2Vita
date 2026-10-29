/* Stock LF2 hitlag/shaking compatibility layer.
   The retail object has a signed `shaking` field beside fall/bdefend. Keep
   this compatibility state outside fighter_t so native Vita runtime layout
   and Stage behavior remain unchanged. */
typedef struct {
    fighter_t *fighter;
    int shaking;
    uint32_t victim_serial;
} stock_hitlag_slot_t;

static stock_hitlag_slot_t g_stock_hitlag_slots[LF2_MAX_ACTORS*2];
static bool g_stock_hitlag_dirty=false;

/* Base functions are renamed at include time in game.c. */
static bool stock_base_in_action(const fighter_t *f);
static input_t stock_base_make_ai_input(fighter_t *me,fighter_t *other,int difficulty);
static void stock_base_update_control(fighter_t *f,const input_t *in);
static void stock_base_handle_catch_input(fighter_t *fighters,int total,int holder_idx,const input_t *in);
static int stock_base_fighter_handle_item_input(object_world_t *w,fighter_t *f,input_t *in);
static void stock_base_spawn_fighter_opoints(object_world_t *w,fighter_t *fighters,int total,int owner_idx);
static int stock_base_try_defend_hit(fighter_t *b,int attacker_facing,const lf2_itr_def_t *it,int injury,int *damage_out);
static void stock_base_note_unblocked_hit(fighter_t *b);
static void stock_base_apply_hits(object_world_t *world,fighter_t *fighters,int total,int aidx,int bidx,int difficulty);
static input_t stock_base_make_ai_retreat_input(fighter_t *me,fighter_t *down);
static void stock_base_process_frame_state(fighter_t *fighters,int total,int idx,object_world_t *world);
static int stock_base_handle_form_input(fighter_t *fighters,int total,int idx,const input_t *in);
static uint32_t stock_base_state_digest(const fighter_t *fighters,int total,const object_world_t *world,int item_drop_timer);

static void stock_hitlag_clear_all(void){
    memset(g_stock_hitlag_slots,0,sizeof(g_stock_hitlag_slots));
    g_stock_hitlag_dirty=false;
}

static stock_hitlag_slot_t *stock_hitlag_slot_for(fighter_t *f,int create){
    if(!f)return NULL;
    stock_hitlag_slot_t *empty=NULL;
    for(unsigned i=0;i<(unsigned)(LF2_MAX_ACTORS*2);i++){
        stock_hitlag_slot_t *s=&g_stock_hitlag_slots[i];
        if(s->fighter==f)return s;
        if(!empty&&!s->fighter)empty=s;
    }
    if(create&&empty){empty->fighter=f;return empty;}
    return NULL;
}

static int stock_hitlag_value(const fighter_t *f){
    if(!f)return 0;
    for(unsigned i=0;i<(unsigned)(LF2_MAX_ACTORS*2);i++)
        if(g_stock_hitlag_slots[i].fighter==f)return g_stock_hitlag_slots[i].shaking;
    return 0;
}

static uint32_t stock_hitlag_victim_serial(const fighter_t *f){
    if(!f)return 0;
    for(unsigned i=0;i<(unsigned)(LF2_MAX_ACTORS*2);i++)
        if(g_stock_hitlag_slots[i].fighter==f)return g_stock_hitlag_slots[i].victim_serial;
    return 0;
}

static void stock_hitlag_advance(stock_hitlag_slot_t *s){
    if(!s||s->shaking==0)return;
    if(s->shaking>0)s->shaking--;
    else s->shaking++;
}

static void stock_hitlag_set(fighter_t *f,int shaking,int victim_event){
    if(!g_runtime_stock_compat||!f||shaking==0)return;
    stock_hitlag_slot_t *s=stock_hitlag_slot_for(f,1);
    if(!s)return;
    s->shaking=shaking;
    if(victim_event)s->victim_serial++;
    g_stock_hitlag_dirty=true;
}

static void stock_hitlag_set_victim(fighter_t *f,int defended){
    stock_hitlag_set(f,defended?-5:-3,1);
}

static void stock_hitlag_set_attacker(fighter_t *f){
    stock_hitlag_set(f,3,0);
}

static bool in_action(const fighter_t *f){
    if(g_runtime_stock_compat&&stock_hitlag_value(f)!=0)return true;
    return stock_base_in_action(f);
}

static input_t make_ai_input(fighter_t *me,fighter_t *other,int difficulty){
    if(g_runtime_stock_compat&&stock_hitlag_value(me)!=0){input_t zero={0};return zero;}
    return stock_base_make_ai_input(me,other,difficulty);
}

/* Shaking freezes frame/gameplay progression. vrest continues to count down;
   attacker-side arest pauses. Stock bdefend still decays once per TU, while
   fall recovery pauses for the full local hitlag interval. Live fighters
   finish the frozen TU in process_frame_state(), so that hook is frozen too. */
static void update_control(fighter_t *f,const input_t *in){
    if(!f)return;
    if(!g_runtime_stock_compat){
        if(g_stock_hitlag_dirty)stock_hitlag_clear_all();
        stock_base_update_control(f,in);
        return;
    }
    stock_hitlag_slot_t *s=stock_hitlag_slot_for(f,0);
    if(!s||s->shaking==0){
        stock_base_update_control(f,in);
        return;
    }
    if(f->bdefend_points>0)f->bdefend_points--;
    if(f->hit_flash>0)f->hit_flash--;
    if(s->shaking<0&&f->arest_ticks>0)f->arest_ticks--;
    for(int ri=0;ri<LF2_MAX_ACTORS;ri++)if(f->vrest_ticks[ri]>0)f->vrest_ticks[ri]--;
    /* KO settlement calls update_control without process_frame_state. */
    if(f->dead)stock_hitlag_advance(s);
}

static void handle_catch_input(fighter_t *fighters,int total,int holder_idx,const input_t *in){
    if(g_runtime_stock_compat&&fighters&&holder_idx>=0&&holder_idx<total&&stock_hitlag_value(&fighters[holder_idx])!=0)return;
    stock_base_handle_catch_input(fighters,total,holder_idx,in);
}

static int fighter_handle_item_input(object_world_t *w,fighter_t *f,input_t *in){
    if(g_runtime_stock_compat&&stock_hitlag_value(f)!=0)return 0;
    return stock_base_fighter_handle_item_input(w,f,in);
}

static void spawn_fighter_opoints(object_world_t *w,fighter_t *fighters,int total,int owner_idx){
    if(g_runtime_stock_compat&&fighters&&owner_idx>=0&&owner_idx<total&&stock_hitlag_value(&fighters[owner_idx])!=0)return;
    stock_base_spawn_fighter_opoints(w,fighters,total,owner_idx);
}

/* Victim hitlag is shared by fighter, held-weapon and object hit paths. */
static int stock_try_defend_hit(fighter_t *b,int attacker_facing,const lf2_itr_def_t *it,int injury,int *damage_out){
    int defended=stock_base_try_defend_hit(b,attacker_facing,it,injury,damage_out);
    if(defended)stock_hitlag_set_victim(b,1);
    return defended;
}

static void stock_note_unblocked_hit(fighter_t *b){
    stock_base_note_unblocked_hit(b);
    if(g_runtime_stock_compat&&b)stock_hitlag_set_victim(b,0);
}

/* Only direct type-0/held-weapon contact freezes the fighter attacker.
   Projectile/object hits intentionally leave the owning fighter running. */
static void apply_hits(object_world_t *world,fighter_t *fighters,int total,int aidx,int bidx,int difficulty){
    uint32_t before=stock_hitlag_victim_serial(&fighters[bidx]);
    stock_base_apply_hits(world,fighters,total,aidx,bidx,difficulty);
    if(g_runtime_stock_compat&&stock_hitlag_victim_serial(&fighters[bidx])!=before)
        stock_hitlag_set_attacker(&fighters[aidx]);
}

static input_t make_ai_retreat_input(fighter_t *me,fighter_t *down){
    if(g_runtime_stock_compat&&stock_hitlag_value(me)!=0){input_t zero={0};return zero;}
    return stock_base_make_ai_retreat_input(me,down);
}

static void process_frame_state(fighter_t *fighters,int total,int idx,object_world_t *world){
    if(g_runtime_stock_compat&&fighters&&idx>=0&&idx<total){
        stock_hitlag_slot_t *s=stock_hitlag_slot_for(&fighters[idx],0);
        if(s&&s->shaking!=0){
            if(!fighters[idx].dead)stock_hitlag_advance(s);
            return;
        }
    }
    stock_base_process_frame_state(fighters,total,idx,world);
}

static int handle_form_input(fighter_t *fighters,int total,int idx,const input_t *in){
    if(g_runtime_stock_compat&&fighters&&idx>=0&&idx<total&&stock_hitlag_value(&fighters[idx])!=0)return 0;
    return stock_base_handle_form_input(fighters,total,idx,in);
}

static uint32_t stock_hitlag_digest_u32(uint32_t h,uint32_t v){
    for(int i=0;i<4;i++){h^=(v>>(i*8))&0xffu;h*=16777619u;}
    return h;
}

static uint32_t stock_state_digest(const fighter_t *fighters,int total,const object_world_t *world,int item_drop_timer){
    uint32_t h=stock_base_state_digest(fighters,total,world,item_drop_timer);
    for(int i=0;i<total;i++)h=stock_hitlag_digest_u32(h,(uint32_t)stock_hitlag_value(&fighters[i]));
    return h;
}
