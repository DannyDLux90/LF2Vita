    if(difficulty<LF2_DIFF_EASY)difficulty=LF2_DIFF_EASY;
    if(difficulty>LF2_DIFF_CRAZY)difficulty=LF2_DIFF_CRAZY;
    float closex=close_by_diff[difficulty];
    if (fabsf(dz)>12.0f) { if (dz<0) in.up=true; else in.down=true; }
    if (fabsf(dx)>closex) { if (dx<0) in.left=true; else in.right=true; }
    if (me->ai_cooldown>0) me->ai_cooldown--;
    if (fabsf(dx)<=closex && fabsf(dz)<25.0f && me->ai_cooldown==0 && !in_action(me)) {
        int special_mod=(difficulty==LF2_DIFF_CRAZY)?4:(difficulty==LF2_DIFF_DIFFICULT?7:12);
        if (difficulty>=LF2_DIFF_DIFFICULT && me->mp>120 && (rand()%special_mod)==0)
            in.special_pressed=in.special=true;
        else
            in.attack_pressed=in.attack=true;
        me->ai_cooldown=cooldown_by_diff[difficulty];
    }
    return in;
}

static void update_control(fighter_t *f, const input_t *in) {
    if (f->dead) return;
    if (f->invuln>0) f->invuln--;
    bool locked=in_action(f)||f->frame==110||f->y<0.0f;
    if(!locked){
        int cmd=resolve_command_frame(f,in);
        if(cmd>0)start_special_frame(f,cmd);
        else if(in->attack_pressed)start_attack(f);
        else if(in->defend_pressed)set_frame(f,110);
        else if(in->jump_pressed){
            set_frame(f,210);f->vy=f->def.jump_height;
            if(in->left)f->vx=-f->def.jump_distance;if(in->right)f->vx=f->def.jump_distance;
            if(in->up)f->vz=-f->def.jump_distancez;if(in->down)f->vz=f->def.jump_distancez;
        } else {
            bool running=in->run&&f->def.frames[9].present;
            float sx=(running?f->def.running_speed:f->def.walking_speed);
            float sz=(running?f->def.running_speedz:f->def.walking_speedz);bool moving=false;
            if(in->left){f->x-=sx;f->facing=1;moving=true;}if(in->right){f->x+=sx;f->facing=0;moving=true;}
            if(in->up){f->z-=sz;moving=true;}if(in->down){f->z+=sz;moving=true;}
            if(moving){
                f->walk_phase++;int first=running?9:5,last=running?11:8,count=last-first+1;
                float rate=running?f->def.running_frame_rate:f->def.walking_frame_rate;int period=(int)(rate+0.5f);if(period<1)period=1;
                int wf=first+(f->walk_phase/period)%count;if(f->frame<first||f->frame>last||f->frame!=wf)set_frame(f,wf);
            }else if((f->frame>=5&&f->frame<=11))set_frame(f,0);
        }
    }else if(f->frame==110&&!in->defend&&f->ticks>4)set_frame(f,0);

    if(f->y<0.0f||f->vy!=0.0f){
        f->y+=f->vy;f->vy+=1.70f;f->x+=f->vx;f->z+=f->vz;f->vx*=0.90f;f->vz*=0.88f;
        if(f->y>=0.0f){f->y=0;f->vy=0;f->vx=0;f->vz=0;if(!in_action(f))set_frame(f,0);}
    }else if(in_action(f)){f->x+=f->vx;f->z+=f->vz;f->vx*=0.72f;f->vz*=0.72f;}
    f->x=clampf(f->x,40.0f,920.0f);f->z=clampf(f->z,-70.0f,75.0f);if(f->mp<500)f->mp++;update_animation(f,in);
}

static bool rect_overlap_i(int ax,int ay,int aw,int ah,int bx,int by,int bw,int bh) {
    return lf2_rect_overlap(ax,ay,(unsigned)aw,(unsigned)ah,bx,by,(unsigned)bw,(unsigned)bh);
}

static void region_world(const fighter_t *f,int rx,int ry,int rw,int rh,int *x,int *y) {
    lf2_frame_def_t *fr=(lf2_frame_def_t*)&f->def.frames[f->frame];
    if (!f->facing) *x=(int)f->x-fr->centerx+rx;
    else *x=(int)f->x+fr->centerx-rw-rx;
    *y=(int)f->y-fr->centery+ry;
}

static int are_enemies(const fighter_t *a,const fighter_t *b){
    if(!a||!b||a==b)return 0;
    /* Independent fighters are hostile to everybody. Team members only spare
       fighters that share the same numbered team. */
    return a->team==0 || b->team==0 || a->team!=b->team;
}

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
