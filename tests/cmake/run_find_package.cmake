# SPDX-License-Identifier: MIT
foreach(required_var source_dir binary_dir sdk_build_dir install_prefix required_version)
  if(NOT DEFINED ${required_var} OR "${${required_var}}" STREQUAL "")
    message(FATAL_ERROR "Missing required variable: ${required_var}")
  endif()
endforeach()

file(REMOVE_RECURSE "${install_prefix}" "${binary_dir}")

set(install_args
  --install "${sdk_build_dir}"
  --prefix "${install_prefix}")
if(DEFINED build_type AND NOT build_type STREQUAL "")
  list(APPEND install_args --config "${build_type}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" ${install_args}
  RESULT_VARIABLE install_result)
if(install_result)
  message(FATAL_ERROR "SDK install failed with code ${install_result}")
endif()

set(configure_args
  -S "${source_dir}"
  -B "${binary_dir}"
  -DOrpheusSDK_DIR:PATH=${install_prefix}/lib/cmake/OrpheusSDK
  -DORPHEUS_REQUIRED_VERSION=${required_version}
  -DORP_ENABLE_UBSAN=OFF)

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
  RESULT_VARIABLE configure_result
  OUTPUT_VARIABLE configure_stdout
  ERROR_VARIABLE configure_stderr)
string(CONCAT configure_output "${configure_stdout}" "\n" "${configure_stderr}")

if(DEFINED expect_configure_failure AND expect_configure_failure)
  if(NOT configure_result)
    message(FATAL_ERROR
      "Configure unexpectedly accepted incompatible SDK version ${required_version}")
  endif()

  string(FIND "${configure_output}"
    "compatible with requested version \"${required_version}\""
    version_rejection_index)
  if(version_rejection_index EQUAL -1)
    message(FATAL_ERROR
      "Configure failed for an unexpected reason:\n${configure_output}")
  endif()

  message(STATUS "Configure correctly rejected incompatible SDK version ${required_version}")
  return()
endif()
if(configure_result)
  message(FATAL_ERROR
    "Configure failed with code ${configure_result}:\n${configure_output}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${binary_dir}" --config "${build_type}" --parallel
  RESULT_VARIABLE build_result)
if(build_result)
  message(FATAL_ERROR "Build failed with code ${build_result}")
endif()

execute_process(
  COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${binary_dir}" --output-on-failure
          --build-config "${build_type}"
  RESULT_VARIABLE test_result)
if(test_result)
  message(FATAL_ERROR "Installed fixture tests failed with code ${test_result}")
endif()
