# SPDX-License-Identifier: MIT
foreach(required_var source_dir binary_dir sdk_build_dir install_prefix
                     EXPECTED_DIAGNOSTIC_SYMBOL EXPECTED_FAILURE_TARGET)
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
          -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF
          -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF
  RESULT_VARIABLE configure_result
  OUTPUT_VARIABLE configure_output
  ERROR_VARIABLE configure_error)
if(configure_result)
  message(FATAL_ERROR
    "Expected-failure fixture configure failed unexpectedly:\n${configure_output}\n${configure_error}")
endif()

# Build and run the equivalent public consumer in this exact clean prefix.
# A dependency/link/runtime failure must not masquerade as rejection of the
# private header, retired symbol or invalid constant expression.
if(DEFINED POSITIVE_TARGET AND NOT POSITIVE_TARGET STREQUAL "")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${binary_dir}" --config "${build_type}"
            --target "${POSITIVE_TARGET}"
    RESULT_VARIABLE positive_build_result
    OUTPUT_VARIABLE positive_build_output
    ERROR_VARIABLE positive_build_error)
  if(positive_build_result)
    message(FATAL_ERROR
      "Public equivalent failed to build:\n${positive_build_output}\n${positive_build_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${binary_dir}"
            --build-config "${build_type}" --output-on-failure
            --no-tests=error -R "^${POSITIVE_TARGET}$"
    RESULT_VARIABLE positive_test_result
    OUTPUT_VARIABLE positive_test_output
    ERROR_VARIABLE positive_test_error)
  if(positive_test_result)
    message(FATAL_ERROR
      "Public equivalent failed to run:\n${positive_test_output}\n${positive_test_error}")
  endif()
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${binary_dir}" --config "${build_type}"
          --target "${EXPECTED_FAILURE_TARGET}"
  RESULT_VARIABLE build_result
  OUTPUT_VARIABLE build_output
  ERROR_VARIABLE build_error)
if(NOT build_result)
  message(FATAL_ERROR "Rejected consumer unexpectedly compiled:\n${build_output}\n${build_error}")
endif()

# Accept only an actual compiler diagnostic with source location, severity,
# and the exact symbol on the same line. Echoed source, build commands,
# configure errors, generic build failures and linker errors are not evidence.
# GCC/Clang: file.cpp:line[:column]: [fatal ]error: ...
# MSVC: file.cpp(line[,column]): [fatal ]error Cxxxx: ...
string(CONCAT diagnostics "${build_output}" "\n" "${build_error}")
string(REPLACE "\r" "" diagnostics "${diagnostics}")
string(REPLACE "\n" ";" diagnostic_lines "${diagnostics}")
set(found_expected_diagnostic OFF)
# The requested names are identifiers or a header basename. Escape the latter
# and require token boundaries so a similarly named symbol cannot satisfy it.
string(REPLACE "." "\\." diagnostic_symbol "${EXPECTED_DIAGNOSTIC_SYMBOL}")
foreach(line IN LISTS diagnostic_lines)
  if(line MATCHES "^[^|]+\\.(c|cc|cpp|cxx|h|hh|hpp|hxx|m|mm):[0-9]+(:[0-9]+)?:[ \t]*(fatal )?error:"
     OR line MATCHES "^[^|]+\\.(c|cc|cpp|cxx|h|hh|hpp|hxx|m|mm)\\([0-9]+(,[0-9]+)?\\)[ \t]*:[ \t]*(fatal )?error [A-Z][0-9]+:")
    if(line MATCHES "(^|[^A-Za-z0-9_])${diagnostic_symbol}([^A-Za-z0-9_]|$)")
      set(found_expected_diagnostic ON)
    endif()
  endif()
endforeach()
if(NOT found_expected_diagnostic)
  message(FATAL_ERROR
    "Build failed without a compiler error naming ${EXPECTED_DIAGNOSTIC_SYMBOL}:\n${diagnostics}")
endif()
message(STATUS "Consumer rejected by compiler diagnostic naming ${EXPECTED_DIAGNOSTIC_SYMBOL}")
