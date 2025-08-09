add_library(reaper_atmos MODULE reaper_atmos.cpp)

set_target_properties(reaper_atmos PROPERTIES PREFIX "")

if(WIN32)
  set_target_properties(reaper_atmos PROPERTIES SUFFIX ".dll")
  target_compile_definitions(reaper_atmos PRIVATE WIN32 _WINDOWS NOMINMAX)
elseif(APPLE)
  set_target_properties(reaper_atmos PROPERTIES SUFFIX ".dylib")
  target_compile_definitions(reaper_atmos PRIVATE MAC_OS_X)
else()
  set_target_properties(reaper_atmos PROPERTIES SUFFIX ".so")
  target_compile_definitions(reaper_atmos PRIVATE LINUX)
  target_compile_options(reaper_atmos PRIVATE -fPIC)
endif()

target_include_directories(reaper_atmos PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../sdk
  ${CMAKE_CURRENT_SOURCE_DIR}/../../WDL/WDL
)

target_compile_features(reaper_atmos PRIVATE cxx_std_17)
target_compile_definitions(reaper_atmos PRIVATE WDL_NO_DEFINE_MINMAX)
