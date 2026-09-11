
static void draw_fighter(fighter_t *f) {
    if(!f)return;
    draw_character_frame(&f->def,f->frame,f->facing,f->x,f->y,f->z,f->hit_flash,f->def.name);
}

static void draw_stage_corpses(const object_world_t *world){
    if(!world)return;
    int order[LF2_MAX_STAGE_CORPSES],n=0;
    for(int i=0;i<world->corpse_count&&i<LF2_MAX_STAGE_CORPSES;i++)if(world->corpses[i].active&&world->corpses[i].def)order[n++]=i;
    for(int i=0;i<n;i++)for(int j=i+1;j<n;j++)if(world->corpses[order[i]].z>world->corpses[order[j]].z){int t=order[i];order[i]=order[j];order[j]=t;}
    for(int i=0;i<n;i++){
        const stage_corpse_t *c=&world->corpses[order[i]];
        draw_character_frame(c->def,c->frame,c->facing,c->x,c->y,c->z,0,c->def->name);
    }
}


static void draw_object(world_object_t *o){
    if(!o||!o->active||!o->cache)return;lf2_frame_def_t *fr=object_frame_def(o);if(!fr)return;
    lf2_character_def_t *def=&o->cache->def;lf2_sheet_def_t *s=lf2_sheet_for_pic(def,fr->pic);if(!s||!s->texture)return;
    int local=fr->pic-s->first_pic,across=s->row>0?s->row:1,down=s->col>0?s->col:1,col=local%across,row=local/across;
    if(local<0||col<0||col>=across||row<0||row>=down)return;
    int pitchx=s->w+1,pitchy=s->h+1;float tx=(float)(col*pitchx),ty=(float)(row*pitchy),sw=(float)s->w,sh=(float)s->h;
    float tw=(float)vita2d_texture_get_width(s->texture),th=(float)vita2d_texture_get_height(s->texture);if(sw<=0||sh<=0||tw<=0||th<=0||tx+sw>tw||ty+sh>th)return;
    float u0=(tx+0.5f)/tw,u1=(tx+sw-0.5f)/tw,v0=(ty+0.5f)/th,v1=(ty+sh-0.5f)/th;if(o->facing){float t=u0;u0=u1;u1=t;}
    float anchor=o->facing?((sw-1.0f)-(float)fr->centerx):(float)fr->centerx;
    float x=floorf(screen_x(o->x)-anchor+0.5f),y=floorf(GROUND_Y+o->z+o->y-(float)fr->centery+0.5f),x1=x+sw,y1=y+sh;
    vita2d_texture_vertex *v=(vita2d_texture_vertex*)vita2d_pool_memalign(6*sizeof(*v),sizeof(*v));if(!v)return;
    v[0]=(vita2d_texture_vertex){x,y,0.5f,u0,v0};v[1]=(vita2d_texture_vertex){x,y1,0.5f,u0,v1};v[2]=(vita2d_texture_vertex){x1,y,0.5f,u1,v0};
    v[3]=(vita2d_texture_vertex){x,y1,0.5f,u0,v1};v[4]=(vita2d_texture_vertex){x1,y1,0.5f,u1,v1};v[5]=(vita2d_texture_vertex){x1,y,0.5f,u1,v0};
    vita2d_draw_array_textured(s->texture,SCE_GXM_PRIMITIVE_TRIANGLES,v,6,RGBA8(255,255,255,255));
}

static object_cache_entry_t *object_cache_find_loaded(object_world_t *w,int oid){
    if(!w)return NULL;for(int i=0;i<w->cache_count;i++)if(w->cache[i].used&&w->cache[i].oid==oid)return &w->cache[i];return NULL;
}

static int held_item_frame_and_pose(object_world_t *w,const fighter_t *f,
                                    object_cache_entry_t **cache_out,int *frame_out,
                                    float *x_out,float *y_out,int *cover_out){
    if(!w||!f||!f->active||!f->held_oid)return 0;
    object_cache_entry_t *c=object_cache_find_loaded(w,f->held_oid);if(!c)return 0;
    const lf2_frame_def_t *ff=(f->frame>=0&&f->frame<LF2_MAX_FRAMES&&f->def.frames[f->frame].present)?&f->def.frames[f->frame]:NULL;
    int fr=-1;
    if(ff&&ff->wpoint.present){
        int wa=ff->wpoint.weaponact;
        if(wa>=0&&wa<LF2_MAX_FRAMES&&c->def.frames[wa].present)fr=wa;
    }
    if(fr<0)fr=(f->drinking&&c->def.frames[31].present)?31:(item_is_heavy(f->held_oid)&&c->def.frames[10].present?10:20);
    if(fr<0||fr>=LF2_MAX_FRAMES||!c->def.frames[fr].present)return 0;
    const lf2_frame_def_t *wf=&c->def.frames[fr];
    float x=f->x+(f->facing?-29.0f:29.0f),y=f->y-40.0f;
    if(ff&&ff->wpoint.present&&wf->wpoint.present){
        /* Align fighter wpoint(kind 1) with weapon wpoint(kind 2). This is the
           coordinate rule used by the original DAT format. Mirroring must be
           applied to both source points, not just to the weapon texture. */
        if(!f->facing)
            x=f->x-(float)ff->centerx+(float)ff->wpoint.x+(float)wf->centerx-(float)wf->wpoint.x;
        else
            x=f->x+(float)ff->centerx-(float)ff->wpoint.x-(float)wf->centerx+(float)wf->wpoint.x;
        y=f->y-(float)ff->centery+(float)ff->wpoint.y+(float)wf->centery-(float)wf->wpoint.y;
    }
    if(cache_out)*cache_out=c;if(frame_out)*frame_out=fr;if(x_out)*x_out=x;if(y_out)*y_out=y;
    if(cover_out)*cover_out=(ff&&ff->wpoint.present)?ff->wpoint.cover:0;
    return 1;
}

static int held_item_cover(object_world_t *w,const fighter_t *f){
    int cover=0;if(!held_item_frame_and_pose(w,f,NULL,NULL,NULL,NULL,&cover))return 0;return cover!=0;
}

static void draw_held_item(object_world_t *w,const fighter_t *f){
    object_cache_entry_t *c=NULL;int fr=0;float x=0,y=0;
    if(!held_item_frame_and_pose(w,f,&c,&fr,&x,&y,NULL))return;
    world_object_t tmp;memset(&tmp,0,sizeof(tmp));tmp.active=true;tmp.cache=c;tmp.oid=f->held_oid;tmp.type=c->type;tmp.frame=fr;tmp.facing=f->facing;
    tmp.x=x;tmp.y=y;tmp.z=f->z+0.4f;
    draw_object(&tmp);
}

static void draw_bar(float x,float y,float w,float h,float value,float max,unsigned fg) {
    vita2d_draw_rectangle(x,y,w,h,RGBA8(10,13,24,220));
    vita2d_draw_rectangle(x+2,y+2,(w-4)*(max>0?value/max:0),h-4,fg);
}

static int fighter_is_lying(const fighter_t *f){
    if(!f||!f->active||f->dead)return 0;
    if(f->frame<0||f->frame>=LF2_MAX_FRAMES||!f->def.frames[f->frame].present)return 0;
    return f->def.frames[f->frame].state==14;
}

static fighter_t *nearest_living_enemy(fighter_t *me,fighter_t *fighters,int total){
    fighter_t *best=NULL;float bestd=1.0e30f;
    for(int i=0;i<total;i++){
        fighter_t *c=&fighters[i];if(!c->active||c==me||c->dead||!are_enemies(me,c)||fighter_is_lying(c))continue;
        float dx=c->x-me->x,dz=c->z-me->z,d=dx*dx+dz*dz;
        if(d<bestd){best=c;bestd=d;}
    }
    return best;
}

static fighter_t *nearest_lying_enemy(fighter_t *me,fighter_t *fighters,int total){
    fighter_t *best=NULL;float bestd=1.0e30f;
    for(int i=0;i<total;i++){
        fighter_t *c=&fighters[i];if(c==me||!fighter_is_lying(c)||!are_enemies(me,c))continue;
        float dx=c->x-me->x,dz=c->z-me->z,d=dx*dx+dz*dz;if(d<bestd){best=c;bestd=d;}
    }
    return best;
}

static input_t make_ai_retreat_input(fighter_t *me,fighter_t *down){
    input_t in={0};if(!me||!down)return in;float dx=down->x-me->x,dz=down->z-me->z;
    if(fabsf(dx)<180.0f){if(dx<0)in.right=true;else in.left=true;}
    if(fabsf(dz)<34.0f){if(dz<0)in.down=true;else in.up=true;}
    in.run=(fabsf(dx)<130.0f);return in;
}

static int player_has_enemy(fighter_t *fighters,int total){
    fighter_t *p=&fighters[0];
    for(int i=1;i<total;i++)if(fighters[i].active&&!fighters[i].dead&&are_enemies(p,&fighters[i]))return 1;
    return 0;
}

static int player_side_alive(fighter_t *fighters,int total){
    fighter_t *p=&fighters[0];
    if(p->team==0)return p->active&&!p->dead;
    for(int i=0;i<total;i++)if(fighters[i].active&&!fighters[i].dead&&fighters[i].team==p->team)return 1;
    return 0;
}

static int player_side_won(fighter_t *fighters,int total){
    return player_side_alive(fighters,total) && !player_has_enemy(fighters,total);
}

static int dead_bodies_settled(fighter_t *fighters,int total){
    for(int i=0;i<total;i++)if(fighters[i].active&&fighters[i].dead){
        if(fighters[i].y<0.0f||fighters[i].vy!=0.0f||fighters[i].falling)return 0;
        if(fighters[i].frame!=230&&fighters[i].frame!=231)return 0;
    }
    return 1;
}

static const char *team_name(int team){
    static const char *n[]={"Independent","Team 1","Team 2","Team 3","Team 4"};
    return (team>=0&&team<=4)?n[team]:"Independent";
}

static const char *difficulty_name(int d){
    static const char *n[]={"Easy","Normal","Difficult","CRAZY!"};
    return (d>=0&&d<=3)?n[d]:"Normal";
}

static void draw_status_slot(fighter_t *f,int slot){
    int col=slot%4,row=slot/4;float x=(float)(col*240),y=(float)(row*64);unsigned panel=RGBA8(44,78,157,248);
    /* LF2's PC HUD is four columns by two rows.  On 960x544 a 240x64
       slot is almost the exact aspect-preserving scale of the original
       800x600 status board shown by the retail PC game. */
    vita2d_draw_rectangle(x,y,238,62,panel);vita2d_draw_rectangle(x+238,y,2,62,RGBA8(24,48,112,255));vita2d_draw_rectangle(x,y+62,240,2,RGBA8(24,48,112,255));
    float bx=x+64,bw=154;vita2d_draw_rectangle(bx,y+12,bw,14,RGBA8(29,45,93,255));vita2d_draw_rectangle(bx,y+36,bw,11,RGBA8(29,45,93,255));
    if(!f||!f->active)return;
    if(f->hud_face)vita2d_draw_texture(f->hud_face,x+12,y+7);
    vita2d_draw_rectangle(bx+2,y+14,(bw-4)*((float)f->hp/(float)(f->max_hp?f->max_hp:1)),10,RGBA8(240,25,36,255));
    vita2d_draw_rectangle(bx+2,y+38,(bw-4)*((float)f->mp/500.0f),7,RGBA8(25,45,245,255));
    if(lf2_ui_font_ready())lf2_ui_draw_textf(x+12,y+52,0.52f,RGBA8(255,255,255,255),"%s",f->ai?"Com":"P1");
}

static void draw_world_background(vita2d_texture *bg){
    if(!bg)return;
    float tw=(float)vita2d_texture_get_width(bg),th=(float)vita2d_texture_get_height(bg);
    if(tw<=0||th<=0)return;
    float sx=960.0f/tw,sy=544.0f/th;
    if(g_camera_x<=0.01f){
        vita2d_draw_texture_scale(bg,0,0,sx,sy);
        return;
    }
    /* The packaged stage preview is one viewport wide. Until the complete
       bg.dat multi-layer renderer lands, tile that original-derived preview
       and move it at a mild parallax rate. This makes stage phase transitions
       continuous instead of tearing the scene down and reloading it. */
    float tile=tw*sx;
    float off=-fmodf(g_camera_x*0.55f,tile);
    if(off>0)off-=tile;
    for(float x=off;x<960.0f;x+=tile)vita2d_draw_texture_scale(bg,x,0,sx,sy);
}

static void draw_match(vita2d_pgf *font,vita2d_texture *bg,fighter_t *fighters,object_world_t *world,int cpu_count,int difficulty,bool cheat,bool paused,const lf2_match_options_t *options) {
    fighter_t *p=&fighters[0];int total=LF2_MAX_CPUS+1;
    vita2d_start_drawing();vita2d_clear_screen();
    draw_world_background(bg);
    /* Original LF2 reserves a two-row blue status board at the top. */
    vita2d_draw_rectangle(0,0,960,128,RGBA8(37,71,148,255));
    for(int i=0;i<8;i++)draw_status_slot(i<total?&fighters[i]:NULL,i);
    /* Stage corpses are snapshots backed by the shared Stage texture cache.
       Draw them under active fighters/objects so dead enemies persist visually
       even when their logical CPU slot is reused by the next wave. */
    draw_stage_corpses(world);
    int order[LF2_MAX_CPUS+1];for(int i=0;i<total;i++)order[i]=i;for(int i=0;i<total;i++)for(int j=i+1;j<total;j++)if(fighters[order[i]].z>fighters[order[j]].z){int t=order[i];order[i]=order[j];order[j]=t;}
    int oo[LF2_MAX_ACTIVE_OBJECTS],on=0;if(world){for(int i=0;i<LF2_MAX_ACTIVE_OBJECTS;i++)if(world->objects[i].active)oo[on++]=i;for(int i=0;i<on;i++)for(int j=i+1;j<on;j++)if(world->objects[oo[i]].z>world->objects[oo[j]].z){int t=oo[i];oo[i]=oo[j];oo[j]=t;}}
    int fi=0,oi=0;while(fi<total||oi<on){
        if(oi<on&&(fi>=total||world->objects[oo[oi]].z<=fighters[order[fi]].z))draw_object(&world->objects[oo[oi++]]);
        else {
            int idx=order[fi++];
            /* wpoint.cover=1 means the fighter sprite covers the held object. */
            int behind=held_item_cover(world,&fighters[idx]);
            if(behind)draw_held_item(world,&fighters[idx]);
            draw_fighter(&fighters[idx]);
            if(!behind)draw_held_item(world,&fighters[idx]);
        }
    }draw_hit_fx(world);
    if(lf2_ui_font_ready()){
        if(options&&options->hud_label&&options->hud_label[0])lf2_ui_draw_text(724,518,0.58f,RGBA8(255,255,255,255),options->hud_label);
        if(fighters&&world&&fighters[0].active&&!fighters[0].dead){
            fighter_t *p=&fighters[0];
            if(!p->held_oid){
                world_object_t *near=nearest_loose_item(world,p);
                if(near)lf2_ui_draw_textf(350,486,0.56f,RGBA8(255,238,145,255),"L: Pick up %s",item_name(near->oid));
            }else if(item_is_bottle(p->held_oid)){
                lf2_ui_draw_textf(320,486,0.54f,RGBA8(255,238,145,255),"Square: Drink %s   L+Square: Throw",item_name(p->held_oid));
            }else{
                lf2_ui_draw_textf(350,486,0.56f,RGBA8(255,238,145,255),"L+Square: Throw %s",item_name(p->held_oid));
            }
        }
        if(cheat)lf2_ui_draw_text(760,122,0.55f,RGBA8(255,225,80,255),"LF2.NET / CRAZY");
    }
    if((!options||!options->suppress_result_overlay) && (!player_side_alive(fighters,total)||!player_has_enemy(fighters,total))){
        vita2d_draw_rectangle(235,185,490,135,RGBA8(0,0,0,205));const char *msg=player_side_won(fighters,total)?"K.O. - Spieler 1 gewinnt":"K.O. - Spieler 1 ist besiegt";
        if(lf2_ui_font_ready()){float tw=lf2_ui_text_width(0.92f,msg);lf2_ui_draw_text(480.0f-tw*0.5f,216,0.92f,RGBA8(255,255,255,255),msg);if(options&&options->auto_continue)lf2_ui_draw_text(337,268,0.72f,RGBA8(235,242,255,255),"Weiter...");else if(options&&!options->allow_rematch)lf2_ui_draw_text(280,268,0.72f,RGBA8(235,242,255,255),"X: Continue     Circle: Menu");else lf2_ui_draw_text(302,268,0.72f,RGBA8(235,242,255,255),"X: Rematch     Circle: Menu");}
    }else if(paused){vita2d_draw_rectangle(285,190,390,120,RGBA8(0,0,0,215));if(lf2_ui_font_ready()){lf2_ui_draw_text(418,216,1.0f,RGBA8(255,255,255,255),"PAUSE");lf2_ui_draw_text(330,264,0.72f,RGBA8(235,242,255,255),"START: Resume   Circle: Menu");}}
    vita2d_end_drawing();vita2d_swap_buffers();
}

static int load_fighter(fighter_t *f,int roster_idx,bool ai,int team,float x,float z,int facing,int slot,int hp) {
    memset(f,0,sizeof(*f)); f->ai=ai;f->active=true;f->team=team; f->x=x; f->z=z; f->facing=facing;f->owns_assets=true;
    f->roster_idx=roster_idx;f->transform_origin=-1;f->transform_target=-1;f->fusion_partner_slot=-1;f->holding_slot=-1;f->caught_by=-1;f->thrown_by=-1;
    if(hp<=0)hp=500; f->hp=f->max_hp=hp; f->mp=500;
    lf2_log_stage("match:character_parse","slot=%s%d roster=%d name=%s team=%d dat=%s",ai?"CPU":"P",ai?slot:1,roster_idx,lf2_roster[roster_idx].name,team,lf2_roster[roster_idx].dat);
    int rc=lf2_load_character(GAME_ROOT,lf2_roster[roster_idx].dat,&f->def);if(rc<0){lf2_logf("ERROR","character parse failed rc=%d",rc);return -1;}
    rc=lf2_load_character_textures(GAME_ROOT,&f->def);if(rc<0){lf2_logf("ERROR","character textures failed rc=%d",rc);lf2_free_character_textures(&f->def);return -2;}
    if(f->def.small[0]){f->hud_face=lf2_load_bmp_colorkey(f->def.small);if(f->hud_face)vita2d_texture_set_filters(f->hud_face,SCE_GXM_TEXTURE_FILTER_POINT,SCE_GXM_TEXTURE_FILTER_POINT);else lf2_logf("WARN","HUD portrait missing char=%s path=%s",f->def.name,f->def.small);}
    set_frame(f,0); lf2_logf("INFO","fighter ready slot=%s%d name=%s team=%d sheets=%d",ai?"CPU":"P",ai?slot:1,f->def.name,team,f->def.sheet_count); return 0;
}

static int lf2_id_to_roster(int id){
    switch(id){
        case 1:return 0;case 2:return 1;case 4:return 2;case 5:return 3;case 6:return 4;
        case 7:return 5;case 8:return 6;case 9:return 7;case 10:return 8;case 11:return 9;
        case 30:return 10;case 31:return 11;case 32:return 12;case 33:return 13;case 34:return 14;
        case 35:return 15;case 36:return 16;case 37:return 17;case 38:return 19;case 39:return 18;
        case 50:return 20;case 51:return 21;case 52:return 22;default:return -1;
    }
}

/* Reload only the authored type-0 data/texture set. Runtime ownership, team,
   position, HP/MP and transformation metadata stay attached to the same slot. */
static int fighter_change_form(fighter_t *f,int roster_idx,int start_frame,const char *reason){
    if(!f||roster_idx<0||roster_idx>=lf2_roster_count)return -1;
    char old_name[32];snprintf(old_name,sizeof(old_name),"%s",f->def.name[0]?f->def.name:"?");
    if(f->owns_assets){
        vita2d_wait_rendering_done();
        if(f->hud_face){vita2d_free_texture(f->hud_face);f->hud_face=NULL;}
        lf2_free_character_textures(&f->def);
    }else{
        /* Stage slots normally reference shared preloaded assets. Detach the
           shallow copy before loading a private transformed form. */
        f->hud_face=NULL;
    }
    memset(&f->def,0,sizeof(f->def));f->owns_assets=true;
    int rc=lf2_load_character(GAME_ROOT,lf2_roster[roster_idx].dat,&f->def);
    if(rc<0){lf2_logf("ERROR","FORM parse failed %s -> %s rc=%d",old_name,lf2_roster[roster_idx].name,rc);return -2;}
    rc=lf2_load_character_textures(GAME_ROOT,&f->def);
    if(rc<0){lf2_logf("ERROR","FORM textures failed %s rc=%d",lf2_roster[roster_idx].name,rc);return -3;}
    if(f->def.small[0]){f->hud_face=lf2_load_bmp_colorkey(f->def.small);if(f->hud_face)vita2d_texture_set_filters(f->hud_face,SCE_GXM_TEXTURE_FILTER_POINT,SCE_GXM_TEXTURE_FILTER_POINT);}
    f->roster_idx=roster_idx;f->dead=false;f->falling=false;f->frozen_ticks=f->burn_ticks=f->sonata_ticks=0;
    f->hit_latched=false;f->opoint_fired=false;f->invuln=10;f->status_age=0;
    if(start_frame<0||start_frame>=LF2_MAX_FRAMES||!f->def.frames[start_frame].present)start_frame=0;
    set_frame(f,start_frame);f->state_processed_serial=0;
    lf2_logf("FORM","%s -> %s frame=%d reason=%s hp=%d mp=%d",old_name,f->def.name,start_frame,reason?reason:"-",f->hp,f->mp);
    return 0;
}

static void spawn_louis_armor(object_world_t *world,fighter_t *f,int owner_idx){
    if(!world||!f||!f->active)return;
    lf2_frame_def_t *fr=frame_def(f);if(!fr)return;
    /* State 9996 is hard-coded in LF2: four id-217 armour pieces around the
       fighter and one id-218 piece in the centre. Give them small outward
       impulses so the familiar armour burst remains visible on Vita. */
    const int oid[5]={217,217,217,217,218};
    const int dx[5]={-24,24,-24,24,0};
    const int dy[5]={-38,-38,-12,-12,-26};
    const int vx[5]={-5,5,-4,4,0};
    const int vy[5]={-8,-8,-6,-6,-9};
    for(int i=0;i<5;i++){
        lf2_opoint_def_t op;memset(&op,0,sizeof(op));
        op.kind=1;op.oid=oid[i];op.action=0;op.facing=0;
        op.x=fr->centerx+dx[i];op.y=fr->centery+dy[i];op.dvx=vx[i];op.dvy=vy[i];
        object_spawn_one(world,f,owner_idx,&op,0,1);
    }
    lf2_logf("FORM","Louis armour burst state9996 owner=%s",f->def.name);
}
