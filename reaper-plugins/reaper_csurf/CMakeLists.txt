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

if(WIN32)
  target_sources(reaper_csurf PRIVATE res.rc)
  set_target_properties(reaper_csurf PROPERTIES SUFFIX ".dll")
elseif(APPLE)
  set_target_properties(reaper_csurf PROPERTIES SUFFIX ".dylib")
else()
  set_target_properties(reaper_csurf PROPERTIES SUFFIX ".so")
endif()

# Include paths for SDK and WDL
set(REAPER_SDK_ROOT ${PROJECT_SOURCE_DIR})

# reaper-plugins directory for shared headers like reaper_plugin.h
set(REAPER_PLUGINS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_include_directories(reaper_csurf PRIVATE
  ${REAPER_PLUGINS_ROOT}
  ${REAPER_SDK_ROOT}/sdk
  ${REAPER_SDK_ROOT}/WDL/WDL
  ${REAPER_SDK_ROOT}/WDL/swell
)

target_compile_features(reaper_csurf PRIVATE cxx_std_17)
target_compile_definitions(reaper_csurf PRIVATE NOMINMAX)
