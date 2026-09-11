static void start_attack(fighter_t *f) {
    f->punch_variant ^= 1;
    f->hit_latched=false;
    /* Light weapons have dedicated type-0 attack frames whose wpoint selects
       the correct weapon picture for every swing. Heavy objects still use the
       conservative punch path until their throw/release state is emulated. */
    if(f->held_oid&&!item_is_bottle(f->held_oid)&&!item_is_heavy(f->held_oid)){