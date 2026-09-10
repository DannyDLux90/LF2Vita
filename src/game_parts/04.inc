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
    float anchor_x=f->facing?(sw-(float)fr->centerx):(float)fr->centerx;
    float x=f->x-anchor_x*SPRITE_SCALE;
    float y=GROUND_Y+f->z+f->y-(float)fr->centery*SPRITE_SCALE;
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
    vita2d_draw_array_textured(s->texture,SCE_GXM_PRIMITIVE_TRIANGLES,v,6,RGBA8(255,255,255,255));
}

static void draw_bar(float x,float y,float w,float h,float value,float max,unsigned fg) {
    vita2d_draw_rectangle(x,y,w,h,RGBA8(10,13,24,220));
    vita2d_draw_rectangle(x+2,y+2,(w-4)*(max>0?value/max:0),h-4,fg);
}

static fighter_t *nearest_living_enemy(fighter_t *me,fighter_t *fighters,int total){
    fighter_t *best=NULL;float bestd=1.0e30f;
    for(int i=0;i<total;i++){
        fighter_t *c=&fighters[i];if(c==me||c->dead||!are_enemies(me,c))continue;
        float dx=c->x-me->x,dz=c->z-me->z,d=dx*dx+dz*dz;
        if(d<bestd){best=c;bestd=d;}
    }
    return best;
}

static int player_has_enemy(fighter_t *fighters,int total){
    fighter_t *p=&fighters[0];
    for(int i=1;i<total;i++)if(!fighters[i].dead&&are_enemies(p,&fighters[i]))return 1;
    return 0;
}

static const char *team_name(int team){
    static const char *n[]={"Independent","Team 1","Team 2","Team 3","Team 4"};
    return (team>=0&&team<=4)?n[team]:"Independent";
}

static const char *difficulty_name(int d){
    static const char *n[]={"Easy","Normal","Difficult","CRAZY!"};
    return (d>=0&&d<=3)?n[d]:"Normal";
}

static void draw_match(vita2d_pgf *font,vita2d_texture *bg,fighter_t *fighters,int cpu_count,int difficulty,bool cheat,bool paused) {
    fighter_t *p=&fighters[0];int total=cpu_count+1;
    vita2d_start_drawing(); vita2d_clear_screen();
    if (bg) vita2d_draw_texture_scale(bg,0,0,960.0f/vita2d_texture_get_width(bg),544.0f/vita2d_texture_get_height(bg));
    vita2d_draw_rectangle(0,310,960,234,RGBA8(9,18,33,64));
