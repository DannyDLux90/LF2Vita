/* Modern big-type/touch server browser and platform selector restored from
   the local 0.70 workbench. */
static int server_browser(bool cheat,int platform){
    if(platform==NET70_PLATFORM_PC)return pc_internet_browser(cheat);
    int transport=NET70_TRANSPORT_INTERNET; /* always start on Internet */
    int backend_rc=net70_browser_start_transport(platform,transport);
    int sel=0,scroll=0,expanded=-1;uint32_t prev=sync_buttons();uint64_t last_probe=0;
    net_browser_filter_t filter={5,4,0,0,0,0};int touch_was_down=0;
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,SCE_TOUCH_SAMPLING_STATE_START);
    lf2_log_stage("network:browser","platform=%s transport=internet default=1",net_platform_name(platform));
    for(;;){
        int supported=(platform==NET70_PLATFORM_VITA&&transport==NET70_TRANSPORT_ADHOC);
        if(supported&&backend_rc>=0)lf2_adhoc_browser_update();
        int visible[LF2_ADHOC_MAX_SESSIONS];int vn=(supported&&backend_rc>=0)?browser_visible_sessions(visible,LF2_ADHOC_MAX_SESSIONS,&filter):0;int total=vn+2;
        if(sel>=total)sel=total-1;if(sel<0)sel=0;if(expanded>=vn)expanded=-1;
        if(sel<vn){if(sel<scroll)scroll=sel;if(sel>=scroll+2)scroll=sel-1;}else if(scroll>vn-1)scroll=vn>0?vn-1:0;
        uint64_t nu=sceKernelGetSystemTimeWide();
        if(supported&&backend_rc>=0&&vn>0&&nu-last_probe>650000ULL){for(int v=0;v<vn;v++){int i=visible[v];const lf2_adhoc_session_t *ss=lf2_adhoc_browser_session(i);if(ss&&ss->ping_ms==0xffff){if(lf2_adhoc_browser_probe(i)>=0){last_probe=nu;break;}}}}

        int card_v[3],card_n=0;float card_y[3],card_h[3];float ly=208;
        for(int v=scroll;v<vn&&card_n<2;v++){float h=(expanded==v)?196.0f:104.0f;if(ly+h>466.0f)break;card_v[card_n]=v;card_y[card_n]=ly;card_h[card_n]=h;card_n++;ly+=h+10.0f;}
        if(sel<vn){int found=0;for(int k=0;k<card_n;k++)if(card_v[k]==sel)found=1;if(!found){scroll=sel;card_n=0;ly=208;for(int v=scroll;v<vn&&card_n<2;v++){float h=(expanded==v)?196.0f:104.0f;if(ly+h>466.0f)break;card_v[card_n]=v;card_y[card_n]=ly;card_h[card_n]=h;card_n++;ly+=h+10.0f;}}}

        SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);uint32_t pr=p.buttons&~prev;prev=p.buttons;
        net70_touch_press_t tp;net70_touch_poll(&tp,&touch_was_down);

        if(tp.pressed){
            if(net70_point_in(tp.x,tp.y,552,11,184,56)||net70_point_in(tp.x,tp.y,744,11,184,56)){
                int nt=net70_point_in(tp.x,tp.y,552,11,184,56)?NET70_TRANSPORT_INTERNET:NET70_TRANSPORT_ADHOC;
                if(nt!=transport){transport=nt;backend_rc=net70_browser_start_transport(platform,transport);sel=0;scroll=0;expanded=-1;last_probe=0;filter.editing=0;ui_join();lf2_log_stage("network:transport","platform=%s transport=%s auto_connect=%d",net_platform_name(platform),transport==NET70_TRANSPORT_ADHOC?"adhoc":"internet",transport==NET70_TRANSPORT_ADHOC?1:0);}
                goto browser_touch_done;
            }
            const float chipx[4]={24,252,480,708};
            for(int i=0;i<4;i++)if(net70_point_in(tp.x,tp.y,chipx[i],146,216,54)){filter.selected=i;browser_filter_change(&filter,1);sel=0;scroll=0;expanded=-1;ui_join();goto browser_touch_done;}
            for(int k=0;k<card_n;k++)if(net70_point_in(tp.x,tp.y,24,card_y[k],912,card_h[k])){
                int v=card_v[k];sel=v;
                if(expanded==v&&net70_point_in(tp.x,tp.y,710,card_y[k]+119,196,58)){
                    int si=visible[v];const lf2_adhoc_session_t *ss=lf2_adhoc_browser_session(si);int can=ss&&ss->players<ss->max_players&&ss->status==LF2_NET_STATUS_WAITING;
                    if(can&&net70_browser_try_join(si,cheat)){backend_rc=net70_browser_start_transport(platform,transport);sel=0;scroll=0;expanded=-1;last_probe=0;prev=sync_buttons();}
                }else{expanded=(expanded==v)?-1:v;ui_join();}
                goto browser_touch_done;
            }
            if(net70_point_in(tp.x,tp.y,24,476,356,58)&&supported&&backend_rc>=0){sel=vn;ui_ok();lf2_adhoc_browser_stop();adhoc_create_game(cheat);backend_rc=net70_browser_start_transport(platform,transport);sel=0;scroll=0;expanded=-1;last_probe=0;prev=sync_buttons();goto browser_touch_done;}
            if(net70_point_in(tp.x,tp.y,736,476,200,58)){ui_cancel();lf2_adhoc_browser_stop();sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,SCE_TOUCH_SAMPLING_STATE_STOP);return 0;}
        }
        browser_touch_done:;

        if(!filter.editing&&(pr&(SCE_CTRL_LTRIGGER|SCE_CTRL_RTRIGGER))){
            int nt=(pr&SCE_CTRL_LTRIGGER)?NET70_TRANSPORT_INTERNET:NET70_TRANSPORT_ADHOC;
            if(nt!=transport){transport=nt;backend_rc=net70_browser_start_transport(platform,transport);sel=0;scroll=0;expanded=-1;last_probe=0;ui_join();lf2_log_stage("network:transport","platform=%s transport=%s auto_connect=%d",net_platform_name(platform),transport==NET70_TRANSPORT_ADHOC?"adhoc":"internet",transport==NET70_TRANSPORT_ADHOC?1:0);}
        }
        if(pr&SCE_CTRL_SELECT){filter.editing=!filter.editing;ui_join();}
        if(filter.editing){
            if(pr&SCE_CTRL_UP){filter.selected=(filter.selected+3)%4;ui_join();}
            if(pr&SCE_CTRL_DOWN){filter.selected=(filter.selected+1)%4;ui_join();}
            if(pr&(SCE_CTRL_LEFT|SCE_CTRL_RIGHT|SCE_CTRL_CROSS|SCE_CTRL_SQUARE)){browser_filter_change(&filter,(pr&SCE_CTRL_LEFT)?-1:1);sel=0;scroll=0;expanded=-1;ui_join();}
            if(pr&SCE_CTRL_CIRCLE){filter.editing=0;ui_cancel();}
        }else{
