# SPDX-License-Identifier: MIT
if(NOT DEFINED source_dir OR NOT DEFINED binary_dir OR NOT DEFINED shmui_source_dir)
  message(FATAL_ERROR "Missing variables for ShmUI no-OpenGL smoke test")
endif()

get_filename_component(source_dir "${source_dir}" ABSOLUTE)
get_filename_component(binary_dir "${binary_dir}" ABSOLUTE)
get_filename_component(shmui_source_dir "${shmui_source_dir}" ABSOLUTE)

set(configure_args
  -S "${source_dir}"
  -B "${binary_dir}"
  -DSHMUI_JUCE_SOURCE_DIR:PATH=${shmui_source_dir}
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
  message(FATAL_ERROR "ShmUI no-OpenGL configure failed with code ${configure_result}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${binary_dir}" --config "${build_type}" --parallel
  RESULT_VARIABLE build_result)
if(build_result)
  message(FATAL_ERROR "ShmUI no-OpenGL build failed with code ${build_result}")
endif()

execute_process(
  COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${binary_dir}" --output-on-failure
    --build-config "${build_type}"
  RESULT_VARIABLE test_result)
if(test_result)
  message(FATAL_ERROR "ShmUI no-OpenGL smoke test failed with code ${test_result}")
endif()
