cmake_minimum_required(VERSION 3.20)
project(orpheus_wasm)

# Ensure we're building with Emscripten
if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "This CMakeLists.txt is for Emscripten builds only. Use emcmake.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include Orpheus SDK headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../include)

# Source files for WASM module
set(WASM_SOURCES
  src/wasm_bindings.cpp
)

# Create WASM module
add_executable(orpheus ${WASM_SOURCES})

# Link Orpheus SDK core
target_link_libraries(orpheus PRIVATE orpheus_sdk)

# Emscripten-specific settings
set_target_properties(orpheus PROPERTIES
  SUFFIX ".js"
  LINK_FLAGS "\
    -s WASM=1 \
    -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap'] \
    -s EXPORTED_FUNCTIONS=['_malloc','_free'] \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s MODULARIZE=1 \
    -s EXPORT_NAME='createOrpheusModule' \
    -s ENVIRONMENT=web,worker \
    -s FILESYSTEM=0 \
    --no-entry \
  "
)

# Output to build directory
set_target_properties(orpheus PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build
)
