    static const char *files[]={"Demo_1on1.lfr","Demo_2on2.lfr","Demo_Battle.lfr","Demo_Stage_1.lfr","Demo_Stage_2.lfr","Demo_Stage_3.lfr","Demo_Stage_4.lfr","Demo_Stage_5.lfr","Demo_Survival.lfr","Demo_VS.lfr"};int count=(int)(sizeof(files)/sizeof(files[0])),sel=0;uint32_t prev=sync_buttons();
    for(;;){SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;if(pr&SCE_CTRL_UP){sel=(sel+count-1)%count;ui_join();}if(pr&SCE_CTRL_DOWN){sel=(sel+1)%count;ui_join();}if(pr&SCE_CTRL_CIRCLE){ui_cancel();return 0;}if(pr&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE)){ui_ok();lf2_logf("MODE","playback selected recording/%s",files[sel]);wait_message("Playback Recording","LFR-Datei erkannt. Der deterministische 2.00a-Replay-Decoder folgt in einem separaten kleinen Update.");prev=sync_buttons();}
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);vita2d_draw_rectangle(120,40,720,470,RGBA8(3,24,80,242));if(lf2_ui_font_ready()){lf2_ui_draw_text(150,60,1.0f,RGBA8(255,255,255,255),"Playback Recording");for(int i=0;i<count;i++){float y=112+i*34;if(i==sel)vita2d_draw_rectangle(145,y-5,650,30,RGBA8(92,139,221,235));lf2_ui_draw_text(165,y,0.68f,RGBA8(255,255,255,255),files[i]);}lf2_ui_draw_text(175,472,0.64f,RGBA8(220,235,255,255),"X/Square: Open    Circle: Back");}vita2d_end_drawing();vita2d_swap_buffers();}
}


#include "../net70/net_ui.inc"

static int confirm_quit(void){
    int yes=0;uint32_t prev=sync_buttons();
    for(;;){SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;
        if(pr&(SCE_CTRL_LEFT|SCE_CTRL_RIGHT)){yes=!yes;ui_join();}
        if(pr&SCE_CTRL_CIRCLE){ui_cancel();return 0;}
        if(pr&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE)){ui_ok();return yes;}
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);vita2d_draw_rectangle(240,185,480,165,RGBA8(8,31,92,245));
        if(g_font){vita2d_pgf_draw_text(g_font,338,230,RGBA8(255,255,255,255),1.0f,"Quit Little Fighter 2?");
            if(!yes)vita2d_draw_rectangle(330,270,110,38,RGBA8(110,150,220,240));else vita2d_draw_rectangle(520,270,110,38,RGBA8(110,150,220,240));
