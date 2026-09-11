    vita2d_draw_rectangle(x,y,w,2,border);vita2d_draw_rectangle(x,y+h-2,w,2,border);vita2d_draw_rectangle(x,y,2,h,border);vita2d_draw_rectangle(x+w-2,y,2,h,border);
    if(enabled&&roster>=0){
        draw_portrait_index(roster,x+58,y+8,0.82f);
    } else {
        vita2d_draw_rectangle(x+57,y+8,100,100,RGBA8(0,0,0,225));
        if(g_font){lf2_ui_draw_text(x+67,y+48,0.68f,RGBA8(255,255,255,255),"Press Square");lf2_ui_draw_text(x+75,y+73,0.68f,RGBA8(255,255,255,255),"to join!");}
    }
    vita2d_draw_rectangle(x+8,y+116,w-16,21,RGBA8(0,5,28,225));
    vita2d_draw_rectangle(x+8,y+139,w-16,21,RGBA8(0,5,28,225));
    vita2d_draw_rectangle(x+8,y+162,w-16,21,RGBA8(0,5,28,225));
    if(g_font){
        const char *role=slot==0?"Player 1":(enabled?"Computer":"Join?");
        const char *fighter=(enabled&&roster>=0)?lf2_roster[roster].name:"-";
        const char *tn=enabled?ui_team_name(team):"-";
        if(lf2_ui_font_ready()){
            lf2_ui_draw_textf(x+12,y+116,0.68f,RGBA8(235,242,255,255),"Player: %s",role);
            lf2_ui_draw_textf(x+12,y+139,0.68f,RGBA8(255,255,255,255),"Fighter: %s",fighter);
            lf2_ui_draw_textf(x+12,y+162,0.68f,RGBA8(255,255,255,255),"Team: %s",tn);
        } else {
            vita2d_pgf_draw_textf(g_font,(int)x+12,(int)y+132,RGBA8(245,247,255,255),0.72f,"Fighter: %s",fighter);
        }
    }
}

static void draw_character_selection_base(const vs_setup_t *setup,int active_slot,int active_roster,int active_team,bool cheat){
    vita2d_start_drawing();vita2d_clear_screen();

    /* The original LF2 selection screen is a very dark royal-blue panel with
       eight player cards. Keep the supplied LF2 artwork faintly visible behind it. */
    draw_full_texture(g_menu_bg);vita2d_draw_rectangle(0,0,960,544,RGBA8(7,29,87,224));
    if(lf2_ui_font_ready()){
        lf2_ui_draw_text(300,16,0.92f,RGBA8(255,255,255,255),"Character Selection");
        if(cheat)lf2_ui_draw_text(775,19,0.68f,RGBA8(255,225,90,255),"LF2.NET");
    } else if(g_font){vita2d_pgf_draw_text(g_font,318,44,RGBA8(255,255,255,255),1.16f,"Character Selection");}
    for(int i=0;i<8;i++)draw_lf2_slot(setup,i,active_slot,active_roster,active_team);
}

static int select_slot(bool cheat,vs_setup_t *setup,int slot,int *roster_out,int *team_out){
    int vis[32],n=build_visible_indices(cheat,vis,32),vpos=0,team=*team_out;
    for(int i=0;i<n;i++)if(vis[i]==*roster_out){vpos=i;break;}
    uint32_t prev=sync_buttons();lf2_log_stage("character_select:slot_open","slot=%d role=%s",slot,slot==0?"Player1":"Computer");
    for(;;){
        SceCtrlData pad={0};sceCtrlPeekBufferPositive(0,&pad,1);uint32_t pr=pad.buttons&~prev;prev=pad.buttons;
        if(pr&SCE_CTRL_LEFT){vpos=(vpos+n-1)%n;ui_join();}
        if(pr&SCE_CTRL_RIGHT){vpos=(vpos+1)%n;ui_join();}
        if(pr&SCE_CTRL_UP){team=(team+4)%5;ui_join();}
        if(pr&SCE_CTRL_DOWN){team=(team+1)%5;ui_join();}
        if(pr&SCE_CTRL_CIRCLE){ui_cancel();return 0;}
        if(pr&(SCE_CTRL_SQUARE|SCE_CTRL_CROSS)){
            *roster_out=vis[vpos];*team_out=team;ui_ok();
            lf2_log_stage("character_select:slot_done","slot=%d fighter=%s team=%s",slot,lf2_roster[*roster_out].name,ui_team_name(team));return 1;
        }
        draw_character_selection_base(setup,slot,vis[vpos],team,cheat);
        vita2d_draw_rectangle(110,505,740,34,RGBA8(0,5,28,235));
        if(lf2_ui_font_ready())lf2_ui_draw_text(130,510,0.68f,RGBA8(255,255,255,255),"Left/Right Fighter   Up/Down Team   X/Square OK   Circle Back");
        vita2d_end_drawing();vita2d_swap_buffers();
    }
}

static int select_cpu_count_original(vs_setup_t *setup,bool cheat){
    int count=setup->cpu_count;uint32_t prev=sync_buttons();lf2_log_stage("character_select:com_count","open=1");
    for(;;){
        SceCtrlData pad={0};sceCtrlPeekBufferPositive(0,&pad,1);uint32_t pr=pad.buttons&~prev;prev=pad.buttons;
        if(pr&(SCE_CTRL_LEFT|SCE_CTRL_DOWN)){count=count>1?count-1:LF2_MAX_CPUS;ui_join();}
        if(pr&(SCE_CTRL_RIGHT|SCE_CTRL_UP)){count=count<LF2_MAX_CPUS?count+1:1;ui_join();}
        if(pr&SCE_CTRL_CIRCLE){ui_cancel();return 0;}
        if(pr&(SCE_CTRL_SQUARE|SCE_CTRL_CROSS)){setup->cpu_count=count;ui_ok();lf2_log_stage("character_select:com_count","count=%d",count);return 1;}
        vs_setup_t preview=*setup;preview.cpu_count=count;
        for(int i=0;i<count;i++)if(preview.cpus[i]<0)preview.cpus[i]=0;
        draw_character_selection_base(&preview,-1,-1,0,cheat);
        vita2d_draw_rectangle(18,17,275,165,RGBA8(25,67,145,242));
        if(g_font){
            vita2d_pgf_draw_text(g_font,36,48,RGBA8(255,255,255,255),0.80f,"COM Players");
            vita2d_pgf_draw_textf(g_font,43,105,RGBA8(255,225,90,255),1.35f,"%d",count);
            vita2d_pgf_draw_text(g_font,90,104,RGBA8(235,242,255,255),0.66f,"Computer");
            vita2d_pgf_draw_text(g_font,36,150,RGBA8(215,230,255,255),0.63f,"←/→ Anzahl    □/× OK");
        }
        vita2d_end_drawing();vita2d_swap_buffers();
    }
}

static const char *stage_option_name(int stage){return stage<0?"Random":g_stages[stage].name;}

static int vs_options(vs_setup_t *setup,bool cheat){
    static const char *labels[]={"Fight!","Reset All","Random","Background","Difficulty","Exit"};
    int sel=0;uint32_t prev=sync_buttons();
    for(;;){
        SceCtrlData pad={0};sceCtrlPeekBufferPositive(0,&pad,1);uint32_t pr=pad.buttons&~prev;prev=pad.buttons;
        if(pr&SCE_CTRL_UP){sel=(sel+5)%6;ui_join();}if(pr&SCE_CTRL_DOWN){sel=(sel+1)%6;ui_join();}
        int adjust=(pr&SCE_CTRL_RIGHT)?1:((pr&SCE_CTRL_LEFT)?-1:0);
        if(adjust&&sel==3){setup->stage+=adjust;if(setup->stage>=STAGE_COUNT)setup->stage=-1;if(setup->stage<-1)setup->stage=STAGE_COUNT-1;ui_join();}
        if(adjust&&sel==4){int max=cheat?LF2_DIFF_CRAZY:LF2_DIFF_DIFFICULT;setup->difficulty+=adjust;if(setup->difficulty>max)setup->difficulty=LF2_DIFF_EASY;if(setup->difficulty<LF2_DIFF_EASY)setup->difficulty=max;ui_join();}
        if(pr&SCE_CTRL_CIRCLE){ui_cancel();return 0;}
        if(pr&(SCE_CTRL_SQUARE|SCE_CTRL_CROSS)){
            if(sel==0){ui_ok();return 1;}
            if(sel==1){ui_cancel();return 2;}
