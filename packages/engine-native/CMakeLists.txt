# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.22)

project(orpheus_native)

# Set C++20 standard (matching Orpheus SDK)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform-specific settings
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
  add_compile_options(-stdlib=libc++)
  add_link_options(-stdlib=libc++)
endif()

# Include Node.js headers
execute_process(
  COMMAND node -p "require('node-addon-api').include"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE NODE_ADDON_API_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(REPLACE "\"" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR})

# Find Orpheus SDK
set(ORPHEUS_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "Path to Orpheus SDK root")

# Determine build type (prefer Release for production, Debug for development)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set(ORPHEUS_BUILD_DIR "${ORPHEUS_SDK_ROOT}/build-release")
if(CMAKE_BUILD_TYPE MATCHES "Debug")
  set(ORPHEUS_BUILD_DIR "${ORPHEUS_SDK_ROOT}/build")
endif()

# Include directories
include_directories(
  ${CMAKE_JS_INC}
  ${NODE_ADDON_API_DIR}
  ${ORPHEUS_SDK_ROOT}/include
  ${ORPHEUS_SDK_ROOT}/src
)

# Link directories
link_directories(
  ${ORPHEUS_BUILD_DIR}/src
)

# Locate Orpheus SDK libraries
find_library(ORPHEUS_SESSION_LIB NAMES orpheus_session liborpheus_session
  PATHS
    ${ORPHEUS_BUILD_DIR}/src
    ${ORPHEUS_BUILD_DIR}
    ${ORPHEUS_SDK_ROOT}/build/src
    ${ORPHEUS_SDK_ROOT}/build-release/src
  NO_DEFAULT_PATH
)
find_library(ORPHEUS_CLIPGRID_LIB NAMES orpheus_clipgrid liborpheus_clipgrid
  PATHS
    ${ORPHEUS_BUILD_DIR}/src
    ${ORPHEUS_BUILD_DIR}
    ${ORPHEUS_SDK_ROOT}/build/src
    ${ORPHEUS_SDK_ROOT}/build-release/src
  NO_DEFAULT_PATH
)
find_library(ORPHEUS_RENDER_LIB NAMES orpheus_render liborpheus_render
  PATHS
    ${ORPHEUS_BUILD_DIR}/src
    ${ORPHEUS_BUILD_DIR}
    ${ORPHEUS_SDK_ROOT}/build/src
    ${ORPHEUS_SDK_ROOT}/build-release/src
  NO_DEFAULT_PATH
)

if(NOT ORPHEUS_SESSION_LIB)
  message(FATAL_ERROR "liborpheus_session not found. Build SDK first: cmake -S ${ORPHEUS_SDK_ROOT} -B ${ORPHEUS_BUILD_DIR} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} && cmake --build ${ORPHEUS_BUILD_DIR}")
endif()
if(NOT ORPHEUS_CLIPGRID_LIB)
  message(FATAL_ERROR "liborpheus_clipgrid not found. Build SDK first: cmake -S ${ORPHEUS_SDK_ROOT} -B ${ORPHEUS_BUILD_DIR} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} && cmake --build ${ORPHEUS_BUILD_DIR}")
endif()
if(NOT ORPHEUS_RENDER_LIB)
  message(FATAL_ERROR "liborpheus_render not found. Build SDK first: cmake -S ${ORPHEUS_SDK_ROOT} -B ${ORPHEUS_BUILD_DIR} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} && cmake --build ${ORPHEUS_BUILD_DIR}")
endif()

message(STATUS "Found Orpheus libraries:")
message(STATUS "  orpheus_session: ${ORPHEUS_SESSION_LIB}")
message(STATUS "  orpheus_clipgrid: ${ORPHEUS_CLIPGRID_LIB}")
message(STATUS "  orpheus_render: ${ORPHEUS_RENDER_LIB}")

# Source files
set(SOURCE_FILES
  src/binding.cpp
  src/session_wrapper.cpp
)

# Create native addon library
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})

# Set library properties
set_target_properties(${PROJECT_NAME} PROPERTIES
  PREFIX ""
  SUFFIX ".node"
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
)

# Disable exceptions (node-addon-api requirement)
target_compile_definitions(${PROJECT_NAME} PRIVATE
  NAPI_DISABLE_CPP_EXCEPTIONS
)

# Link Orpheus SDK libraries
target_link_libraries(${PROJECT_NAME}
  ${ORPHEUS_SESSION_LIB}
  ${ORPHEUS_CLIPGRID_LIB}
  ${ORPHEUS_RENDER_LIB}
)

# Platform-specific linking
if(APPLE)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    LINK_FLAGS "-undefined dynamic_lookup"
  )
endif()

# Set RPATH for finding Orpheus libraries
if(APPLE)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    BUILD_RPATH "${ORPHEUS_BUILD_DIR}/src"
    INSTALL_RPATH "@loader_path/../../build-release/src;@loader_path/../../build/src"
  )
elseif(UNIX)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    BUILD_RPATH "${ORPHEUS_BUILD_DIR}/src"
    INSTALL_RPATH "$ORIGIN/../../build-release/src:$ORIGIN/../../build/src"
  )
endif()
