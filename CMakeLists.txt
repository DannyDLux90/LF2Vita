cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path")
  endif()
endif()

project(lf2vita C)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "Little Fighter 2 Vita")
set(VITA_TITLEID "LF2V00001")
set(VITA_VERSION "00.70")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
# Keep symbols and frame pointers in the developer ELF. The packaged SELF stays compact,
# while future Vita crash dumps can be resolved against the matching debug ELF.
add_compile_options(-O2 -g3 -fno-omit-frame-pointer -fno-optimize-sibling-calls -Wall -Wextra -Wno-unused-parameter -Wno-misleading-indentation -ffunction-sections -fdata-sections)
add_link_options(-Wl,--gc-sections)

# Request the Vita extended-memory application profile.
set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d ATTRIBUTE2=12")

# Keep the menu/network shell compact enough for Vita ELF metadata headroom.
set_source_files_properties(src/main.c src/game.c src/net70/pc_net.c src/replay_ref.c PROPERTIES COMPILE_OPTIONS "-Os")

add_executable(lf2vita
  src/main.c
  src/filecheck.c
  src/asset_pack.c
  src/audio.c
  src/log.c
  src/ui_font.c
  src/game.c
  src/lf2_data.c
  src/stage_data.c
  src/lf2_core.c
  src/lf2_rng.c
  src/replay_ref.c
  src/net70/adhoc_net.c
  src/net70/pc_net.c
)

target_include_directories(lf2vita PRIVATE src)

target_link_libraries(lf2vita
  vita2d png jpeg freetype z m c
  SceDisplay_stub SceGxm_stub SceSysmodule_stub SceCtrl_stub SceTouch_stub
  ScePgf_stub ScePvf_stub SceCommonDialog_stub SceIme_stub SceAppUtil_stub
  SceAppMgr_stub SceIofilemgr_stub SceLibKernel_stub SceAudio_stub
  SceNet_stub SceNetCtl_stub ScePspnetAdhoc_stub SceNetAdhocMatching_stub
)

vita_create_self(eboot.bin lf2vita UNSAFE)

# v0.60: all 690 original 2.00a files are stored inside one indexed/CRC'd
# game.lf2pak. This removes hundreds of VPK entries and dramatically reduces
# VitaShell installation metadata overhead while still keeping the game data
# directly inside the VPK.
set(LF2_OPTIONAL_MUSIC_ARGS)
set(LF2_OPTIONAL_REF_ARGS)
foreach(LF2_REF_NAME
    recording_Demo_1on1.l2rf
    recording_Demo_2on2.l2rf
    recording_Demo_Battle.l2rf
    recording_Demo_Stage_1.l2rf
    recording_Demo_Stage_2.l2rf
    recording_Demo_Stage_3.l2rf
    recording_Demo_Stage_4.l2rf
    recording_Demo_Stage_5.l2rf
    recording_Demo_Survival.l2rf
    recording_Demo_VS.l2rf)
  if(EXISTS "${CMAKE_SOURCE_DIR}/ref/${LF2_REF_NAME}")
    list(APPEND LF2_OPTIONAL_REF_ARGS FILE "${CMAKE_SOURCE_DIR}/ref/${LF2_REF_NAME}" "ref/${LF2_REF_NAME}")
  endif()
endforeach()

if(EXISTS "${CMAKE_SOURCE_DIR}/assets/main_bgm.wav")
  list(APPEND LF2_OPTIONAL_MUSIC_ARGS FILE "${CMAKE_SOURCE_DIR}/assets/main_bgm.wav" "assets/main_bgm.wav")
else()
  message(WARNING "assets/main_bgm.wav is missing; run tools/prepare_music.py with your LF2 2.00a archive to include the original title music")
endif()

vita_create_vpk(lf2vita.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE "${CMAKE_SOURCE_DIR}/sce_sys/icon0.png" "sce_sys/icon0.png"
  FILE "${CMAKE_SOURCE_DIR}/sce_sys/pic0.png" "sce_sys/pic0.png"
  FILE "${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/template.xml" "sce_sys/livearea/contents/template.xml"
  FILE "${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/bg.png" "sce_sys/livearea/contents/bg.png"
  FILE "${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/startup.png" "sce_sys/livearea/contents/startup.png"
  FILE "${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/cheat.png" "sce_sys/livearea/contents/cheat.png"
  FILE "${CMAKE_SOURCE_DIR}/sce_sys/manual/001.png" "sce_sys/manual/001.png"
  FILE "${CMAKE_SOURCE_DIR}/sce_sys/manual/002.png" "sce_sys/manual/002.png"
  FILE "${CMAKE_SOURCE_DIR}/sce_sys/manual/003.png" "sce_sys/manual/003.png"
  FILE "${CMAKE_SOURCE_DIR}/sce_sys/manual/004.png" "sce_sys/manual/004.png"
  FILE "${CMAKE_SOURCE_DIR}/sce_sys/manual/005.png" "sce_sys/manual/005.png"
  FILE "${CMAKE_SOURCE_DIR}/sce_sys/manual/006.png" "sce_sys/manual/006.png"
  FILE "${CMAKE_SOURCE_DIR}/sce_sys/manual/007.png" "sce_sys/manual/007.png"
  FILE "${CMAKE_SOURCE_DIR}/sce_sys/manual/008.png" "sce_sys/manual/008.png"
  FILE "${CMAKE_SOURCE_DIR}/sce_sys/manual/009.png" "sce_sys/manual/009.png"
  FILE "${CMAKE_SOURCE_DIR}/assets/menu_bg.png" "assets/menu_bg.png"
  FILE "${CMAKE_SOURCE_DIR}/assets/check_bg.png" "assets/check_bg.png"
  FILE "${CMAKE_SOURCE_DIR}/assets/manual_bg.png" "assets/manual_bg.png"
  FILE "${CMAKE_SOURCE_DIR}/assets/portraits.png" "assets/portraits.png"
  FILE "${CMAKE_SOURCE_DIR}/assets/net70_vita_button.png" "assets/net70_vita_button.png"
  FILE "${CMAKE_SOURCE_DIR}/assets/net70_pc_button.png" "assets/net70_pc_button.png"
  ${LF2_OPTIONAL_MUSIC_ARGS}
  ${LF2_OPTIONAL_REF_ARGS}
  FILE "${CMAKE_SOURCE_DIR}/assets/stages/00_sp.png" "assets/stages/00_sp.png"
  FILE "${CMAKE_SOURCE_DIR}/assets/stages/01_bc.png" "assets/stages/01_bc.png"
  FILE "${CMAKE_SOURCE_DIR}/assets/stages/02_cuhk.png" "assets/stages/02_cuhk.png"
  FILE "${CMAKE_SOURCE_DIR}/assets/stages/03_hkc.png" "assets/stages/03_hkc.png"
  FILE "${CMAKE_SOURCE_DIR}/assets/stages/04_thv.png" "assets/stages/04_thv.png"
  FILE "${CMAKE_SOURCE_DIR}/assets/stages/05_qi.png" "assets/stages/05_qi.png"
  FILE "${CMAKE_SOURCE_DIR}/assets/stages/06_gw.png" "assets/stages/06_gw.png"
  FILE "${CMAKE_SOURCE_DIR}/assets/stages/07_lf.png" "assets/stages/07_lf.png"
  FILE "${CMAKE_SOURCE_DIR}/assets/stages/08_ft.png" "assets/stages/08_ft.png"
  FILE "${CMAKE_SOURCE_DIR}/gamepack/game.lf2pak" "game.lf2pak"
)
