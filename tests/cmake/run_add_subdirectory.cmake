# SPDX-License-Identifier: MIT
#
# ORP127 T10: configure + build the add_subdirectory (submodule) consumer smoke
# project. Mirrors run_find_package.cmake but exercises the submodule path.
if(NOT DEFINED source_dir OR NOT DEFINED binary_dir OR NOT DEFINED sdk_source_dir)
  message(FATAL_ERROR "Missing variables for add_subdirectory smoke test")
endif()


function(run_add_subdirectory mode mode_binary_dir)
  file(REMOVE_RECURSE "${mode_binary_dir}")
  set(configure_args
    -S "${source_dir}"
    -B "${mode_binary_dir}"
    -DORPHEUS_SDK_SOURCE_DIR:PATH=${sdk_source_dir}
    -DTREEFALL_DISCOVERY_MODE=${mode}
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
    message(FATAL_ERROR "Configure failed for add_subdirectory mode ${mode} with code ${configure_result}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${mode_binary_dir}" --config "${build_type}" --parallel 4
    RESULT_VARIABLE build_result)
  if(build_result)
    message(FATAL_ERROR "Build failed for add_subdirectory mode ${mode} with code ${build_result}")
  endif()
  execute_process(
    COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${mode_binary_dir}"
            --build-config "${build_type}" --output-on-failure
    RESULT_VARIABLE test_result)
  if(test_result)
    message(FATAL_ERROR "Source consumer failed for mode ${mode}: ${test_result}")
  endif()
endfunction()

if(DEFINED run_discovery_matrix AND run_discovery_matrix)
  run_add_subdirectory(legacy "${binary_dir}")
  run_add_subdirectory(treefall "${binary_dir}-treefall")
  run_add_subdirectory(legacy-first "${binary_dir}-legacy-first")
  run_add_subdirectory(treefall-first "${binary_dir}-treefall-first")
else()
  run_add_subdirectory(legacy "${binary_dir}")
endif()
