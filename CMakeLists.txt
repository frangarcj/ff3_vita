cmake_minimum_required(VERSION 3.5)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(FF3.elf C CXX)
include("${VITASDK}/share/vita.cmake" REQUIRED)
set(VITA_APP_NAME "Final Fantasy III")
set(VITA_TITLEID  "FF3000000")
set(VITA_VERSION  "01.22")
set(VITA_MKSFOEX_FLAGS "-d ATTRIBUTE2=12")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wl,-q,--wrap,memcpy,--wrap,memmove,--wrap,memset -Wall -O3 -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++17")

set(ICU_DATA_DIR "")
if(DEFINED ENV{VITASDK})
  file(GLOB ICU_DATA_CANDIDATES "$ENV{VITASDK}/arm-vita-eabi/share/icu/*/icudt*l")
  list(SORT ICU_DATA_CANDIDATES ORDER DESCENDING)
  foreach(candidate ${ICU_DATA_CANDIDATES})
    if(IS_DIRECTORY "${candidate}" AND EXISTS "${candidate}/cnvalias.icu")
      set(ICU_DATA_DIR "${candidate}")
      break()
    endif()
  endforeach()
endif()

if(NOT ICU_DATA_DIR AND EXISTS "${CMAKE_SOURCE_DIR}/icudt73l")
  set(ICU_DATA_DIR "${CMAKE_SOURCE_DIR}/icudt73l")
endif()

if(NOT ICU_DATA_DIR)
  message(FATAL_ERROR "Could not locate an ICU data directory with converter files")
endif()

get_filename_component(ICU_DATA_BASENAME "${ICU_DATA_DIR}" NAME)

set(VPK_FILE_ARGS
  ${CMAKE_SOURCE_DIR}/sce_sys/icon0.png sce_sys/icon0.png
  ${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/bg0.png sce_sys/livearea/contents/bg0.png
  ${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
  ${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/config.png sce_sys/livearea/contents/config.png
  ${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
  ${CMAKE_SOURCE_DIR}/NotoSansJP-Regular.ttf NotoSansJP-Regular.ttf
  ${CMAKE_SOURCE_DIR}/NotoSansSC-Regular.ttf NotoSansSC-Regular.ttf
  ${CMAKE_SOURCE_DIR}/NotoSansKR-Regular.ttf NotoSansKR-Regular.ttf
  ${CMAKE_BINARY_DIR}/companion.bin companion.bin
)

set(ICU_WINDOWS_1252_SOURCE "")
if(EXISTS "${ICU_DATA_DIR}/windows-1252.cnv")
  set(ICU_WINDOWS_1252_SOURCE "${ICU_DATA_DIR}/windows-1252.cnv")
elseif(EXISTS "${ICU_DATA_DIR}/ibm-1252_P100-2000.cnv")
  set(ICU_WINDOWS_1252_SOURCE "${ICU_DATA_DIR}/ibm-1252_P100-2000.cnv")
endif()

if(ICU_WINDOWS_1252_SOURCE)
  list(APPEND VPK_FILE_ARGS
    ${ICU_WINDOWS_1252_SOURCE} ${ICU_DATA_BASENAME}/windows-1252.cnv
  )
endif()

foreach(icu_file
    cnvalias.icu
    ibm-943_P15A-2003.cnv
    ibm-5348_P100-1997.cnv
    confusables.cfu
    unames.icu)
  if(EXISTS "${ICU_DATA_DIR}/${icu_file}")
    list(APPEND VPK_FILE_ARGS
      ${ICU_DATA_DIR}/${icu_file} ${ICU_DATA_BASENAME}/${icu_file}
    )
  endif()
endforeach()

list(APPEND VPK_FILE_ARGS
  ${CMAKE_SOURCE_DIR}/shaders/1_Negative_f.cg shaders/1_Negative_f.cg
  ${CMAKE_SOURCE_DIR}/shaders/1_Negative_v.cg shaders/1_Negative_v.cg
  ${CMAKE_SOURCE_DIR}/shaders/2_FXAA_f.cg shaders/2_FXAA_f.cg
  ${CMAKE_SOURCE_DIR}/shaders/2_FXAA_v.cg shaders/2_FXAA_v.cg
  ${CMAKE_SOURCE_DIR}/shaders/3_Sepia_f.cg shaders/3_Sepia_f.cg
  ${CMAKE_SOURCE_DIR}/shaders/3_Sepia_v.cg shaders/3_Sepia_v.cg
  ${CMAKE_SOURCE_DIR}/shaders/4_Greyscale_f.cg shaders/4_Greyscale_f.cg
  ${CMAKE_SOURCE_DIR}/shaders/4_Greyscale_v.cg shaders/4_Greyscale_v.cg
)

add_executable(FF3.elf
  loader/main.cpp
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
  SceShaccCgExt
  SceSysmodule_stub
  ScePower_stub
  SceTouch_stub
  SceVshBridge_stub
  SceAvPlayer_stub
  taihen_stub
  OpenSLES
  sndfile
  mpg123
  mp3lame
  opus
  opusfile
  FLAC
  vorbisenc
  vorbisfile
  vorbis
  ogg
  SDL
  SceAudio_stub
  SceHid_stub
  SceMotion_stub
  icuuc
  icudata
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
  SceAppUtil_stub
  SceAudio_stub
  SceCtrl_stub
  SceCommonDialog_stub
  SceDisplay_stub
  SceKernelDmacMgr_stub
  SceGxm_stub
  SceShaccCg_stub
  SceShaccCgExt
  SceSysmodule_stub
  SceTouch_stub
  SceVshBridge_stub
  taihen_stub
)

vita_create_self(companion.bin companion UNSAFE)

vita_create_vpk(FF3.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE ${VPK_FILE_ARGS}
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