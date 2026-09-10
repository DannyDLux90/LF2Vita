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
set(VITA_VERSION "00.60")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-O2 -g3 -fno-omit-frame-pointer -fno-optimize-sibling-calls -Wall -Wextra -Wno-unused-parameter -Wno-misleading-indentation -ffunction-sections -fdata-sections)
add_link_options(-Wl,--gc-sections)
set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d ATTRIBUTE2=12")

add_executable(lf2vita
  src/main.c
  src/filecheck.c
  src/asset_pack.c
  src/audio.c
  src/log.c
  src/game.c
  src/lf2_data.c
  src/lf2_core.c
)

target_include_directories(lf2vita PRIVATE src)

target_link_libraries(lf2vita
  vita2d png jpeg freetype z m c
  SceDisplay_stub SceGxm_stub SceSysmodule_stub SceCtrl_stub
  ScePgf_stub ScePvf_stub SceCommonDialog_stub SceAppUtil_stub
  SceAppMgr_stub SceIofilemgr_stub SceLibKernel_stub SceAudio_stub
)

vita_create_self(eboot.bin lf2vita UNSAFE)

# Generated UI/manual assets plus the locally generated proprietary game pack
# are part of the local workspace. See WORKSPACE.md / gamepack/README.md.
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
