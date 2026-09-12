/* Deterministic stock-mode diagnostics. Hash explicit scalar fields only;
   never hash pointers, texture handles, padding or allocator-dependent bytes. */
static uint32_t stock_digest_u32(uint32_t h,uint32_t v){
    for(int i=0;i<4;i++){h^=(v>>(i*8))&0xffu;h*=16777619u;}
    return h;
}

static uint32_t stock_digest_float(uint32_t h,float v){
    uint32_t bits=0;memcpy(&bits,&v,sizeof(bits));return stock_digest_u32(h,bits);
}

static uint32_t stock_state_digest(const fighter_t *fighters,int total,const object_world_t *world,int item_drop_timer){
    uint32_t h=2166136261u;
    h=stock_digest_u32(h,(uint32_t)total);
    h=stock_digest_u32(h,(uint32_t)item_drop_timer);
    h=stock_digest_u32(h,(uint32_t)lf2_rng_stock_i());
    h=stock_digest_u32(h,(uint32_t)lf2_rng_stock_j());
    h=stock_digest_u32(h,(uint32_t)lf2_rng_calls());
    for(int i=0;i<total;i++){
        const fighter_t *f=&fighters[i];
        h=stock_digest_u32(h,(uint32_t)i);
        h=stock_digest_u32(h,f->active?1u:0u);
        h=stock_digest_u32(h,f->dead?1u:0u);
        h=stock_digest_u32(h,(uint32_t)f->roster_idx);
        h=stock_digest_u32(h,(uint32_t)f->team);
        h=stock_digest_u32(h,(uint32_t)f->human_no);
        h=stock_digest_u32(h,(uint32_t)f->hp);
        h=stock_digest_u32(h,(uint32_t)f->max_hp);
        h=stock_digest_u32(h,(uint32_t)f->mp);
        h=stock_digest_u32(h,(uint32_t)f->frame);
        h=stock_digest_u32(h,(uint32_t)f->ticks);
        h=stock_digest_u32(h,(uint32_t)f->facing);
        h=stock_digest_float(h,f->x);h=stock_digest_float(h,f->y);h=stock_digest_float(h,f->z);
        h=stock_digest_float(h,f->vx);h=stock_digest_float(h,f->vy);h=stock_digest_float(h,f->vz);
        h=stock_digest_u32(h,(uint32_t)f->fall_points);
        h=stock_digest_u32(h,(uint32_t)f->bdefend_points);
        h=stock_digest_u32(h,f->falling?1u:0u);
        h=stock_digest_u32(h,(uint32_t)f->arest_ticks);
        for(int r=0;r<LF2_MAX_ACTORS;r++)h=stock_digest_u32(h,(uint32_t)f->vrest_ticks[r]);
        h=stock_digest_u32(h,(uint32_t)f->holding_slot);
        h=stock_digest_u32(h,(uint32_t)f->caught_by);
        h=stock_digest_u32(h,(uint32_t)f->thrown_by);
        h=stock_digest_u32(h,(uint32_t)f->frozen_ticks);
        h=stock_digest_u32(h,(uint32_t)f->burn_ticks);
        h=stock_digest_u32(h,(uint32_t)f->sonata_ticks);
        h=stock_digest_u32(h,(uint32_t)f->held_oid);
        h=stock_digest_u32(h,(uint32_t)f->held_units);
        h=stock_digest_u32(h,(uint32_t)f->held_weapon_hp);
        h=stock_digest_u32(h,f->drinking?1u:0u);
    }
    if(world){
        for(int i=0;i<LF2_MAX_ACTIVE_OBJECTS;i++){
            const world_object_t *o=&world->objects[i];
            h=stock_digest_u32(h,(uint32_t)i);
            h=stock_digest_u32(h,o->active?1u:0u);
            if(!o->active)continue;
            h=stock_digest_u32(h,(uint32_t)o->oid);
            h=stock_digest_u32(h,(uint32_t)o->type);
            h=stock_digest_u32(h,(uint32_t)o->owner);
            h=stock_digest_u32(h,(uint32_t)o->team);
            h=stock_digest_u32(h,(uint32_t)o->frame);
            h=stock_digest_u32(h,(uint32_t)o->ticks);
            h=stock_digest_u32(h,(uint32_t)o->age);
            h=stock_digest_u32(h,(uint32_t)o->facing);
            h=stock_digest_float(h,o->x);h=stock_digest_float(h,o->y);h=stock_digest_float(h,o->z);
            h=stock_digest_float(h,o->vx);h=stock_digest_float(h,o->vy);h=stock_digest_float(h,o->vz);
            h=stock_digest_u32(h,(uint32_t)o->arest_ticks);
            for(int r=0;r<LF2_MAX_ACTORS;r++)h=stock_digest_u32(h,(uint32_t)o->vrest_ticks[r]);
            h=stock_digest_u32(h,(uint32_t)o->target_slot);
            h=stock_digest_u32(h,(uint32_t)o->type3_timer);
            h=stock_digest_u32(h,(uint32_t)o->type3_timer_code);
            h=stock_digest_u32(h,o->attached?1u:0u);
            h=stock_digest_u32(h,o->pickup_item?1u:0u);
            h=stock_digest_u32(h,o->recoverable_weapon?1u:0u);
            h=stock_digest_u32(h,o->grounded?1u:0u);
            h=stock_digest_u32(h,(uint32_t)o->durability);
            h=stock_digest_u32(h,(uint32_t)o->weapon_hp);
        }
    }
    return h;
}
