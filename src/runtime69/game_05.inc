                if(a->roster_idx==3)a->transform_target=b->roster_idx;
                lf2_logf("GRAB","%s caught %s catching=%d caught=%d transform_target=%d",a->def.name,b->def.name,ca,cb,a->transform_target);
                return;
            }
            if(it->kind==10||it->kind==11){
                apply_sonata(b,it,it->kind);
                return;
            }
            if(it->kind==15){
                b->vx+=(a->x<b->x)?2.5f:-2.5f;b->vy=-2.0f;b->falling=true;
                if(b->def.frames[180].present)set_frame(b,180);
                lf2_logf("STATUS","%s wind lift by %s",b->def.name,a->def.name);
                return;
            }
            if(it->kind==16){
                apply_elemental_status(b,3,a->facing,it);
                if(it->arest>0)a->arest_ticks=it->arest;if(it->vrest>0)a->vrest_ticks[bidx]=it->vrest;
                return;
            }

            int effect=it->effect;
            int dmg=it->injury>0?it->injury:18;
            /* Freeze's summoned Ice Sword is a held-weapon mechanic in the
               PC executable. The Vita runtime keeps the sword attached and
               gives melee contact its ice property while it is equipped. */
            if(a->ice_sword_ticks>0 && effect==0){effect=3;dmg+=10;}
            if(a->ai){
                if(difficulty==LF2_DIFF_EASY)dmg=(dmg*4)/5;
                else if(difficulty==LF2_DIFF_DIFFICULT)dmg=(dmg*11)/10;
                else if(difficulty==LF2_DIFF_CRAZY)dmg=(dmg*5)/4;
            }
            if (b->frame==110) dmg=(dmg+3)/4;
            if(dmg<1)dmg=1;
            b->hp-=dmg; if(b->hp<0)b->hp=0;
            if(b->hp==0)b->dead=true;
            int status_applied=(!b->dead)?apply_elemental_status(b,effect,a->facing,it):0;
            if(!status_applied&&!apply_caught_hurt_frame(b,a->facing))apply_hit_reaction(b,a->facing,it);
            play_hit_sound(effect,it->fall?it->fall:20,dmg);
            spawn_hit_fx(world,b,effect);
            if(it->arest>0)a->arest_ticks=it->arest;if(it->vrest>0)a->vrest_ticks[bidx]=it->vrest;
            return;
        }
    }
}
static void draw_fighter(fighter_t *f) {
    lf2_frame_def_t *fr=frame_def(f);if(!fr)return;
    lf2_sheet_def_t *s=lf2_sheet_for_pic(&f->def,fr->pic);if(!s||!s->texture)return;

    int local=fr->pic-s->first_pic;
    /* LF2 DAT naming is counter-intuitive: "row" is the number of cells
       across the bitmap, while "col" is the number of cells vertically.
       v0.50 used col as the horizontal count, selecting the wrong source
       rectangle for most frame pictures. */
    int across=s->row>0?s->row:1;
    int down=s->col>0?s->col:1;
    int col=local%across,row=local/across;
    if(local<0||col<0||col>=across||row<0||row>=down){
        static int warned=0;
        if(warned<12){lf2_logf("RENDER","invalid sprite cell char=%s frame=%d pic=%d local=%d layout=%dx%d",f->def.name,f->frame,fr->pic,local,across,down);warned++;}
        return;
    }

    int pitchx=s->w+1,pitchy=s->h+1;
    float tx=(float)(col*pitchx),ty=(float)(row*pitchy);
    /* w/h are the visible image size. The extra pixel in each cell is the
       separator and must never be sampled; including it created vertical /
       horizontal garbage lines between animation frames. */
    float sw=(float)s->w,sh=(float)s->h;
    float tw=(float)vita2d_texture_get_width(s->texture),th=(float)vita2d_texture_get_height(s->texture);
    if(sw<=0||sh<=0||tw<=0||th<=0)return;
    if(tx+sw>tw || ty+sh>th){
        static int bounds_warned=0;
        if(bounds_warned<16){lf2_logf("RENDER","sprite rect outside bitmap char=%s frame=%d pic=%d rect=%.0f,%.0f %.0fx%.0f tex=%.0fx%.0f",f->def.name,f->frame,fr->pic,tx,ty,sw,sh,tw,th);bounds_warned++;}
        return;
    }

    /* Half-texel inset + point filtering keeps adjacent cells from bleeding
       into the selected frame when sprites are scaled on the Vita. */
    float u0=(tx+0.5f)/tw,u1=(tx+sw-0.5f)/tw;
    float v0=(ty+0.5f)/th,v1=(ty+sh-0.5f)/th;
    if(f->facing){float t=u0;u0=u1;u1=t;}

    /* centerx is measured from the left edge in the source-facing sprite.
       Mirroring changes the anchor to (width-centerx). Without this correction
       fighters visibly jumped sideways every time they turned around. */
    float anchor_x=f->facing?((sw-1.0f)-(float)fr->centerx):(float)fr->centerx;
    /* Keep LF2 pixel art on integer destination pixels. Non-integer 1.28x
       scaling in v0.60 made nearest-neighbour pixels change width while the
       fighter moved, which looked like flickering/corrupt animation on Vita. */
    float x=floorf(screen_x(f->x)-anchor_x*SPRITE_SCALE+0.5f);
    float y=floorf(GROUND_Y+f->z+f->y-(float)fr->centery*SPRITE_SCALE+0.5f);
    float x1=x+sw*SPRITE_SCALE,y1=y+sh*SPRITE_SCALE;

    vita2d_texture_vertex *v=(vita2d_texture_vertex*)vita2d_pool_memalign(6*sizeof(*v),sizeof(*v));
    if(!v){
        static int pool_warned=0;
        if(!pool_warned){lf2_logf("ERROR","vita2d vertex pool exhausted while drawing %s",f->def.name);pool_warned=1;}
        return;
    }
    v[0]=(vita2d_texture_vertex){x, y, 0.5f, u0, v0};
    v[1]=(vita2d_texture_vertex){x, y1,0.5f, u0, v1};
    v[2]=(vita2d_texture_vertex){x1,y, 0.5f, u1, v0};
    v[3]=(vita2d_texture_vertex){x, y1,0.5f, u0, v1};
    v[4]=(vita2d_texture_vertex){x1,y1,0.5f, u1, v1};
    v[5]=(vita2d_texture_vertex){x1,y, 0.5f, u1, v0};
    unsigned tint=f->hit_flash?RGBA8(255,205,205,255):RGBA8(255,255,255,255);
    vita2d_draw_array_textured(s->texture,SCE_GXM_PRIMITIVE_TRIANGLES,v,6,tint);
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
                lf2_ui_draw_textf(350,486,0.56f,RGBA8(255,238,145,255),"Hold Square: Drink %s",item_name(p->held_oid));
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
