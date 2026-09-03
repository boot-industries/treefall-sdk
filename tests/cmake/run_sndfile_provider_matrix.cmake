if(NOT DEFINED sdk_source_dir OR NOT DEFINED binary_dir OR NOT DEFINED fixture_dir)
  message(FATAL_ERROR "Missing sndfile provider matrix paths")
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

file(REMOVE_RECURSE "${binary_dir}")
file(MAKE_DIRECTORY "${binary_dir}")
set(ENV{CMAKE_TOOLCHAIN_FILE})
set(fake_build "${binary_dir}/fake_build")
set(fake_prefix "${binary_dir}/fake_prefix")
set(msvc_runtime_args)
if(WIN32)
  list(APPEND msvc_runtime_args -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded)
endif()
if(WIN32)
  set(matrix_generator "Visual Studio 17 2022")
else()
  set(matrix_generator Ninja)
endif()

run_checked("${CMAKE_COMMAND}" -S "${fixture_dir}" -B "${fake_build}"
  -G "${matrix_generator}"
  -DCMAKE_BUILD_TYPE=Debug
  ${msvc_runtime_args}
  "-DCMAKE_INSTALL_PREFIX=${fake_prefix}")
run_checked("${CMAKE_COMMAND}" --build "${fake_build}"
  --config Debug --target install --parallel 4)

foreach(provider IN ITEMS SndFile PkgConfig None)
  set(provider_build "${binary_dir}/${provider}/sdk_build")
  set(provider_prefix "${binary_dir}/${provider}/sdk_prefix")
  set(consumer_build "${binary_dir}/${provider}/consumer_build")
  file(MAKE_DIRECTORY "${binary_dir}/${provider}")
  set(sdk_args
    -S "${sdk_source_dir}"
    -B "${provider_build}"
    -G "${matrix_generator}"
    -DCMAKE_BUILD_TYPE=Debug
    "-DCMAKE_INSTALL_PREFIX=${provider_prefix}"
    -DORP_WITH_TESTS=OFF
    -DORPHEUS_ENABLE_REALTIME=ON
    -DORPHEUS_ENABLE_EXTENDED_TESTS=OFF
    -DORPHEUS_ENABLE_ADAPTERS=OFF
    -DORPHEUS_ENABLE_ADAPTER_MINHOST=OFF
    -DORPHEUS_ENABLE_ADAPTER_REAPER=OFF
    -DORPHEUS_ENABLE_COREAUDIO=OFF
    -DORPHEUS_ENABLE_WASAPI=OFF
    -DORPHEUS_ENABLE_ASIO=OFF
    -DORP_BUILD_ABI_DYNAMIC=OFF
    -DORP_BUILD_SHARED_CORE=OFF
    -DORP_ENABLE_ASAN=OFF
    -DORP_ENABLE_UBSAN=OFF
    ${msvc_runtime_args}
    "-DORPHEUS_SNDFILE_PROVIDER=${provider}")
  if(provider STREQUAL "SndFile")
    list(APPEND sdk_args
      "-DCMAKE_PREFIX_PATH=${fake_prefix}"
      "-DSndFile_DIR=${fake_prefix}/lib/cmake/SndFile")
  elseif(provider STREQUAL "PkgConfig")
    list(APPEND sdk_args "-DCMAKE_MODULE_PATH=${fixture_dir}"
      "-DFAKE_SNDFILE_PREFIX=${fake_prefix}")
  endif()
  run_checked("${CMAKE_COMMAND}" ${sdk_args})
  run_checked("${CMAKE_COMMAND}" --build "${provider_build}"
    --config Debug --target install --parallel 4)

  set(export_file "${provider_prefix}/lib/cmake/OrpheusSDK/OrpheusSDKTargets.cmake")
  file(READ "${export_file}" export_text)
  string(FIND "${export_text}" "${sdk_source_dir}" source_path)
  string(FIND "${export_text}" "${provider_build}" build_path)
  string(FIND "${export_text}" "Cellar" cellar_path)
  if(NOT source_path EQUAL -1 OR NOT build_path EQUAL -1 OR NOT cellar_path EQUAL -1)
    message(FATAL_ERROR "${provider} export embeds a producer path")
  endif()

  set(consumer_args
    -S "${fixture_dir}/consumer"
    -B "${consumer_build}"
    -G "${matrix_generator}"
    -DCMAKE_BUILD_TYPE=Debug
    ${msvc_runtime_args}
    "-DORPHEUS_EXPECT_PROVIDER=${provider}"
    "-DOrpheusSDK_DIR=${provider_prefix}/lib/cmake/OrpheusSDK"
    "-DCMAKE_PREFIX_PATH=${fake_prefix}")
  if(provider STREQUAL "SndFile")
    list(APPEND consumer_args
      "-DSndFile_DIR=${fake_prefix}/lib/cmake/SndFile")
  elseif(provider STREQUAL "PkgConfig")
    list(APPEND consumer_args "-DCMAKE_MODULE_PATH=${fixture_dir}"
      "-DFAKE_SNDFILE_PREFIX=${fake_prefix}")
  endif()
  run_checked("${CMAKE_COMMAND}" ${consumer_args})
  run_checked("${CMAKE_COMMAND}" --build "${consumer_build}"
    --config Debug --target sndfile_provider_consumer --parallel 4)
  if(WIN32)
    set(consumer_executable
      "${consumer_build}/Debug/sndfile_provider_consumer.exe")
  else()
    set(consumer_executable
      "${consumer_build}/sndfile_provider_consumer${CMAKE_EXECUTABLE_SUFFIX}")
  endif()
  run_checked("${consumer_executable}")
endforeach()
