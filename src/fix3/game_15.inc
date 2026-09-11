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
static void draw_character_frame(const lf2_character_def_t *def,int frame,int facing,
                                 float wx,float wy,float wz,int hit_flash,const char *name) {
    if(!def||frame<0||frame>=LF2_MAX_FRAMES||!def->frames[frame].present)return;
    lf2_frame_def_t *fr=(lf2_frame_def_t*)&def->frames[frame];
    lf2_sheet_def_t *s=lf2_sheet_for_pic((lf2_character_def_t*)def,fr->pic);if(!s||!s->texture)return;

    int local=fr->pic-s->first_pic;
    int across=s->row>0?s->row:1;
    int down=s->col>0?s->col:1;
    int col=local%across,row=local/across;
    if(local<0||col<0||col>=across||row<0||row>=down){
        static int warned=0;
        if(warned<12){lf2_logf("RENDER","invalid sprite cell char=%s frame=%d pic=%d local=%d layout=%dx%d",name?name:def->name,frame,fr->pic,local,across,down);warned++;}
        return;
    }

    int pitchx=s->w+1,pitchy=s->h+1;
    float tx=(float)(col*pitchx),ty=(float)(row*pitchy);
    float sw=(float)s->w,sh=(float)s->h;
    float tw=(float)vita2d_texture_get_width(s->texture),th=(float)vita2d_texture_get_height(s->texture);
    if(sw<=0||sh<=0||tw<=0||th<=0)return;
    if(tx+sw>tw || ty+sh>th){
        static int bounds_warned=0;
        if(bounds_warned<16){lf2_logf("RENDER","sprite rect outside bitmap char=%s frame=%d pic=%d rect=%.0f,%.0f %.0fx%.0f tex=%.0fx%.0f",name?name:def->name,frame,fr->pic,tx,ty,sw,sh,tw,th);bounds_warned++;}
        return;
    }

    float u0=(tx+0.5f)/tw,u1=(tx+sw-0.5f)/tw;
    float v0=(ty+0.5f)/th,v1=(ty+sh-0.5f)/th;
    if(facing){float t=u0;u0=u1;u1=t;}
    float anchor_x=facing?((sw-1.0f)-(float)fr->centerx):(float)fr->centerx;
    float x=floorf(screen_x(wx)-anchor_x*SPRITE_SCALE+0.5f);
    float y=floorf(GROUND_Y+wz+wy-(float)fr->centery*SPRITE_SCALE+0.5f);
    float x1=x+sw*SPRITE_SCALE,y1=y+sh*SPRITE_SCALE;

    vita2d_texture_vertex *v=(vita2d_texture_vertex*)vita2d_pool_memalign(6*sizeof(*v),sizeof(*v));
    if(!v){
        static int pool_warned=0;
        if(!pool_warned){lf2_logf("ERROR","vita2d vertex pool exhausted while drawing %s",name?name:def->name);pool_warned=1;}
        return;
    }
    v[0]=(vita2d_texture_vertex){x, y, 0.5f, u0, v0};
    v[1]=(vita2d_texture_vertex){x, y1,0.5f, u0, v1};
    v[2]=(vita2d_texture_vertex){x1,y, 0.5f, u1, v0};
    v[3]=(vita2d_texture_vertex){x, y1,0.5f, u0, v1};
    v[4]=(vita2d_texture_vertex){x1,y1,0.5f, u1, v1};
    v[5]=(vita2d_texture_vertex){x1,y, 0.5f, u1, v0};
    unsigned tint=hit_flash?RGBA8(255,205,205,255):RGBA8(255,255,255,255);
    vita2d_draw_array_textured(s->texture,SCE_GXM_PRIMITIVE_TRIANGLES,v,6,tint);
}

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
