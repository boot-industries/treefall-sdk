add_library(reaper_stt MODULE reaper_stt.cpp)

set_target_properties(reaper_stt PROPERTIES PREFIX "")

if(WIN32)
  set_target_properties(reaper_stt PROPERTIES SUFFIX ".dll")
  target_compile_definitions(reaper_stt PRIVATE WIN32 _WINDOWS NOMINMAX)
elseif(APPLE)
  set_target_properties(reaper_stt PROPERTIES SUFFIX ".dylib")
  target_compile_definitions(reaper_stt PRIVATE MAC_OS_X)
else()
  set_target_properties(reaper_stt PROPERTIES SUFFIX ".so")
  target_compile_definitions(reaper_stt PRIVATE LINUX)
  target_compile_options(reaper_stt PRIVATE -fPIC)
endif()

target_include_directories(reaper_stt PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../sdk
  ${CMAKE_CURRENT_SOURCE_DIR}/../../WDL/WDL
)

target_compile_features(reaper_stt PRIVATE cxx_std_17)
target_compile_definitions(reaper_stt PRIVATE WDL_NO_DEFINE_MINMAX)
