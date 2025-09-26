cmake_minimum_required(VERSION 3.22)

project(orpheus LANGUAGES CXX)

option(ORP_WITH_TESTS "Build Orpheus tests" ON)
option(ORP_BUILD_REAPER "Build the REAPER adapter" ON)
option(ORP_BUILD_MINHOST "Build the minimal host adapter" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT MSVC)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address,undefined")
  set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fsanitize=address,undefined")
else()
  message(STATUS "Sanitizers are not enabled on MSVC builds.")
endif()

add_subdirectory(src)

if(ORP_BUILD_REAPER)
  add_subdirectory(adapters/reaper)
endif()

if(ORP_BUILD_MINHOST)
  add_subdirectory(adapters/minhost)
endif()

include(CTest)
if(ORP_WITH_TESTS)
  include(${CMAKE_SOURCE_DIR}/cmake/Dependencies.cmake)
  enable_testing()
  add_subdirectory(tests)
endif()
