add_library(orpheus_core
  core/abi.cpp
  core/session.cpp
)

set_target_properties(orpheus_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(orpheus_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(orpheus_core PUBLIC cxx_std_20)

include(${CMAKE_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
orpheus_enable_warnings(orpheus_core)
