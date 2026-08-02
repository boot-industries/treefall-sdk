cmake_minimum_required(VERSION 3.22)
project(fake_sndfile LANGUAGES CXX)

add_library(fake_sndfile STATIC sndfile.cpp)
target_include_directories(fake_sndfile PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
set_target_properties(fake_sndfile PROPERTIES OUTPUT_NAME fake_sndfile)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
install(TARGETS fake_sndfile ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES include/sndfile.h DESTINATION include)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/SndFileConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/SndFileConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SndFile
  NO_CHECK_REQUIRED_COMPONENTS_MACRO)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/SndFileConfig.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SndFile)
