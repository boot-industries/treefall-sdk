# SPDX-License-Identifier: MIT

# Unit tests
add_executable(transport_controller_test
    transport_controller_test.cpp
)

target_link_libraries(transport_controller_test
    PRIVATE
        orpheus_transport
        orpheus_audio_io
        orpheus_session
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(transport_controller_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/core
)

add_test(
    NAME transport_controller_test
    COMMAND transport_controller_test
)

# ORP133 G3: debug-build single-producer contract assertion (death test)
add_executable(command_producer_assert_test
    command_producer_assert_test.cpp
)

target_link_libraries(command_producer_assert_test
    PRIVATE
        orpheus_transport
        orpheus_audio_io
        orpheus_session
        GTest::gtest
)

target_include_directories(command_producer_assert_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/core
)

add_test(
    NAME command_producer_assert_test
    COMMAND command_producer_assert_test
)

# Fade processing tests
add_executable(fade_processing_test
    fade_processing_test.cpp
)

target_link_libraries(fade_processing_test
    PRIVATE
        orpheus_transport
        orpheus_audio_io
        orpheus_session
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(fade_processing_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/core
)

add_test(
    NAME fade_processing_test
    COMMAND fade_processing_test
)

# OUT point enforcement tests (ORP091, ORP093)
add_executable(out_point_enforcement_test
    out_point_enforcement_test.cpp
)

target_link_libraries(out_point_enforcement_test
    PRIVATE
        orpheus_transport
        orpheus_audio_io
        orpheus_session
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(out_point_enforcement_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/core
)

add_test(
    NAME out_point_enforcement_test
    COMMAND out_point_enforcement_test
)

# Integration tests (requires orpheus_audio_io)
if(TARGET orpheus_audio_io)
    # Find libsndfile (needed because orpheus_audio_io is static)
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(SNDFILE sndfile)
    endif()

    if(NOT SNDFILE_FOUND)
        find_library(SNDFILE_LIBRARIES NAMES sndfile libsndfile)
        find_path(SNDFILE_INCLUDE_DIRS NAMES sndfile.h)
        if(SNDFILE_LIBRARIES AND SNDFILE_INCLUDE_DIRS)
            set(SNDFILE_FOUND TRUE)
        endif()
    endif()

    add_executable(transport_integration_test
        integration_test.cpp
    )

    target_link_libraries(transport_integration_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    # Link libsndfile if found (needed because orpheus_audio_io is static)
    if(SNDFILE_FOUND)
        target_link_libraries(transport_integration_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(transport_integration_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(transport_integration_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(
        NAME transport_integration_test
        COMMAND transport_integration_test
    )

    add_executable(multichannel_transport_test
        multichannel_transport_test.cpp
    )

    target_link_libraries(multichannel_transport_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    if(SNDFILE_FOUND)
        target_link_libraries(multichannel_transport_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(multichannel_transport_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(multichannel_transport_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(
        NAME multichannel_transport_test
        COMMAND multichannel_transport_test
    )

    # Multi-clip stress test
    add_executable(multi_clip_stress_test
        multi_clip_stress_test.cpp
    )

    target_link_libraries(multi_clip_stress_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    # Link libsndfile if found (needed because orpheus_audio_io is static)
    if(SNDFILE_FOUND)
        target_link_libraries(multi_clip_stress_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(multi_clip_stress_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(multi_clip_stress_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(
        NAME multi_clip_stress_test
        COMMAND multi_clip_stress_test
    )

    # Clip gain tests (ORP099 Task 1.1)
    add_executable(clip_gain_test
        clip_gain_test.cpp
    )

    target_link_libraries(clip_gain_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    # Link libsndfile if found (needed because orpheus_audio_io is static)
    if(SNDFILE_FOUND)
        target_link_libraries(clip_gain_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(clip_gain_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(clip_gain_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(
        NAME clip_gain_test
        COMMAND clip_gain_test
    )

    # Clip loop tests (ORP099 Task 1.2)
    add_executable(clip_loop_test
        clip_loop_test.cpp
    )

    target_link_libraries(clip_loop_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    # Link libsndfile if found
    if(SNDFILE_FOUND)
        target_link_libraries(clip_loop_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(clip_loop_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(clip_loop_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(
        NAME clip_loop_test
        COMMAND clip_loop_test
    )

    # Clip metadata tests (ORP099 Task 1.3)
    add_executable(clip_metadata_test
        clip_metadata_test.cpp
    )

    target_link_libraries(clip_metadata_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    # Link libsndfile if found
    if(SNDFILE_FOUND)
        target_link_libraries(clip_metadata_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(clip_metadata_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(clip_metadata_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(
        NAME clip_metadata_test
        COMMAND clip_metadata_test
    )

    # Clip restart tests
    add_executable(clip_restart_test
        clip_restart_test.cpp
    )

    target_link_libraries(clip_restart_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    # Link libsndfile if found
    if(SNDFILE_FOUND)
        target_link_libraries(clip_restart_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(clip_restart_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(clip_restart_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(
        NAME clip_restart_test
        COMMAND clip_restart_test
    )

    # Clip seek tests
    add_executable(clip_seek_test
        clip_seek_test.cpp
    )

    target_link_libraries(clip_seek_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    # Link libsndfile if found
    if(SNDFILE_FOUND)
        target_link_libraries(clip_seek_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(clip_seek_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(clip_seek_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(
        NAME clip_seek_test
        COMMAND clip_seek_test
    )

    # Clip cue points tests (ORP109 Feature 6)
    add_executable(clip_cue_points_test
        clip_cue_points_test.cpp
    )

    target_link_libraries(clip_cue_points_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    # Link libsndfile if found
    if(SNDFILE_FOUND)
        target_link_libraries(clip_cue_points_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(clip_cue_points_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(clip_cue_points_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
            ${CMAKE_SOURCE_DIR}/tests
    )

    add_test(
        NAME clip_cue_points_test
        COMMAND clip_cue_points_test
    )

    # ORP121 Q-07: Callback queue stress tests
    add_executable(callback_queue_stress_test
        callback_queue_stress_test.cpp
    )

    target_link_libraries(callback_queue_stress_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    # Link libsndfile if found
    if(SNDFILE_FOUND)
        target_link_libraries(callback_queue_stress_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(callback_queue_stress_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(callback_queue_stress_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(
        NAME callback_queue_stress_test
        COMMAND callback_queue_stress_test
    )

    # ORP127 T2: ThreadSanitizer stress harness for per-voice state.
    # Builds/runs under normal Debug too (liveness check); TSan is the verdict.
    add_executable(voice_state_tsan_test
        voice_state_tsan_test.cpp
    )

    target_link_libraries(voice_state_tsan_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    # Link libsndfile if found (needed because orpheus_audio_io is static)
    if(SNDFILE_FOUND)
        target_link_libraries(voice_state_tsan_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(voice_state_tsan_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(voice_state_tsan_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(
        NAME voice_state_tsan_test
        COMMAND voice_state_tsan_test
    )

    # ORP127 T4: per-sample stop fade envelope measurement (G2)
    add_executable(stop_fade_envelope_test
        stop_fade_envelope_test.cpp
    )

    target_link_libraries(stop_fade_envelope_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    if(SNDFILE_FOUND)
        target_link_libraries(stop_fade_envelope_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(stop_fade_envelope_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(stop_fade_envelope_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(
        NAME stop_fade_envelope_test
        COMMAND stop_fade_envelope_test
    )

    # ORP127 T6: clip gain smoothing (anti-zipper) measurement (G4)
    add_executable(clip_gain_smoothing_test
        clip_gain_smoothing_test.cpp
    )

    target_link_libraries(clip_gain_smoothing_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    if(SNDFILE_FOUND)
        target_link_libraries(clip_gain_smoothing_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(clip_gain_smoothing_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(clip_gain_smoothing_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(
        NAME clip_gain_smoothing_test
        COMMAND clip_gain_smoothing_test
    )

    # ORP127 T7: voice mode API (G5)
    add_executable(voice_mode_test
        voice_mode_test.cpp
    )

    target_link_libraries(voice_mode_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    if(SNDFILE_FOUND)
        target_link_libraries(voice_mode_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(voice_mode_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(voice_mode_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(
        NAME voice_mode_test
        COMMAND voice_mode_test
    )

    # ORP127 T9: choke primitive + configurable voice caps (G7)
    add_executable(choke_and_voice_cap_test
        choke_and_voice_cap_test.cpp
    )

    target_link_libraries(choke_and_voice_cap_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    if(SNDFILE_FOUND)
        target_link_libraries(choke_and_voice_cap_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(choke_and_voice_cap_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(choke_and_voice_cap_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(
        NAME choke_and_voice_cap_test
        COMMAND choke_and_voice_cap_test
    )

    # ORP150: one-command atomic metadata-group start/choke admission.
    add_executable(atomic_group_choke_test
        atomic_group_choke_test.cpp
    )

    target_link_libraries(atomic_group_choke_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    if(SNDFILE_FOUND)
        target_link_libraries(atomic_group_choke_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(atomic_group_choke_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(atomic_group_choke_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(
        NAME atomic_group_choke_test
        COMMAND atomic_group_choke_test
    )

    # OCC155: SDK API requests — getRoutingMatrix / panic / unregisterClipAudio,
    # plus the fade-overlap re-fire independent-cursor regression (Ask #2).
    add_executable(occ155_api_test
        occ155_api_test.cpp
    )

    target_link_libraries(occ155_api_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    if(SNDFILE_FOUND)
        target_link_libraries(occ155_api_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(occ155_api_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(occ155_api_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(
        NAME occ155_api_test
        COMMAND occ155_api_test
    )

    # ORP136 §2.2: runtime realtime-safety harness (alloc hooks + /proc/self/io).
    # Gates ORP134 G1: proves at runtime whether the audio thread allocates or
    # performs file I/O — not just that forbidden tokens are absent statically.
    add_executable(realtime_harness_test
        realtime_harness_test.cpp
    )

    target_link_libraries(realtime_harness_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
    )

    if(SNDFILE_FOUND)
        target_link_libraries(realtime_harness_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(realtime_harness_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(realtime_harness_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
            ${CMAKE_SOURCE_DIR}/tests
    )

    add_test(
        NAME realtime_harness_test
        COMMAND realtime_harness_test
    )

    # ORP136 §2.3: golden render-hash determinism gate (block-size invariance
    # + run-to-run reproducibility). Parity oracle for the ORP134 G1 migration.
    add_executable(transport_render_hash_test
        transport_render_hash_test.cpp
    )

    target_link_libraries(transport_render_hash_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
    )

    if(SNDFILE_FOUND)
        target_link_libraries(transport_render_hash_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(transport_render_hash_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(transport_render_hash_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
            ${CMAKE_SOURCE_DIR}/tests
    )

    add_test(
        NAME transport_render_hash_test
        COMMAND transport_render_hash_test
    )

    # ORP127 T8: end-to-end sample-rate conversion in the transport (G6)
    add_executable(sample_rate_conversion_test
        sample_rate_conversion_test.cpp
    )

    target_link_libraries(sample_rate_conversion_test
        PRIVATE
            orpheus_transport
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )

    if(SNDFILE_FOUND)
        target_link_libraries(sample_rate_conversion_test PRIVATE ${SNDFILE_LIBRARIES})
        if(SNDFILE_LIBRARY_DIRS)
            target_link_directories(sample_rate_conversion_test PRIVATE ${SNDFILE_LIBRARY_DIRS})
        endif()
    endif()

    target_include_directories(sample_rate_conversion_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(
        NAME sample_rate_conversion_test
        COMMAND sample_rate_conversion_test
    )
endif()
