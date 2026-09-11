static void unload_fighter_slot(fighter_t *f){
    if(!f||!f->active){if(f){memset(f,0,sizeof(*f));f->dead=true;}return;}
    if(f->owns_assets){
        vita2d_wait_rendering_done();
        if(f->hud_face)vita2d_free_texture(f->hud_face);
        lf2_free_character_textures(&f->def);
    }
    memset(f,0,sizeof(*f));f->dead=true;f->active=false;
}

/* Stage waves reuse preloaded read-only fighter assets. In v0.67 every freed
   enemy slot parsed its DAT and synchronously uploaded all sprite sheets again
   inside the simulation tick; the hardware log showed ~100-130 ms stalls at
   wave_advance. Sharing one texture set per roster removes that frame hitch and
   also uses less CDRAM when a wave contains several identical Bandits/Hunters. */
typedef struct {
    bool used;
    int roster_idx;
    lf2_character_def_t def;
    vita2d_texture *hud_face;
} stage_fighter_asset_t;

typedef struct {
    stage_fighter_asset_t *entries;
    int count;
    int cap;
} stage_asset_cache_t;

static stage_fighter_asset_t *stage_asset_find(stage_asset_cache_t *c,int roster_idx){
    if(!c)return NULL;
    for(int i=0;i<c->count;i++)if(c->entries[i].used&&c->entries[i].roster_idx==roster_idx)return &c->entries[i];
    return NULL;
}

static stage_fighter_asset_t *stage_asset_load(stage_asset_cache_t *c,int roster_idx){
    stage_fighter_asset_t *hit=stage_asset_find(c,roster_idx);if(hit)return hit;
    if(!c||!c->entries||c->count>=c->cap||roster_idx<0||roster_idx>=lf2_roster_count)return NULL;
    stage_fighter_asset_t *a=&c->entries[c->count];memset(a,0,sizeof(*a));a->roster_idx=roster_idx;
    int rc=lf2_load_character(GAME_ROOT,lf2_roster[roster_idx].dat,&a->def);
    if(rc<0){lf2_logf("ERROR","stage preload parse failed roster=%d rc=%d",roster_idx,rc);return NULL;}
    rc=lf2_load_character_textures(GAME_ROOT,&a->def);
    if(rc<0){lf2_logf("ERROR","stage preload textures failed roster=%d rc=%d",roster_idx,rc);lf2_free_character_textures(&a->def);return NULL;}
    if(a->def.small[0]){
        a->hud_face=lf2_load_bmp_colorkey(a->def.small);
        if(a->hud_face)vita2d_texture_set_filters(a->hud_face,SCE_GXM_TEXTURE_FILTER_POINT,SCE_GXM_TEXTURE_FILTER_POINT);
    }
    a->used=true;c->count++;
    lf2_logf("STAGE","preload fighter roster=%d name=%s sheets=%d",roster_idx,a->def.name,a->def.sheet_count);
    return a;
}
