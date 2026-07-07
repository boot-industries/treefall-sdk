# SPDX-License-Identifier: MIT
#
# ORP127 T10: configure + build the add_subdirectory (submodule) consumer smoke
# project. Mirrors run_find_package.cmake but exercises the submodule path.
if(NOT DEFINED source_dir OR NOT DEFINED binary_dir OR NOT DEFINED sdk_source_dir)
  message(FATAL_ERROR "Missing variables for add_subdirectory smoke test")
endif()

set(configure_args
  -S "${source_dir}"
  -B "${binary_dir}"
  -DORPHEUS_SDK_SOURCE_DIR:PATH=${sdk_source_dir}
  -DORP_ENABLE_UBSAN=OFF
  -DORP_ENABLE_ASAN=OFF
  -DCMAKE_CXX_STANDARD=20)

if(DEFINED build_type AND NOT build_type STREQUAL "")
  list(APPEND configure_args -DCMAKE_BUILD_TYPE=${build_type})
endif()

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
  list(APPEND configure_args
    -DCMAKE_OSX_ARCHITECTURES=arm64
    -DCMAKE_CXX_FLAGS=-stdlib=libc++
    -DCMAKE_EXE_LINKER_FLAGS=-stdlib=libc++)
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" ${configure_args}
  RESULT_VARIABLE configure_result)
if(configure_result)
  message(FATAL_ERROR "Configure failed with code ${configure_result}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${binary_dir}"
  RESULT_VARIABLE build_result)
if(build_result)
  message(FATAL_ERROR "Build failed with code ${build_result}")
endif()
