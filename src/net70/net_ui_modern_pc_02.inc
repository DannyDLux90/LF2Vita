static int pc_internet_browser(bool cheat){
    (void)cheat;int sel=0,touch_was_down=0;uint32_t prev=sync_buttons();
    lf2_adhoc_shutdown();int netrc=lf2_pc_init();
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,SCE_TOUCH_SAMPLING_STATE_START);
    lf2_log_stage("network:pc_browser","transport=internet protocol=stock-lf2-2.00a tcp=12345 init_rc=0x%08X",(unsigned)netrc);
    for(;;){
        const lf2_pc_stock_state_t *st=lf2_pc_stock_state();int inet=lf2_pc_inet_state();int busy=st->running||st->connected;
        SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;
        net70_touch_press_t tp;net70_touch_poll(&tp,&touch_was_down);
        if(pr&SCE_CTRL_UP){sel=(sel+3)%4;ui_join();}if(pr&SCE_CTRL_DOWN){sel=(sel+1)%4;ui_join();}
        if(pr&SCE_CTRL_TRIANGLE&&busy){lf2_pc_stock_stop();ui_cancel();prev=sync_buttons();}
        if(pr&SCE_CTRL_CIRCLE){ui_cancel();lf2_pc_stock_stop();lf2_pc_shutdown();sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,SCE_TOUCH_SAMPLING_STATE_STOP);return 0;}
        if(pr&(SCE_CTRL_CROSS|SCE_CTRL_SQUARE)){
            if(sel==0&&!busy){int rc=net70_pc_prompt_ipv4(g_pc_target_ip);if(rc>0)ui_ok();else if(rc<0){ui_cancel();wait_message("PC-Adresse","Bitte eine gueltige IPv4-Adresse eingeben.");}prev=sync_buttons();touch_was_down=0;}
            else if(sel==1&&!busy){if(!g_pc_target_ip[0]){ui_cancel();wait_message("LF2 2.00a","Zuerst die IPv4-Adresse des Windows-PCs eingeben.");prev=sync_buttons();}else{int rc=lf2_pc_stock_join_start(g_pc_target_ip);if(rc<0)ui_cancel();else ui_ok();}}
            else if(sel==2&&!busy){int rc=lf2_pc_stock_host_start();if(rc<0)ui_cancel();else ui_ok();}
            else if(sel==3){ui_cancel();lf2_pc_stock_stop();lf2_pc_shutdown();sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,SCE_TOUCH_SAMPLING_STATE_STOP);return 0;}
            else if(busy){lf2_pc_stock_stop();ui_cancel();}
        }
        if(tp.pressed){
            if(net70_point_in(tp.x,tp.y,48,176,864,66)&&!busy){sel=0;int rc=net70_pc_prompt_ipv4(g_pc_target_ip);if(rc>0)ui_ok();else if(rc<0){ui_cancel();wait_message("PC-Adresse","Bitte eine gueltige IPv4-Adresse eingeben.");}prev=sync_buttons();touch_was_down=0;}
            else if(net70_point_in(tp.x,tp.y,48,256,420,60)&&!busy){sel=1;if(g_pc_target_ip[0]){int rc=lf2_pc_stock_join_start(g_pc_target_ip);if(rc<0)ui_cancel();else ui_ok();}else{ui_cancel();wait_message("LF2 2.00a","Zuerst die IPv4-Adresse des Windows-PCs eingeben.");prev=sync_buttons();touch_was_down=0;}}
            else if(net70_point_in(tp.x,tp.y,492,256,420,60)&&!busy){sel=2;int rc=lf2_pc_stock_host_start();if(rc<0)ui_cancel();else ui_ok();}
            else if(net70_point_in(tp.x,tp.y,48,330,864,60)&&busy){lf2_pc_stock_stop();ui_cancel();}
            else if(net70_point_in(tp.x,tp.y,736,476,200,58)){sel=3;ui_cancel();lf2_pc_stock_stop();lf2_pc_shutdown();sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,SCE_TOUCH_SAMPLING_STATE_STOP);return 0;}
        }

        st=lf2_pc_stock_state();busy=st->running||st->connected;inet=lf2_pc_inet_state();
        vita2d_start_drawing();vita2d_clear_screen();vita2d_draw_rectangle(0,0,960,544,RGBA8(12,16,23,255));
        vita2d_draw_rectangle(0,0,960,78,RGBA8(20,27,39,255));net70_text(24,51,1.08f,RGBA8(252,253,255,255),"Serverbrowser");
        vita2d_draw_rectangle(244,18,132,42,RGBA8(95,68,151,255));net70_text_center(244,132,48,0.72f,RGBA8(252,253,255,255),"PC");
        net70_draw_segment(552,11,184,56,"Internet",1,1);net70_draw_segment(744,11,184,56,"AdHoc",0,0);
        unsigned topbg=(netrc>=0&&inet==SCE_NETCTL_STATE_CONNECTED)?RGBA8(25,73,53,255):RGBA8(89,43,49,255);vita2d_draw_rectangle(24,86,912,48,topbg);
        net70_text(38,118,0.70f,RGBA8(252,253,255,255),(netrc<0)?lf2_pc_error():(inet==SCE_NETCTL_STATE_CONNECTED?"WLAN bereit - Original LF2 2.00a / TCP 12345":"Vita ist nicht mit WLAN/Internet verbunden"));
        net70_text(48,162,0.72f,RGBA8(189,205,226,255),"Unveraendertes Little Fighter 2 v2.00a: Connect to opponent / Waiting for opponent");
        vita2d_draw_rectangle(48,176,864,66,sel==0?RGBA8(52,72,105,255):RGBA8(25,33,46,255));if(sel==0)net70_draw_outline(48,176,864,66,RGBA8(145,116,235,255));
        net70_text(70,218,0.72f,RGBA8(189,205,226,255),"PC IPv4");net70_text(265,219,0.92f,RGBA8(255,255,255,255),g_pc_target_ip[0]?g_pc_target_ip:"Antippen und Adresse eingeben");
        net70_modern_button(48,256,420,60,"Mit LF2-PC verbinden",sel==1,!busy&&netrc>=0);
        net70_modern_button(492,256,420,60,"Auf LF2-PC warten",sel==2,!busy&&netrc>=0);
        net70_modern_button(48,330,864,60,busy?"Verbindung stoppen":"Original LF2 Handshake + Lockstep",busy&&(sel==0||sel==1||sel==2),busy);
        net70_pc_stock_status_box(48,404,664,112,st);
        if(st->connected)net70_text(54,536,0.58f,RGBA8(255,213,127,255),"22-Byte Lockstep bereit; Match-State-Paritaet wird weiter vervollstaendigt");
        else if(busy)net70_text(54,536,0.58f,RGBA8(255,213,127,255),"Dreieck: Verbindung abbrechen");
        net70_modern_button(736,476,200,58,"Zurueck",sel==3,1);
        vita2d_end_drawing();vita2d_swap_buffers();
    }
}
