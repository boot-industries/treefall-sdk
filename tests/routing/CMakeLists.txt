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

# Clip routing matrix unit tests
add_executable(clip_routing_test
    clip_routing_test.cpp
)

target_link_libraries(clip_routing_test
    PRIVATE
        orpheus_routing
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(clip_routing_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/core
)

add_test(
    NAME clip_routing_test
    COMMAND clip_routing_test
)

# Multi-channel routing unit tests (Feature 7)
add_executable(multi_channel_routing_test
    multi_channel_routing_test.cpp
)

target_link_libraries(multi_channel_routing_test
    PRIVATE
        orpheus_routing
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(multi_channel_routing_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/core
)

add_test(
    NAME multi_channel_routing_test
    COMMAND multi_channel_routing_test
)

if(TARGET Orpheus::diagnostics)
    add_executable(loudness_meter_test
        loudness_meter_test.cpp
    )

    target_link_libraries(loudness_meter_test
        PRIVATE
            Orpheus::diagnostics
            GTest::gtest
            GTest::gtest_main
    )

    add_test(
        NAME loudness_meter_test
        COMMAND loudness_meter_test
    )
endif()


# ORP134 G3: graph-neutral routing seam + soundboard facade
add_executable(audio_graph_test
    audio_graph_test.cpp
)

target_link_libraries(audio_graph_test
    PRIVATE
        orpheus_routing
        orpheus_session
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(audio_graph_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

add_test(
    NAME audio_graph_test
    COMMAND audio_graph_test
)

orpheus_stage_runtime_dlls_in_directory()
