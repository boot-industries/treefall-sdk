add_library(reaper_atmos MODULE reaper_atmos.cpp)

if(WIN32)
  set_target_properties(reaper_atmos PROPERTIES SUFFIX ".dll")
elseif(APPLE)
  set_target_properties(reaper_atmos PROPERTIES SUFFIX ".dylib")
else()
  set_target_properties(reaper_atmos PROPERTIES SUFFIX ".so")
endif()

set(REAPER_SDK_ROOT ${PROJECT_SOURCE_DIR})
set(REAPER_PLUGINS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_include_directories(reaper_atmos PRIVATE
  ${REAPER_PLUGINS_ROOT}
  ${REAPER_SDK_ROOT}/sdk
  ${REAPER_SDK_ROOT}/WDL/WDL
)

target_compile_features(reaper_atmos PRIVATE cxx_std_17)
target_compile_definitions(reaper_atmos PRIVATE NOMINMAX)
