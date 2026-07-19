# SPDX-License-Identifier: MIT
foreach(required_var
    sdk_source_dir producer_source_dir producer_binary_dir consumer_source_dir
    consumer_binary_dir install_prefix dependency_dir required_version)
  if(NOT DEFINED ${required_var} OR "${${required_var}}" STREQUAL "")
    message(FATAL_ERROR "Missing variable: ${required_var}")
  endif()
endforeach()

# JUCE configures its helper executable during CMake generation. Keep that
# nested build bounded and avoid serializing the package gate.
set(ENV{CMAKE_BUILD_PARALLEL_LEVEL} 8)

file(REMOVE_RECURSE
  "${producer_binary_dir}"
  "${consumer_binary_dir}"
  "${install_prefix}")

set(producer_configure_args
  -S "${producer_source_dir}"
  -B "${producer_binary_dir}"
  -DORPHEUS_SOURCE_DIR:PATH=${sdk_source_dir}
  -DFETCHCONTENT_BASE_DIR:PATH=${dependency_dir})

if(DEFINED build_type AND NOT build_type STREQUAL "")
  list(APPEND producer_configure_args -DCMAKE_BUILD_TYPE=${build_type})
endif()

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
  list(APPEND producer_configure_args
    -DCMAKE_OSX_ARCHITECTURES=arm64
    -DCMAKE_CXX_FLAGS=-stdlib=libc++
    -DCMAKE_EXE_LINKER_FLAGS=-stdlib=libc++)
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" ${producer_configure_args}
  RESULT_VARIABLE producer_configure_result)
if(producer_configure_result)
  message(FATAL_ERROR
    "ShmUI package producer configure failed with code ${producer_configure_result}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${producer_binary_dir}" --parallel
  RESULT_VARIABLE producer_build_result)
if(producer_build_result)
  message(FATAL_ERROR "ShmUI package producer build failed with code ${producer_build_result}")
endif()

set(install_args --install "${producer_binary_dir}" --prefix "${install_prefix}")
if(DEFINED build_type AND NOT build_type STREQUAL "")
  list(APPEND install_args --config "${build_type}")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" ${install_args}
  RESULT_VARIABLE install_result)
if(install_result)
  message(FATAL_ERROR "ShmUI package install failed with code ${install_result}")
endif()

set(consumer_configure_args
  -S "${consumer_source_dir}"
  -B "${consumer_binary_dir}"
  -DOrpheusSDK_DIR:PATH=${install_prefix}/lib/cmake/OrpheusSDK
  -DORPHEUS_REQUIRED_VERSION=${required_version}
  -DFETCHCONTENT_BASE_DIR:PATH=${dependency_dir})

if(DEFINED build_type AND NOT build_type STREQUAL "")
  list(APPEND consumer_configure_args -DCMAKE_BUILD_TYPE=${build_type})
endif()

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
  list(APPEND consumer_configure_args
    -DCMAKE_OSX_ARCHITECTURES=arm64
    -DCMAKE_CXX_FLAGS=-stdlib=libc++
    -DCMAKE_EXE_LINKER_FLAGS=-stdlib=libc++)
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" ${consumer_configure_args}
  RESULT_VARIABLE consumer_configure_result)
if(consumer_configure_result)
  message(FATAL_ERROR
    "ShmUI package consumer configure failed with code ${consumer_configure_result}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${consumer_binary_dir}" --parallel
  RESULT_VARIABLE consumer_build_result)
if(consumer_build_result)
  message(FATAL_ERROR "ShmUI package consumer build failed with code ${consumer_build_result}")
endif()

execute_process(
  COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${consumer_binary_dir}" --output-on-failure
  RESULT_VARIABLE consumer_test_result)
if(consumer_test_result)
  message(FATAL_ERROR
    "ShmUI package consumer test failed with code ${consumer_test_result}")
endif()
