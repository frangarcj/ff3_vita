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

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wl,-q,--wrap,memcpy,--wrap,memmove,--wrap,memset -Wall -O3 -mfloat-abi=softfp -D__psp2__")
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
  kubridge_stub
  SceAppMgr_stub
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
  OpenSLES
  sndfile
  SDL
  SceHid_stub
  z
  liblibGLESv1_CM_stub_weak.a liblibgpu_es4_ext_stub_weak.a liblibIMGEGL_stub_weak.a
  -Wl,--whole-archive
  pthread
  -Wl,--no-whole-archive
)

vita_create_self(eboot.bin FF3.elf UNSAFE)
vita_create_vpk(FF3.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE ${CMAKE_SOURCE_DIR}/sce_sys/icon0.png sce_sys/icon0.png
       ${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
       ${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
       ${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
)

add_custom_target(copy
  COMMAND cp eboot.bin E:/app/${VITA_TITLEID}/eboot.bin
  DEPENDS eboot.bin
)

add_custom_target(launch
  COMMAND echo "screen on" | nc $(PSVITAIP) 1338
  COMMAND echo "destroy" | nc $(PSVITAIP) 1338
  COMMAND curl -T eboot.bin ftp://$(PSVITAIP):1337/ux0:/app/${VITA_TITLEID}/eboot.bin
  COMMAND echo "launch ${VITA_TITLEID}" | nc $(PSVITAIP) 1338
  DEPENDS eboot.bin	
)