# SPDX-License-Identifier: MIT

# Gain smoother unit tests
add_executable(gain_smoother_test
    gain_smoother_test.cpp
)

target_link_libraries(gain_smoother_test
    PRIVATE
        orpheus_routing
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(gain_smoother_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/core
)

add_test(
    NAME gain_smoother_test
    COMMAND gain_smoother_test
)

# Routing matrix unit tests
add_executable(routing_matrix_test
    routing_matrix_test.cpp
)

target_link_libraries(routing_matrix_test
    PRIVATE
        orpheus_routing
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(routing_matrix_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/core
)

add_test(
    NAME routing_matrix_test
    COMMAND routing_matrix_test
)
