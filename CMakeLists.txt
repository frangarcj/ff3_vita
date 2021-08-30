cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(FF3.elf C CXX)
include("${VITASDK}/share/vita.cmake" REQUIRED)
set(VITA_APP_NAME "Final Fantasy 3")
set(VITA_TITLEID  "FF3000000")
set(VITA_VERSION  "01.00")
set(VITA_MKSFOEX_FLAGS "-d ATTRIBUTE2=12")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wl,-q,--wrap,memcpy,--wrap,memmove,--wrap,memset -Wall -O3 -mfloat-abi=softfp")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11")

add_executable(FF3.elf
  loader/main.c
  loader/dialog.c
  loader/so_util.c
  loader/bridge.c
  loader/stb_image.c
  loader/stb_truetype.c
)

target_link_libraries(FF3.elf
  m
  stdc++
  vitaGL
  vitashark
  mathneon
  kubridge_stub
  SceAppMgr_stub
  SceAppUtil_stub
  SceAudio_stub
  SceCtrl_stub
  SceCommonDialog_stub
  SceDisplay_stub
  SceKernelDmacMgr_stub
  SceGxm_stub
  SceShaccCg_stub
  SceSysmodule_stub
  ScePower_stub
  SceTouch_stub
  SceVshBridge_stub
  SceAvPlayer_stub
  OpenSLES
  sndfile
  SDL
  SceHid_stub
  icudata
  icuuc
  z
  -Wl,--whole-archive
  pthread
  -Wl,--no-whole-archive
)

vita_create_self(eboot.bin FF3.elf UNSAFE)

add_executable(companion
  companion/main.cpp
)

target_link_libraries(companion
  imgui
  vitaGL
  vitashark
  mathneon
  SceAppMgr_stub
  SceAudio_stub
  SceCtrl_stub
  SceCommonDialog_stub
  SceDisplay_stub
  SceKernelDmacMgr_stub
  SceGxm_stub
  SceShaccCg_stub
  SceSysmodule_stub
  SceTouch_stub
  SceVshBridge_stub
)

vita_create_self(companion.bin companion UNSAFE)

vita_create_vpk(FF3.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE ${CMAKE_SOURCE_DIR}/sce_sys/icon0.png sce_sys/icon0.png
       ${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/bg0.png sce_sys/livearea/contents/bg0.png
       ${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
       ${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/config.png sce_sys/livearea/contents/config.png
       ${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
       ${CMAKE_SOURCE_DIR}/NotoSansJP-Regular.ttf NotoSansJP-Regular.ttf
       ${CMAKE_BINARY_DIR}/companion.bin companion.bin
       ${CMAKE_SOURCE_DIR}/icudt59l/cnvalias.icu icudt59l/cnvalias.icu
       ${CMAKE_SOURCE_DIR}/icudt59l/confusables.cfu icudt59l/confusables.cfu
       ${CMAKE_SOURCE_DIR}/icudt59l/unames.icu icudt59l/unames.icu
       ${CMAKE_SOURCE_DIR}/icudt59l/ibm-943_P15A-2003.cnv icudt59l/ibm-943_P15A-2003.cnv
       ${CMAKE_SOURCE_DIR}/icudt59l/ibm-5348_P100-1997.cnv icudt59l/ibm-5348_P100-1997.cnv
       ${CMAKE_SOURCE_DIR}/shaders/1_Negative_f.cg shaders/1_Negative_f.cg
       ${CMAKE_SOURCE_DIR}/shaders/1_Negative_v.cg shaders/1_Negative_v.cg
       ${CMAKE_SOURCE_DIR}/shaders/2_FXAA_f.cg shaders/2_FXAA_f.cg
       ${CMAKE_SOURCE_DIR}/shaders/2_FXAA_v.cg shaders/2_FXAA_v.cg
       ${CMAKE_SOURCE_DIR}/shaders/3_Sepia_f.cg shaders/3_Sepia_f.cg
       ${CMAKE_SOURCE_DIR}/shaders/3_Sepia_v.cg shaders/3_Sepia_v.cg
       ${CMAKE_SOURCE_DIR}/shaders/4_Greyscale_f.cg shaders/4_Greyscale_f.cg
       ${CMAKE_SOURCE_DIR}/shaders/4_Greyscale_v.cg shaders/4_Greyscale_v.cg
)

add_custom_target(copy
  COMMAND echo "destroy" | nc $(PSVITAIP) 1338
  COMMAND curl -T eboot.bin ftp://$(PSVITAIP):1337/ux0:/app/${VITA_TITLEID}/eboot.bin  DEPENDS eboot.bin
  COMMAND curl -T companion.bin ftp://$(PSVITAIP):1337/ux0:/app/${VITA_TITLEID}/companion.bin  DEPENDS companion.bin
)

add_custom_target(launch
  COMMAND echo "screen on" | nc $(PSVITAIP) 1338
  COMMAND echo "destroy" | nc $(PSVITAIP) 1338
  COMMAND curl -T eboot.bin ftp://$(PSVITAIP):1337/ux0:/app/${VITA_TITLEID}/eboot.bin   DEPENDS eboot.bin	
  COMMAND curl -T companion.bin ftp://$(PSVITAIP):1337/ux0:/app/${VITA_TITLEID}/companion.bin  DEPENDS companion.bin
  COMMAND echo "launch ${VITA_TITLEID}" | nc $(PSVITAIP) 1338
)