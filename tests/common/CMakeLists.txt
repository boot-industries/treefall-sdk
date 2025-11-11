# SPDX-License-Identifier: MIT

# Performance Monitor Unit Tests
add_executable(performance_monitor_test
    performance_monitor_test.cpp
)

target_link_libraries(performance_monitor_test
    PRIVATE
        orpheus_session
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(performance_monitor_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/core
)

add_test(
    NAME performance_monitor_test
    COMMAND performance_monitor_test
)

# Performance Monitor Integration Tests
add_executable(performance_integration_test
    performance_integration_test.cpp
)

target_link_libraries(performance_integration_test
    PRIVATE
        orpheus_transport
        orpheus_session
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(performance_integration_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/core
)

add_test(
    NAME performance_integration_test
    COMMAND performance_integration_test
)
