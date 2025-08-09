add_library(reaper_stt MODULE reaper_stt.cpp)

if(WIN32)
  set_target_properties(reaper_stt PROPERTIES SUFFIX ".dll")
elseif(APPLE)
  set_target_properties(reaper_stt PROPERTIES SUFFIX ".dylib")
else()
  set_target_properties(reaper_stt PROPERTIES SUFFIX ".so")
endif()

set(REAPER_SDK_ROOT ${PROJECT_SOURCE_DIR})
set(REAPER_PLUGINS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_include_directories(reaper_stt PRIVATE
  ${REAPER_PLUGINS_ROOT}
  ${REAPER_SDK_ROOT}/sdk
  ${REAPER_SDK_ROOT}/WDL/WDL
)

target_compile_features(reaper_stt PRIVATE cxx_std_17)
target_compile_definitions(reaper_stt PRIVATE NOMINMAX)
