# SPDX-License-Identifier: MIT
if(NOT DEFINED source_dir OR NOT DEFINED binary_dir OR NOT DEFINED OrpheusSDK_DIR)
  message(FATAL_ERROR "Missing variables for find_package smoke test")
endif()

set(configure_args
  -S "${source_dir}"
  -B "${binary_dir}"
  -DOrpheusSDK_DIR:PATH=${OrpheusSDK_DIR}
  -DORP_ENABLE_UBSAN=OFF
  -DCMAKE_CXX_STANDARD=17)

if(CMAKE_BUILD_TYPE)
  list(APPEND configure_args -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})
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
