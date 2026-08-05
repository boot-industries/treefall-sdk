# SPDX-License-Identifier: MIT

# ORP127 T8 (G6): polyphase resampler unit test. Standalone — links only
# orpheus_audio_utils (no libsndfile, no JUCE, host-neutral).
add_executable(polyphase_resampler_test
    polyphase_resampler_test.cpp
)

target_link_libraries(polyphase_resampler_test
    PRIVATE
        orpheus_audio_utils
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(polyphase_resampler_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

add_test(NAME polyphase_resampler_test COMMAND polyphase_resampler_test)

add_executable(realtime_borrowed_target_test
    realtime_borrowed_target_test.cpp
)
target_link_libraries(realtime_borrowed_target_test PRIVATE GTest::gtest GTest::gtest_main)
target_include_directories(realtime_borrowed_target_test
    PRIVATE ${CMAKE_SOURCE_DIR}/src/core)
add_test(NAME realtime_borrowed_target_test COMMAND realtime_borrowed_target_test)
set_tests_properties(realtime_borrowed_target_test PROPERTIES LABELS "fast;unit")

# FTR041: sample-accurate, bounded trigger voice contract and RT allocation gate.
add_executable(trigger_voice_test
    trigger_voice_test.cpp
)

target_link_libraries(trigger_voice_test
    PRIVATE
        orpheus_audio_utils
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(trigger_voice_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

add_test(NAME trigger_voice_test COMMAND trigger_voice_test)

# ORP128/FTR018 §2: scrub resampler unit test. Standalone — links only
# orpheus_audio_utils (no libsndfile, no JUCE, host-neutral).
add_executable(scrub_resampler_test
    scrub_resampler_test.cpp
)

target_link_libraries(scrub_resampler_test
    PRIVATE
        orpheus_audio_utils
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(scrub_resampler_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

add_test(NAME scrub_resampler_test COMMAND scrub_resampler_test)

# ORP160/FTR063: prepared/offline Master tape-varispeed contract.
add_executable(tape_varispeed_test
    tape_varispeed_test.cpp
)
target_link_libraries(tape_varispeed_test PRIVATE orpheus_audio_utils GTest::gtest GTest::gtest_main)
target_include_directories(tape_varispeed_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME tape_varispeed_test COMMAND tape_varispeed_test)

# ORP134 G7: lock-free input ring + capture-stream contract. Links audio_io
# so the capture->disk pairing test can use the reader/writer when available.
add_executable(audio_input_test
    audio_input_test.cpp
)

target_link_libraries(audio_input_test
    PRIVATE
        orpheus_audio_io
        orpheus_session
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(audio_input_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

add_test(NAME audio_input_test COMMAND audio_input_test)

# ORP134 G6: analysis facade (standalone; links audio_utils only)
add_executable(audio_analysis_test
    audio_analysis_test.cpp
)

target_link_libraries(audio_analysis_test
    PRIVATE
        orpheus_audio_utils
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(audio_analysis_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/tests
)

add_test(NAME audio_analysis_test COMMAND audio_analysis_test)

add_executable(media_integrity_test media_integrity_test.cpp)
target_link_libraries(media_integrity_test
    PRIVATE orpheus_audio_utils GTest::gtest GTest::gtest_main)
target_include_directories(media_integrity_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME media_integrity_test COMMAND media_integrity_test)


# ORP128/FTR018 §1: backward/windowed read (readSamplesEndingAt). The base-default
# half (in-memory fake reader) is host-neutral; the libsndfile-override half is
# compiled in only when sndfile is present (guarded by ORPHEUS_HAVE_SNDFILE).
add_executable(reverse_read_test
    reverse_read_test.cpp
)

target_link_libraries(reverse_read_test
    PRIVATE
        orpheus_audio_utils
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(reverse_read_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/core
)

if(NOT ORPHEUS_SNDFILE_PROVIDER_RESOLVED STREQUAL "None")
    target_compile_definitions(reverse_read_test PRIVATE ORPHEUS_HAVE_SNDFILE)
endif()

add_test(NAME reverse_read_test COMMAND reverse_read_test)

# Audio file reader tests (requires libsndfile)
if(NOT ORPHEUS_SNDFILE_PROVIDER_RESOLVED STREQUAL "None")
    add_executable(audio_file_reader_test
        audio_file_reader_test.cpp
    )

    target_link_libraries(audio_file_reader_test
        PRIVATE
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )


    target_include_directories(audio_file_reader_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(NAME audio_file_reader_test COMMAND audio_file_reader_test)

    # ORP134 G5 / FTR007: IAudioFileWriter round-trip tests (requires libsndfile)
    add_executable(audio_file_writer_test
        audio_file_writer_test.cpp
    )

    target_link_libraries(audio_file_writer_test
        PRIVATE
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )


    target_include_directories(audio_file_writer_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
            ${CMAKE_SOURCE_DIR}/tests
    )

    add_test(NAME audio_file_writer_test COMMAND audio_file_writer_test)

    # Waveform processor tests (requires libsndfile)
    add_executable(waveform_processor_test
        waveform_processor_test.cpp
    )

    target_link_libraries(waveform_processor_test
        PRIVATE
            orpheus_audio_io
            orpheus_session
            GTest::gtest
            GTest::gtest_main
    )


    target_include_directories(waveform_processor_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
    )

    add_test(NAME waveform_processor_test COMMAND waveform_processor_test)
endif()

# Dummy driver tests
add_executable(dummy_driver_test
    dummy_driver_test.cpp
)

target_link_libraries(dummy_driver_test
    PRIVATE
        orpheus_audio_io
        orpheus_session
        GTest::gtest
        GTest::gtest_main
)


target_include_directories(dummy_driver_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/core
)

add_test(NAME dummy_driver_test COMMAND dummy_driver_test)

# CoreAudio driver tests (macOS only)
if(ORPHEUS_ENABLE_COREAUDIO AND ORPHEUS_ENABLE_EXTENDED_TESTS)
    add_executable(coreaudio_driver_test
        coreaudio_driver_test.cpp
        coreaudio_sample_rate_monitor_test.cpp
        coreaudio_route_monitor_test.cpp
    )

    target_link_libraries(coreaudio_driver_test
        PRIVATE
            orpheus_audio_driver_coreaudio
            orpheus_session
            GTest::gtest
            GTest::gtest_main
            "-framework CoreFoundation"
    )

    target_include_directories(coreaudio_driver_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/core
            ${CMAKE_SOURCE_DIR}/src/platform/audio_drivers
    )

    add_test(NAME coreaudio_driver_test COMMAND coreaudio_driver_test)
    set_tests_properties(coreaudio_driver_test PROPERTIES LABELS "extended;hardware")
endif()

if(WIN32 AND ORPHEUS_ENABLE_WASAPI)
    add_executable(wasapi_driver_test wasapi_driver_test.cpp)
    target_link_libraries(wasapi_driver_test
        PRIVATE orpheus_audio_driver_wasapi orpheus_audio_driver_manager
                GTest::gtest GTest::gtest_main)
    target_include_directories(wasapi_driver_test PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/platform/audio_drivers/wasapi)
    add_test(NAME wasapi_driver_test COMMAND wasapi_driver_test)
    set_tests_properties(wasapi_driver_test PROPERTIES LABELS "extended;hardware")
endif()

# Audio Driver Manager tests
add_executable(driver_manager_test
    driver_manager_test.cpp
    coreaudio_endpoint_monitor_test.cpp
    coreaudio_endpoint_catalog_test.cpp
)

target_link_libraries(driver_manager_test
    PRIVATE
        orpheus_audio_driver_manager
        orpheus_audio_io  # For dummy driver
        orpheus_session
        GTest::gtest
        GTest::gtest_main
)

# If CoreAudio is enabled, link it for hot-swap tests
if(ORPHEUS_ENABLE_COREAUDIO)
    target_link_libraries(driver_manager_test PRIVATE
        orpheus_audio_driver_coreaudio
        "-framework CoreFoundation"
    )
endif()


target_include_directories(driver_manager_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/core
        ${CMAKE_SOURCE_DIR}/src/platform/audio_drivers
)

add_test(NAME driver_manager_test COMMAND driver_manager_test)

# Device hot-swap integration tests
add_executable(device_hot_swap_test
    device_hot_swap_test.cpp
)

target_link_libraries(device_hot_swap_test
    PRIVATE
        orpheus_audio_driver_manager
        orpheus_audio_io  # For dummy driver
        orpheus_session
        GTest::gtest
        GTest::gtest_main
)

# If CoreAudio is enabled, link it for hot-swap tests
if(ORPHEUS_ENABLE_COREAUDIO)
    target_link_libraries(device_hot_swap_test PRIVATE
        orpheus_audio_driver_coreaudio
        "-framework CoreFoundation"
    )
endif()


target_include_directories(device_hot_swap_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/core
)

add_test(NAME device_hot_swap_test COMMAND device_hot_swap_test)
