# SPDX-License-Identifier: MIT
foreach(required_var source_dir binary_dir sdk_build_dir install_prefix)
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

execute_process(
  COMMAND "${CMAKE_COMMAND}" -S "${source_dir}" -B "${binary_dir}"
          -DOrpheusSDK_DIR:PATH=${install_prefix}/lib/cmake/OrpheusSDK
          -DCMAKE_BUILD_TYPE=${build_type}
  RESULT_VARIABLE configure_result
  OUTPUT_VARIABLE configure_output
  ERROR_VARIABLE configure_error)
if(configure_result)
  message(FATAL_ERROR
    "Expected-failure fixture configure failed unexpectedly:\n${configure_output}\n${configure_error}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${binary_dir}" --config "${build_type}"
  RESULT_VARIABLE build_result
  OUTPUT_VARIABLE build_output
  ERROR_VARIABLE build_error)
if(NOT build_result)
  message(FATAL_ERROR "Retired consumer unexpectedly compiled:\n${build_output}\n${build_error}")
endif()

message(STATUS "Retired consumer correctly failed to compile")
