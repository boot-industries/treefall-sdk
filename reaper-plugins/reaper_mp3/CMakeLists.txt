set(MP3_SOURCES
  mp3dec.cpp
  mp3_index.cpp
  pcmsrc_mp3dec.cpp
  pcmsink_mp3lame.cpp
  mpglib/StdAfx.cpp
  mpglib/common.cpp
  mpglib/dct64_i386.cpp
  mpglib/decode_i386.cpp
  mpglib/interface.cpp
  mpglib/layer2.cpp
  mpglib/layer3.cpp
  mpglib/tabinit.cpp
  ${PROJECT_SOURCE_DIR}/WDL/WDL/lameencdec.cpp
)

add_library(reaper_mp3 MODULE ${MP3_SOURCES})

if(WIN32)
  target_sources(reaper_mp3 PRIVATE res.rc)
  set_target_properties(reaper_mp3 PROPERTIES SUFFIX ".dll")
elseif(APPLE)
  set_target_properties(reaper_mp3 PROPERTIES SUFFIX ".dylib")
else()
  set_target_properties(reaper_mp3 PROPERTIES SUFFIX ".so")
endif()

set(REAPER_SDK_ROOT ${PROJECT_SOURCE_DIR})
set(REAPER_PLUGINS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_include_directories(reaper_mp3 PRIVATE
  ${REAPER_PLUGINS_ROOT}
  ${REAPER_SDK_ROOT}/sdk
  ${REAPER_SDK_ROOT}/WDL/WDL
  ${REAPER_SDK_ROOT}/WDL/swell
)

target_compile_features(reaper_mp3 PRIVATE cxx_std_17)
target_compile_definitions(reaper_mp3 PRIVATE NOMINMAX)
