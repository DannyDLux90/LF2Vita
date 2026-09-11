
typedef struct { const char *name; const char *asset; } stage_entry_t;
static const stage_entry_t g_stages[] = {
 {"Stanley Prison",    "app0:/assets/stages/00_sp.png"},
 {"Brokeback Cliff",  "app0:/assets/stages/01_bc.png"},
 {"CUHK",              "app0:/assets/stages/02_cuhk.png"},
 {"HK Coliseum",       "app0:/assets/stages/03_hkc.png"},
 {"Tai Hom Village",   "app0:/assets/stages/04_thv.png"},
 {"Queen's Island",    "app0:/assets/stages/05_qi.png"},
 {"The Great Wall",    "app0:/assets/stages/06_gw.png"},
 {"Lion Forest",       "app0:/assets/stages/07_lf.png"},
 {"Forbidden Tower",   "app0:/assets/stages/08_ft.png"}
};
#define STAGE_COUNT ((int)(sizeof(g_stages)/sizeof(g_stages[0])))

static uint32_t sync_buttons(void){SceCtrlData p={0};sceCtrlPeekBufferPositive(0,&p,1);return p.buttons;}

static void draw_full_texture(vita2d_texture *t){if(t)vita2d_draw_texture_scale(t,0,0,960.0f/(float)vita2d_texture_get_width(t),544.0f/(float)vita2d_texture_get_height(t));}

static const char *check_progress_kind(const char *path){
    if(!path||!path[0])return "Spieldaten";
    if(!strncmp(path,"bg/",3))return "Hintergrund";
    if(!strncmp(path,"data/",5)){
        size_t n=strlen(path);if(n>=4&&!strcmp(path+n-4,".wav"))return "Sound";
        if(n>=4&&!strcmp(path+n-4,".dat"))return "Charakter-/Objektdaten";
        return "Spieldaten";
    }
    if(!strncmp(path,"recording/",10))return "Aufzeichnung";
    if(!strncmp(path,"sprite/",7))return "Grafik";
    return "Spieldatei";
}

static void check_progress(int done,int total,const char *path,int ok,void *userdata){
    (void)userdata;float p=total?((float)done/(float)total):0.0f;if(p<0)p=0;if(p>1)p=1;
    vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_check_bg);
    /* 0.70: prominent, readable progress without the old translucent dialog.
       Show both resource class and packed path so the integrity/load pass never
       looks frozen on slower memory cards. */
    const float bx=105,by=430,bw=750,bh=24;
    vita2d_draw_rectangle(bx-2,by-2,bw+4,bh+4,RGBA8(225,235,250,235));
    vita2d_draw_rectangle(bx,by,bw,bh,RGBA8(13,29,55,245));
    vita2d_draw_rectangle(bx,by,bw*p,bh,ok?RGBA8(72,176,238,255):RGBA8(225,75,92,255));
    if(g_font){
        char short_path[96];snprintf(short_path,sizeof(short_path),"%s",path&&path[0]?path:"Initialisiere Spieldaten...");
        vita2d_pgf_draw_text(g_font,105,375,RGBA8(250,252,255,255),0.86f,"Little Fighter 2 wird geladen");
        vita2d_pgf_draw_textf(g_font,105,405,RGBA8(225,238,252,255),0.66f,"Lade / pruefe: %s",check_progress_kind(path));
        vita2d_pgf_draw_textf(g_font,105,425,RGBA8(205,222,244,245),0.54f,"%s",short_path);
        vita2d_pgf_draw_textf(g_font,760,477,RGBA8(245,249,255,255),0.60f,"%d/%d  %d%%",done,total,(int)(p*100.0f+0.5f));
    }
    vita2d_end_drawing();vita2d_swap_buffers();
}

static int wait_message(const char *title,const char *message){
    uint32_t prev=0;for(;;){SceCtrlData pad={0};sceCtrlPeekBufferPositive(0,&pad,1);uint32_t pressed=pad.buttons&~prev;prev=pad.buttons;
        vita2d_start_drawing();vita2d_clear_screen();draw_full_texture(g_menu_bg);vita2d_draw_rectangle(95,135,770,280,RGBA8(0,7,28,230));
        if(g_font){vita2d_pgf_draw_text(g_font,140,190,RGBA8(255,255,255,255),1.15f,title);vita2d_pgf_draw_text(g_font,140,250,RGBA8(205,220,245,255),0.76f,message);vita2d_pgf_draw_textf(g_font,140,320,RGBA8(150,195,245,255),0.64f,"Diagnose-Log: %s",lf2_log_path());vita2d_pgf_draw_text(g_font,140,375,RGBA8(130,190,255,255),0.72f,"KREUZ / KREIS: zurück");}
        vita2d_end_drawing();vita2d_swap_buffers();if(pressed&(SCE_CTRL_CROSS|SCE_CTRL_CIRCLE))return 0;}
}

static launch_mode_t detect_launch_mode(void){
    launch_mode_t mode=LAUNCH_NORMAL;SceAppUtilInitParam init={0};SceAppUtilBootParam boot={0};
    int rc=sceAppUtilInit(&init,&boot);lf2_logf("INFO","sceAppUtilInit rc=0x%08X",rc);
    if(rc>=0){SceAppUtilAppEventParam ev;memset(&ev,0,sizeof(ev));rc=sceAppUtilReceiveAppEvent(&ev);lf2_logf("INFO","ReceiveAppEvent rc=0x%08X type=0x%X",rc,ev.type);
