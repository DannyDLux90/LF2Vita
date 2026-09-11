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
    fighter_t *p=&fighters[0];int total=cpu_count+1;if(total<1)total=1;if(total>LF2_MAX_ACTORS)total=LF2_MAX_ACTORS;
    vita2d_start_drawing();vita2d_clear_screen();
    draw_world_background(bg);
    /* Original LF2 reserves a two-row blue status board at the top. */
    vita2d_draw_rectangle(0,0,960,128,RGBA8(37,71,148,255));
    for(int i=0;i<8;i++)draw_status_slot(i<total?&fighters[i]:NULL,i);
    /* Stage corpses are snapshots backed by the shared Stage texture cache.
       Draw them under active fighters/objects so dead enemies persist visually
       even when their logical CPU slot is reused by the next wave. */
    draw_stage_corpses(world);
    int order[LF2_MAX_ACTORS];for(int i=0;i<total;i++)order[i]=i;for(int i=0;i<total;i++)for(int j=i+1;j<total;j++)if(fighters[order[i]].z>fighters[order[j]].z){int t=order[i];order[i]=order[j];order[j]=t;}
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
