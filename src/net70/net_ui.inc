/* 0.70 network UI: preserve the historical split helpers, but keep the older
   screens private so the restored big-type/touch workbench frontend can own
   the public menu entry points. */
#define adhoc_lobby_screen legacy_adhoc_lobby_screen
#define adhoc_create_game legacy_adhoc_create_game
#define adhoc_browser legacy_adhoc_browser
#define network_play_menu legacy_network_play_menu
#include "net_ui_01.inc"
#include "net_ui_02.inc"
#include "net_ui_03a.inc"
#include "net_ui_03b.inc"
#include "net_ui_03c.inc"
#include "net_ui_04.inc"
#undef network_play_menu
#undef adhoc_browser
#undef adhoc_create_game
#undef adhoc_lobby_screen

#include "net_ui_modern_common_01.inc"
#include "net_ui_modern_common_02.inc"
#include "net_ui_modern_common_03.inc"
#include "net_ui_modern_pc_01.inc"
#include "net_ui_modern_pc_02.inc"
#include "net_ui_modern_browser_01.inc"
#include "net_ui_modern_browser_02.inc"
#include "net_ui_modern_browser_03.inc"
