    return c;
}

static lf2_frame_def_t *object_frame_def(world_object_t *o){
    if(!o||!o->active||!o->cache||o->frame<0||o->frame>=LF2_MAX_FRAMES||!o->cache->def.frames[o->frame].present)return NULL;
    return &o->cache->def.frames[o->frame];
}

static void object_set_frame(world_object_t *o,int id){
    if(!o||!o->cache)return;
    if(id<0||id>=LF2_MAX_FRAMES||!o->cache->def.frames[id].present){o->active=false;return;}
    o->frame=id;o->ticks=0;o->opoint_fired=false;
    lf2_frame_def_t *fr=&o->cache->def.frames[id];
    if(fr->dvx==550)o->vx=0;else if(fr->dvx)o->vx=(float)(o->facing?-fr->dvx:fr->dvx);
    if(fr->dvy==550)o->vy=0;else if(fr->dvy)o->vy=(float)fr->dvy;
    if(fr->dvz==550)o->vz=0;else if(fr->dvz)o->vz=(float)fr->dvz;
    if(fr->sound[0])lf2_audio_play(fr->sound);
}

static world_object_t *world_alloc_object(object_world_t *w){
    if(!w)return NULL;
    for(int i=0;i<LF2_MAX_ACTIVE_OBJECTS;i++)if(!w->objects[i].active){memset(&w->objects[i],0,sizeof(w->objects[i]));w->objects[i].active=true;return &w->objects[i];}
    static int warned=0;if(!warned++){lf2_logf("WARN","active object pool full (%d)",LF2_MAX_ACTIVE_OBJECTS);}return NULL;
}

static void object_spawn_one(object_world_t *w,fighter_t *owner,int owner_idx,const lf2_opoint_def_t *op,int variant,int count){
    if(!w||!owner||!op||op->oid<=0)return;
    object_cache_entry_t *cache=object_cache_get(w,op->oid);if(!cache)return;
    world_object_t *o=world_alloc_object(w);if(!o)return;
    o->cache=cache;o->oid=op->oid;o->type=cache->type;o->owner=owner_idx;o->team=owner->team;
    int backwards=(op->facing%10)==1;
    if(op->facing==10)o->facing=0;else o->facing=owner->facing^(backwards?1:0);
    lf2_frame_def_t *fr=frame_def(owner);if(!fr){o->active=false;return;}
    if(!owner->facing)o->x=owner->x-(float)fr->centerx+(float)op->x;
    else o->x=owner->x+1.0f+(float)fr->centerx-(float)op->x;
    o->y=owner->y-(float)fr->centery+(float)op->y;o->z=owner->z;
    object_set_frame(o,op->action);
    if(!o->active)return;
    /* Explicit opoint launch speeds override values in the first object frame. */
    if(op->dvx)o->vx=(float)(o->facing?-op->dvx:op->dvx);
    if(op->dvy)o->vy=(float)op->dvy;
    if(op->dvz)o->vz=(float)(o->facing?-op->dvz:op->dvz);
    /* LF2's facing: N0 creates N copies. Henry's five-arrow shot relies on
       those copies being spatially distinct; spread them symmetrically while
       preserving the authored centre arrow. */
    if(count>1){float mid=(float)(count-1)*0.5f,off=(float)variant-mid;o->vy+=off*0.85f;o->z+=off*4.0f;}
    lf2_logf("OBJECT","spawn oid=%d type=%d owner=%d action=%d facing=%d copy=%d/%d pos=%.1f,%.1f,%.1f vel=%.1f,%.1f,%.1f",
             o->oid,o->type,owner_idx,o->frame,o->facing,variant+1,count,o->x,o->y,o->z,o->vx,o->vy,o->vz);
}

static void spawn_fighter_opoints(object_world_t *w,fighter_t *f,int owner_idx){
    if(!w||!f||f->dead||f->opoint_fired)return;lf2_frame_def_t *fr=frame_def(f);if(!fr||fr->opoint_count<=0)return;
    f->opoint_fired=true;
    for(int i=0;i<fr->opoint_count;i++){
        const lf2_opoint_def_t *op=&fr->opoints[i];
        if(op->kind==2){lf2_logf("OBJECT","held weapon opoint deferred owner=%s oid=%d action=%d",f->def.name,op->oid,op->action);continue;}
        int count=(op->facing>=20)?op->facing/10:1;if(count<1)count=1;if(count>8)count=8;
        for(int n=0;n<count;n++)object_spawn_one(w,f,owner_idx,op,n,count);
    }
}

static void object_region_world(const world_object_t *o,int rx,int ry,int rw,int rh,int *x,int *y){
    lf2_frame_def_t *fr=(lf2_frame_def_t*)&o->cache->def.frames[o->frame];
    if(!o->facing)*x=(int)o->x-fr->centerx+rx;else *x=(int)o->x+1+fr->centerx-rw-rx;
    *y=(int)o->y-fr->centery+ry;
}

static fighter_t *object_nearest_enemy(world_object_t *o,fighter_t *fighters,int total){
    fighter_t *best=NULL;float bd=1.0e30f;if(!o)return NULL;
    for(int i=0;i<total;i++){fighter_t *f=&fighters[i];if(f->dead||i==o->owner)continue;if(o->team&&f->team==o->team)continue;float dx=f->x-o->x,dz=f->z-o->z,d=dx*dx+dz*dz;if(d<bd){bd=d;best=f;}}
    return best;
}

static void update_object(object_world_t *w,world_object_t *o,fighter_t *fighters,int total){
    (void)w;if(!o||!o->active)return;lf2_frame_def_t *fr=object_frame_def(o);if(!fr){o->active=false;return;}
    o->age++;
    /* hit_Fa on a ball is LF2's chase flag. Apply a conservative homing turn
       rather than teleporting so Dennis/John-style chase objects remain visible. */
    if(o->type==3&&fr->hit_Fa>0){fighter_t *t=object_nearest_enemy(o,fighters,total);if(t){float dx=t->x-o->x,dy=(t->y-35.0f)-o->y,dz=t->z-o->z;float len=sqrtf(dx*dx+dy*dy+dz*dz);if(len>1.0f){float speed=sqrtf(o->vx*o->vx+o->vy*o->vy+o->vz*o->vz);if(speed<4.0f)speed=7.0f;float turn=fr->hit_Fa>=7?0.20f:0.11f;o->vx=o->vx*(1.0f-turn)+dx/len*speed*turn;o->vy=o->vy*(1.0f-turn)+dy/len*speed*turn;o->vz=o->vz*(1.0f-turn)+dz/len*speed*turn;}}}
    o->x+=o->vx;o->y+=o->vy;o->z+=o->vz;
    if(o->type==1&&fr->state==1002)o->vy+=0.45f;
    if(o->type==1&&o->y>=2.0f&&fr->state==1002){if(o->cache->def.frames[60].present)object_set_frame(o,60);else o->active=false;o->y=0.0f;}
    if(!o->active)return;
    if(o->x<-180||o->x>1140||o->z<-180||o->z>180||o->y>180||o->age>420){o->active=false;return;}
    fr=object_frame_def(o);if(!fr)return;o->ticks++;int duration=fr->wait+1;if(duration<1)duration=1;if(o->ticks<duration)return;
    int next=fr->next;if(next<0){o->facing^=1;next=-next;}
    if(next==999)next=0;else if(next==1000){o->active=false;return;}
    object_set_frame(o,next);
}

static void object_apply_hits(world_object_t *o,fighter_t *fighters,int total,int difficulty){
    if(!o||!o->active)return;lf2_frame_def_t *of=object_frame_def(o);if(!of||of->itr_count<=0)return;
    for(int target=0;target<total;target++){
        fighter_t *b=&fighters[target];if(b->dead||(o->hit_mask&(1u<<target)))continue;
        if(o->team!=0&&b->team==o->team)continue;
        lf2_frame_def_t *bf=frame_def(b);if(!bf||bf->bdy_count<=0)continue;
        for(int ii=0;ii<of->itr_count;ii++){
            lf2_itr_def_t *it=&of->itrs[ii];if(it->kind!=0&&it->kind!=5&&it->kind!=6&&it->kind!=8&&it->kind!=9)continue;
            bool healing=(it->kind==8);if(healing){if(target!=o->owner&&(o->team==0||b->team!=o->team))continue;}else {if(target==o->owner||b->invuln>0)continue;if(o->team!=0&&b->team==o->team)continue;}
            int zw=it->zwidth?it->zwidth:20;if(fabsf(o->z-b->z)>=zw)continue;int ax,ay;object_region_world(o,it->x,it->y,it->w,it->h,&ax,&ay);
            for(int j=0;j<bf->bdy_count;j++){lf2_bdy_def_t *bd=&bf->bdys[j];int bx,by;region_world(b,bd->x,bd->y,bd->w,bd->h,&bx,&by);if(!rect_overlap_i(ax,ay,it->w,it->h,bx,by,bd->w,bd->h))continue;
                o->hit_mask|=(1u<<target);
                if(healing){int heal=it->injury>0?it->injury:50;int old=b->hp;b->hp+=heal;if(b->hp>b->max_hp)b->hp=b->max_hp;lf2_logf("OBJECT","heal oid=%d target=%s hp=%d->%d",o->oid,b->def.name,old,b->hp);}
                else {int dmg=it->injury>0?it->injury:1;if(o->owner>=0&&o->owner<total&&fighters[o->owner].ai){if(difficulty==LF2_DIFF_EASY)dmg=dmg*4/5;else if(difficulty==LF2_DIFF_DIFFICULT)dmg=dmg*11/10;else if(difficulty==LF2_DIFF_CRAZY)dmg=dmg*5/4;}if(b->frame==110)dmg=(dmg+3)/4;if(dmg<1)dmg=1;int old=b->hp;b->hp-=dmg;if(b->hp<0)b->hp=0;b->invuln=(it->effect==3)?32:18;b->vx=(float)((o->facing?-1:1)*(it->dvx?it->dvx:4));if(it->dvy)b->vy=(float)it->dvy*0.65f;if(b->hp==0){b->dead=true;set_frame(b,b->def.frames[230].present?230:220);}else set_frame(b,b->def.frames[220].present?220:0);lf2_logf("OBJECT","hit oid=%d owner=%d target=%s hp=%d->%d effect=%d state=%d",o->oid,o->owner,b->def.name,old,b->hp,it->effect,of->state);}
                if(o->type==1){o->active=false;return;}
                if(of->state==3000){if(o->cache->def.frames[10].present)object_set_frame(o,10);else o->active=false;return;}
                /* 3005/3006 are penetrating objects and remain active. */
                break;
            }
        }
    }
}

static void world_clear_active(object_world_t *w){if(!w)return;memset(w->objects,0,sizeof(w->objects));}
static void world_shutdown(object_world_t *w){if(!w)return;vita2d_wait_rendering_done();for(int i=0;i<w->cache_count;i++)if(w->cache[i].used)lf2_free_character_textures(&w->cache[i].def);memset(w,0,sizeof(*w));}

static void apply_hits(fighter_t *a, fighter_t *b, int difficulty) {
    if (!are_enemies(a,b) || a->dead || b->dead || a->hit_latched || b->invuln>0) return;
    lf2_frame_def_t *af=frame_def(a), *bf=frame_def(b);
    if (!af || !bf || af->itr_count==0 || bf->bdy_count==0) return;
    for (int i=0;i<af->itr_count;i++) {
        lf2_itr_def_t *it=&af->itrs[i];
        if (it->kind!=0 && it->kind!=5 && it->kind!=6) continue;
        int zw=it->zwidth?it->zwidth:14;
        if (fabsf(a->z-b->z)>=zw) continue;
        int ax,ay; region_world(a,it->x,it->y,it->w,it->h,&ax,&ay);
        for (int j=0;j<bf->bdy_count;j++) {
            lf2_bdy_def_t *bd=&bf->bdys[j]; int bx,by;
            region_world(b,bd->x,bd->y,bd->w,bd->h,&bx,&by);
            if (!rect_overlap_i(ax,ay,it->w,it->h,bx,by,bd->w,bd->h)) continue;
            int dmg=it->injury>0?it->injury:18;
            if(a->ai){
                if(difficulty==LF2_DIFF_EASY)dmg=(dmg*4)/5;
                else if(difficulty==LF2_DIFF_DIFFICULT)dmg=(dmg*11)/10;
                else if(difficulty==LF2_DIFF_CRAZY)dmg=(dmg*5)/4;
            }
            if (b->frame==110) dmg=(dmg+3)/4;
            if(dmg<1)dmg=1;
            b->hp-=dmg; if(b->hp<0)b->hp=0;
            b->invuln=18;
            b->vx=(float)((a->facing? -1:1)*(it->dvx?it->dvx:3));
            if (it->dvy) b->vy=(float)it->dvy*0.65f;
            if (b->hp==0) { b->dead=true; set_frame(b,b->def.frames[230].present?230:220); }
            else set_frame(b,b->def.frames[220].present?220:0);
            a->hit_latched=true;
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
