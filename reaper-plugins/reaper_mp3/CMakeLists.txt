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

if(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86|x86_64|AMD64")
  enable_language(ASM_NASM)
  list(APPEND MP3_SOURCES mpglib/dct64_asm.nas)
  set_source_files_properties(mpglib/dct64_asm.nas PROPERTIES LANGUAGE ASM_NASM)
endif()

add_library(reaper_mp3 MODULE ${MP3_SOURCES})

set_target_properties(reaper_mp3 PROPERTIES PREFIX "")

if(WIN32)
  target_sources(reaper_mp3 PRIVATE res.rc)
  set_target_properties(reaper_mp3 PROPERTIES SUFFIX ".dll")
  target_compile_definitions(reaper_mp3 PRIVATE WIN32 _WINDOWS NOMINMAX)
elseif(APPLE)
  set_target_properties(reaper_mp3 PROPERTIES SUFFIX ".dylib")
  target_compile_definitions(reaper_mp3 PRIVATE MAC_OS_X)
else()
  set_target_properties(reaper_mp3 PROPERTIES SUFFIX ".so")
  target_compile_definitions(reaper_mp3 PRIVATE LINUX)
  target_compile_options(reaper_mp3 PRIVATE -fPIC)
endif()

target_include_directories(reaper_mp3 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../sdk
  ${CMAKE_CURRENT_SOURCE_DIR}/../../WDL/WDL
  ${CMAKE_CURRENT_SOURCE_DIR}/../../WDL/swell
)

target_compile_features(reaper_mp3 PRIVATE cxx_std_17)
target_compile_definitions(reaper_mp3 PRIVATE WDL_NO_DEFINE_MINMAX)
