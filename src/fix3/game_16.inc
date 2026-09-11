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
