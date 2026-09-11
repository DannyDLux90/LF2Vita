static void net70_draw_platform_icon(float x,float y,int platform,unsigned accent){
    if(platform==NET70_PLATFORM_VITA){
        vita2d_draw_rectangle(x+18,y+18,176,86,RGBA8(17,22,31,255));net70_draw_outline(x+18,y+18,176,86,accent);
        vita2d_draw_rectangle(x+43,y+35,126,51,RGBA8(43,55,73,255));
        vita2d_draw_rectangle(x,y+50,14,24,accent);vita2d_draw_rectangle(x+198,y+50,14,24,accent);
        vita2d_draw_rectangle(x+31,y+51,9,9,RGBA8(195,219,248,255));vita2d_draw_rectangle(x+174,y+51,9,9,RGBA8(195,219,248,255));
    }else{
        vita2d_draw_rectangle(x+31,y+10,152,94,RGBA8(17,22,31,255));net70_draw_outline(x+31,y+10,152,94,accent);
        vita2d_draw_rectangle(x+46,y+26,122,62,RGBA8(43,53,69,255));
        vita2d_draw_rectangle(x+101,y+105,14,24,accent);vita2d_draw_rectangle(x+65,y+128,86,8,accent);
    }
}

static void net70_draw_platform_card(float x,float y,float w,float h,int platform,int selected){
    unsigned accent=platform==NET70_PLATFORM_VITA?RGBA8(68,145,242,255):RGBA8(157,101,230,255);
    unsigned bg=selected?RGBA8(31,44,63,255):RGBA8(23,30,42,255);
    vita2d_draw_rectangle(x,y,w,h,bg);vita2d_draw_rectangle(x,y,w,8,accent);if(selected)net70_draw_outline(x,y,w,h,accent);
    net70_draw_platform_icon(x+104,y+31,platform,accent);
    const char *title=platform==NET70_PLATFORM_VITA?"PS Vita":"PC";
    const char *sub=platform==NET70_PLATFORM_VITA?"Mit anderen Vita-Spielern":"Mit PC-Spielern";
    net70_text_center(x,w,y+210,1.22f,RGBA8(249,251,254,255),title);
    net70_text_center(x,w,y+246,0.78f,RGBA8(178,195,217,255),sub);
    net70_text(x+42,y+286,0.68f,RGBA8(218,229,243,255),platform==NET70_PLATFORM_VITA?"Internet + lokales AdHoc":"Original LF2 2.00a / TCP 12345");
    net70_text(x+42,y+318,0.68f,RGBA8(169,187,209,255),platform==NET70_PLATFORM_VITA?"Touch und Controller":"Unveraenderte Windows-Version");
    vita2d_draw_rectangle(x+42,y+h-62,w-84,48,selected?accent:RGBA8(40,49,64,255));
    net70_text_center(x+42,w-84,y+h-30,0.76f,RGBA8(249,251,254,255),selected?"X oder antippen":"Auswaehlen");
}

static int network_play_menu(bool cheat){
    int sel=0,touch_was_down=0;uint32_t prev=sync_buttons();
    lf2_log_stage("network:menu","version=0.70 selector=platform vita=1 pc=1 internet_default=1");
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,SCE_TOUCH_SAMPLING_STATE_START);
    for(;;){
        SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;
        net70_touch_press_t tp;net70_touch_poll(&tp,&touch_was_down);
        if(pr&(SCE_CTRL_LEFT|SCE_CTRL_RIGHT)){sel=1-sel;ui_join();}
        if(pr&SCE_CTRL_CIRCLE){ui_cancel();sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,SCE_TOUCH_SAMPLING_STATE_STOP);return 0;}
        if(pr&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE)){
            int platform=sel==0?NET70_PLATFORM_VITA:NET70_PLATFORM_PC;ui_ok();lf2_log_stage("network:platform","target=%s",net_platform_name(platform));
            sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,SCE_TOUCH_SAMPLING_STATE_STOP);server_browser(cheat,platform);sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,SCE_TOUCH_SAMPLING_STATE_START);prev=sync_buttons();touch_was_down=0;
        }
        if(tp.pressed){
            int platform=-1;
            if(net70_point_in(tp.x,tp.y,36,124,432,340)){sel=0;platform=NET70_PLATFORM_VITA;}
            else if(net70_point_in(tp.x,tp.y,492,124,432,340)){sel=1;platform=NET70_PLATFORM_PC;}
            else if(net70_point_in(tp.x,tp.y,378,478,204,56)){ui_cancel();sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,SCE_TOUCH_SAMPLING_STATE_STOP);return 0;}
            if(platform>=0){ui_ok();lf2_log_stage("network:platform","target=%s",net_platform_name(platform));sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,SCE_TOUCH_SAMPLING_STATE_STOP);server_browser(cheat,platform);sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,SCE_TOUCH_SAMPLING_STATE_START);prev=sync_buttons();touch_was_down=0;}
        }

        vita2d_start_drawing();vita2d_clear_screen();vita2d_draw_rectangle(0,0,960,544,RGBA8(11,15,22,255));
        vita2d_draw_rectangle(0,0,960,104,RGBA8(19,25,36,255));
        vita2d_draw_rectangle(36,103,888,2,RGBA8(52,68,91,255));
        net70_text(38,53,1.28f,RGBA8(249,251,254,255),"Netzwerkspiel");
        net70_text(40,88,0.76f,RGBA8(166,184,207,255),"Waehle zuerst, mit welchem Geraet du spielen moechtest.");
        net70_draw_platform_card(36,124,432,340,NET70_PLATFORM_VITA,sel==0);
        net70_draw_platform_card(492,124,432,340,NET70_PLATFORM_PC,sel==1);
        net70_modern_button(378,478,204,56,"Zurueck",0,1);
        net70_text(38,530,0.62f,RGBA8(155,174,198,255),"Touch oder Links/Rechts + X");
        net70_text(660,530,0.62f,RGBA8(155,174,198,255),"Start: Internet");
        vita2d_end_drawing();vita2d_swap_buffers();
    }
}
