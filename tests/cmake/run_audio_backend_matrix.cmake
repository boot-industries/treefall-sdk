if(NOT DEFINED sdk_source_dir OR NOT DEFINED binary_dir OR NOT DEFINED fixture_dir)
  message(FATAL_ERROR "Missing audio backend matrix paths")
endif()

function(run_checked)
  execute_process(
    COMMAND ${ARGV}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr)
  if(NOT _result EQUAL 0)
    message(FATAL_ERROR
      "Command failed (${_result}): ${ARGV}\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
  endif()
endfunction()

function(expect_configure_failure expected_text)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr)
  if(_result EQUAL 0)
    message(FATAL_ERROR "Expected configure failure for ${expected_text}")
  endif()
  string(CONCAT _output "${_stdout}" "\n" "${_stderr}")
  if(NOT _output MATCHES "${expected_text}")
    message(FATAL_ERROR
      "Configure failed without exact diagnostic '${expected_text}':\n${_output}")
  endif()
endfunction()

file(REMOVE_RECURSE "${binary_dir}")
file(MAKE_DIRECTORY "${binary_dir}")
if(NOT DEFINED MATRIX_MODE)
  set(MATRIX_MODE all)
endif()


if(APPLE)
  set(invalid_options
    ORPHEUS_ENABLE_WASAPI=ON
    ORPHEUS_ENABLE_ASIO=ON)
  set(native_option ORPHEUS_ENABLE_COREAUDIO)
  set(native_backend CoreAudio)
elseif(WIN32)
  set(invalid_options
    ORPHEUS_ENABLE_COREAUDIO=ON)
  set(native_option ORPHEUS_ENABLE_WASAPI)
  set(native_backend WASAPI)
else()
  set(invalid_options
    ORPHEUS_ENABLE_COREAUDIO=ON
    ORPHEUS_ENABLE_WASAPI=ON
    ORPHEUS_ENABLE_ASIO=ON)
  set(native_option ORPHEUS_ENABLE_COREAUDIO)
  set(native_backend CoreAudio)
endif()
if(MATRIX_MODE STREQUAL "invalid" OR MATRIX_MODE STREQUAL "all")

foreach(option IN LISTS invalid_options)
  string(REPLACE "=" ";" option_parts "${option}")
  list(GET option_parts 0 option_name)
  if(option_name STREQUAL "ORPHEUS_ENABLE_COREAUDIO")
    set(expected "ORPHEUS_ENABLE_COREAUDIO requires an Apple platform")
  elseif(option_name STREQUAL "ORPHEUS_ENABLE_WASAPI")
    set(expected "ORPHEUS_ENABLE_WASAPI requires Windows")
  else()
    set(expected "ORPHEUS_ENABLE_ASIO requires Windows")
  endif()
  expect_configure_failure("${expected}"
    "${CMAKE_COMMAND}" -S "${sdk_source_dir}" -B "${binary_dir}/invalid_${option_name}"
    -G Ninja -DCMAKE_BUILD_TYPE=Debug -DORP_WITH_TESTS=OFF
    -DORPHEUS_ENABLE_REALTIME=ON -DORPHEUS_ENABLE_ADAPTERS=OFF
    -DORPHEUS_ENABLE_ADAPTER_MINHOST=OFF -DORPHEUS_ENABLE_ADAPTER_REAPER=OFF
    -DORPHEUS_SNDFILE_PROVIDER=None
    -DORP_ENABLE_ASAN=OFF -DORP_ENABLE_UBSAN=OFF "-D${option}")
endforeach()
endif()


if(MATRIX_MODE STREQUAL "native" OR MATRIX_MODE STREQUAL "all")
set(native_build "${binary_dir}/native_disabled_sdk")
set(native_prefix "${binary_dir}/native_disabled_prefix")
run_checked("${CMAKE_COMMAND}" -S "${sdk_source_dir}" -B "${native_build}" -G Ninja
  -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_INSTALL_PREFIX=${native_prefix}"
  -DORP_WITH_TESTS=OFF -DORPHEUS_ENABLE_REALTIME=ON
  -DORPHEUS_ENABLE_ADAPTERS=OFF -DORPHEUS_ENABLE_ADAPTER_MINHOST=OFF
  -DORPHEUS_ENABLE_ADAPTER_REAPER=OFF -DORPHEUS_SNDFILE_PROVIDER=None
  -DORP_ENABLE_ASAN=OFF -DORP_ENABLE_UBSAN=OFF
  "-D${native_option}=OFF")
run_checked("${CMAKE_COMMAND}" --build "${native_build}" --target install --parallel 4)

set(consumer_build "${binary_dir}/native_disabled_consumer")
run_checked("${CMAKE_COMMAND}" -S "${fixture_dir}" -B "${consumer_build}" -G Ninja
  -DCMAKE_BUILD_TYPE=Debug
  "-DOrpheusSDK_DIR=${native_prefix}/lib/cmake/OrpheusSDK"
  "-DORPHEUS_DISABLED_BACKEND=${native_backend}")
run_checked("${CMAKE_COMMAND}" --build "${consumer_build}" --target audio_backend_consumer --parallel 4)
run_checked("${consumer_build}/audio_backend_consumer${CMAKE_EXECUTABLE_SUFFIX}")
endif()
