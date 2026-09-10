    /* The original LF2 selection screen is a very dark royal-blue panel with
       eight player cards. Keep the supplied LF2 artwork faintly visible behind it. */
    draw_full_texture(g_menu_bg);vita2d_draw_rectangle(0,0,960,544,RGBA8(7,29,87,224));
    if(g_font){
        vita2d_pgf_draw_text(g_font,318,44,RGBA8(255,255,255,255),1.16f,"Character Selection");
        if(cheat)vita2d_pgf_draw_text(g_font,760,43,RGBA8(255,225,90,255),0.60f,"LF2.NET");
    }
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
        if(g_font){
            vita2d_draw_rectangle(160,510,640,28,RGBA8(0,5,28,225));
            vita2d_pgf_draw_text(g_font,190,530,RGBA8(225,236,255,255),0.62f,"←/→ Fighter   ↑/↓ Team   □/× OK   ○ Zurück");
        }
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
            vita2d_pgf_draw_text(g_font,36,150,RGBA8(215,230,255,255),0.62f,"←/→ Anzahl    □/× OK");
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
            if(sel==2){int vis[32],n=build_visible_indices(cheat,vis,32);setup->p1=vis[rand()%n];setup->p1_team=0;for(int i=0;i<setup->cpu_count;i++){setup->cpus[i]=vis[rand()%n];setup->cpu_teams[i]=0;}ui_join();}
            if(sel==3){setup->stage++;if(setup->stage>=STAGE_COUNT)setup->stage=-1;ui_join();}
            if(sel==4){int max=cheat?LF2_DIFF_CRAZY:LF2_DIFF_DIFFICULT;setup->difficulty=(setup->difficulty+1)%(max+1);ui_join();}
            if(sel==5){ui_cancel();return 0;}
        }
        draw_character_selection_base(setup,-1,-1,0,cheat);
        vita2d_draw_rectangle(17,15,330,205,RGBA8(36,79,159,247));
        if(g_font){
            for(int i=0;i<6;i++){
                float y=43.0f+i*27.0f;if(i==sel)vita2d_draw_rectangle(28,y-18,306,23,RGBA8(114,153,225,245));
                unsigned c=i==sel?RGBA8(255,255,255,255):RGBA8(216,228,252,255);
                if(i==3)vita2d_pgf_draw_textf(g_font,38,(int)y,c,0.64f,"Background: %s",stage_option_name(setup->stage));
                else if(i==4)vita2d_pgf_draw_textf(g_font,38,(int)y,c,0.64f,"Difficulty: %s",ui_diff_name(setup->difficulty));
                else vita2d_pgf_draw_text(g_font,38,(int)y,c,0.67f,labels[i]);
            }
            vita2d_pgf_draw_text(g_font,365,530,RGBA8(220,232,250,255),0.60f,"↑/↓ Menü   ←/→ Wert   □/× OK   ○ Zurück");
        }
        vita2d_end_drawing();vita2d_swap_buffers();
    }
}

static int run_vs(bool cheat){
    vs_setup_t setup;setup_defaults(&setup);srand((unsigned)sceKernelGetSystemTimeWide());
    for(;;){
        lf2_log_stage("character_select:open","style=classic cheat=%d",cheat?1:0);
        if(!select_slot(cheat,&setup,0,&setup.p1,&setup.p1_team))return 0;
        if(!select_cpu_count_original(&setup,cheat))continue;
        int canceled=0;
