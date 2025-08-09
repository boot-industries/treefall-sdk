add_library(reaper_csurf MODULE
  csurf_main.cpp
  csurf_01X.cpp
  csurf_alphatrack.cpp
  csurf_babyhui.cpp
  csurf_bcf2000.cpp
  csurf_faderport.cpp
  csurf_faderport2.cpp
  csurf_mcu.cpp
  csurf_osc.cpp
  csurf_tranzport.cpp
  csurf_www.cpp
  osc.cpp
  osc_message.cpp
)

set_target_properties(reaper_csurf PROPERTIES PREFIX "")

if(WIN32)
  target_sources(reaper_csurf PRIVATE res.rc)
  set_target_properties(reaper_csurf PROPERTIES SUFFIX ".dll")
  target_compile_definitions(reaper_csurf PRIVATE WIN32 _WINDOWS NOMINMAX)
elseif(APPLE)
  set_target_properties(reaper_csurf PROPERTIES SUFFIX ".dylib")
  target_compile_definitions(reaper_csurf PRIVATE MAC_OS_X)
else()
  set_target_properties(reaper_csurf PROPERTIES SUFFIX ".so")
  target_compile_definitions(reaper_csurf PRIVATE LINUX)
  target_compile_options(reaper_csurf PRIVATE -fPIC)
endif()

target_include_directories(reaper_csurf PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../sdk
  ${CMAKE_CURRENT_SOURCE_DIR}/../../WDL/WDL
  ${CMAKE_CURRENT_SOURCE_DIR}/../../WDL/swell
)

target_compile_features(reaper_csurf PRIVATE cxx_std_17)
target_compile_definitions(reaper_csurf PRIVATE WDL_NO_DEFINE_MINMAX)
