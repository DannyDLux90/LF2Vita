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
    float x=floorf(f->x-anchor_x*SPRITE_SCALE+0.5f);
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

static int player_side_alive(fighter_t *fighters,int total){
    fighter_t *p=&fighters[0];
    if(p->team==0)return !p->dead;
    for(int i=0;i<total;i++)if(!fighters[i].dead&&fighters[i].team==p->team)return 1;
    return 0;
}

static int player_side_won(fighter_t *fighters,int total){
    return player_side_alive(fighters,total) && !player_has_enemy(fighters,total);
}

static const char *team_name(int team){
    static const char *n[]={"Independent","Team 1","Team 2","Team 3","Team 4"};
    return (team>=0&&team<=4)?n[team]:"Independent";
}

static const char *difficulty_name(int d){
    static const char *n[]={"Easy","Normal","Difficult","CRAZY!"};
    return (d>=0&&d<=3)?n[d]:"Normal";
}

static void draw_match(vita2d_pgf *font,vita2d_texture *bg,fighter_t *fighters,int cpu_count,int difficulty,bool cheat,bool paused,const lf2_match_options_t *options) {
    fighter_t *p=&fighters[0];int total=cpu_count+1;
    vita2d_start_drawing(); vita2d_clear_screen();
    if (bg) vita2d_draw_texture_scale(bg,0,0,960.0f/vita2d_texture_get_width(bg),544.0f/vita2d_texture_get_height(bg));
    vita2d_draw_rectangle(0,310,960,234,RGBA8(9,18,33,64));

    vita2d_draw_rectangle(0,468,960,76,RGBA8(3,8,20,185));
    int order[LF2_MAX_CPUS+1];for(int i=0;i<total;i++)order[i]=i;
    for(int i=0;i<total;i++)for(int j=i+1;j<total;j++)if(fighters[order[i]].z>fighters[order[j]].z){int t=order[i];order[i]=order[j];order[j]=t;}
    for(int i=0;i<total;i++)draw_fighter(&fighters[order[i]]);
    draw_bar(28,25,370,19,(float)p->hp,(float)p->max_hp,RGBA8(55,220,90,255));
    draw_bar(28,48,300,11,(float)p->mp,500,RGBA8(70,145,255,255));
    for(int i=0;i<cpu_count;i++){
        fighter_t *c=&fighters[i+1];float y=18.0f+i*26.0f;
        draw_bar(705,y,225,9,(float)c->hp,(float)c->max_hp,RGBA8(230,65,75,255));
        if(lf2_ui_font_ready())lf2_ui_draw_textf(520,y-7,0.66f,c->dead?RGBA8(170,170,180,255):RGBA8(255,255,255,255),"COM%d %s",i+1,c->def.name);
        else if(font)vita2d_pgf_draw_textf(font,535,(int)y+9,RGBA8(255,255,255,255),0.72f,"COM%d %s",i+1,c->def.name);
    }
    if(font || lf2_ui_font_ready()){
