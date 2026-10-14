# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.22)
project(orpheus_time_point_constexpr_rejection LANGUAGES CXX)

find_package(OrpheusSDK CONFIG REQUIRED)
enable_testing()
add_executable(public_time_point_consumer positive.cpp)
target_compile_features(public_time_point_consumer PRIVATE cxx_std_20)
target_link_libraries(public_time_point_consumer PRIVATE Orpheus::core)
add_test(NAME public_time_point_consumer COMMAND public_time_point_consumer)

add_executable(invalid_time_point_consumer main.cpp)
target_compile_features(invalid_time_point_consumer PRIVATE cxx_std_20)
target_link_libraries(invalid_time_point_consumer PRIVATE Orpheus::core)
