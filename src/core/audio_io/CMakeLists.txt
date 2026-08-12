# SPDX-License-Identifier: MIT

# Select one consumer-resolved libsndfile target. Raw library/include paths
# must never enter an installed SDK target.
set(ORPHEUS_SNDFILE_PROVIDER "Auto" CACHE STRING
    "libsndfile provider: Auto, SndFile, PkgConfig, or None")
set_property(CACHE ORPHEUS_SNDFILE_PROVIDER PROPERTY STRINGS
    Auto SndFile PkgConfig None)
set(_orpheus_sndfile_provider_values Auto SndFile PkgConfig None)
if(NOT ORPHEUS_SNDFILE_PROVIDER IN_LIST _orpheus_sndfile_provider_values)
    message(FATAL_ERROR
        "ORPHEUS_SNDFILE_PROVIDER must be one of Auto, SndFile, PkgConfig, or None")
endif()

set(_orpheus_sndfile_target "")
set(ORPHEUS_SNDFILE_PROVIDER_RESOLVED "None" CACHE INTERNAL
    "Resolved libsndfile provider" FORCE)

if(NOT ORPHEUS_SNDFILE_PROVIDER STREQUAL "None")
    if(ORPHEUS_SNDFILE_PROVIDER STREQUAL "Auto" OR
       ORPHEUS_SNDFILE_PROVIDER STREQUAL "SndFile")
        find_package(SndFile CONFIG QUIET)
        if(TARGET SndFile::sndfile)
            set(_orpheus_sndfile_target SndFile::sndfile)
            set(ORPHEUS_SNDFILE_PROVIDER_RESOLVED "SndFile" CACHE INTERNAL
                "Resolved libsndfile provider" FORCE)
        elseif(ORPHEUS_SNDFILE_PROVIDER STREQUAL "SndFile")
            message(FATAL_ERROR
                "ORPHEUS_SNDFILE_PROVIDER=SndFile requires SndFile::sndfile")
        endif()
    endif()

    if(NOT _orpheus_sndfile_target AND
       (ORPHEUS_SNDFILE_PROVIDER STREQUAL "Auto" OR
        ORPHEUS_SNDFILE_PROVIDER STREQUAL "PkgConfig"))
        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(SNDFILE QUIET IMPORTED_TARGET sndfile)
        endif()
        if(TARGET PkgConfig::SNDFILE)
            set(_orpheus_sndfile_target PkgConfig::SNDFILE)
            set(ORPHEUS_SNDFILE_PROVIDER_RESOLVED "PkgConfig" CACHE INTERNAL
                "Resolved libsndfile provider" FORCE)
        elseif(ORPHEUS_SNDFILE_PROVIDER STREQUAL "PkgConfig")
            message(FATAL_ERROR
                "ORPHEUS_SNDFILE_PROVIDER=PkgConfig requires PkgConfig::SNDFILE")
        endif()
    endif()
endif()


# Build audio utilities with available components
set(ORPHEUS_AUDIO_UTILS_SOURCES)

# ORP127 G6: host-neutral, dependency-free sample-rate conversion. Always built.
list(APPEND ORPHEUS_AUDIO_UTILS_SOURCES polyphase_resampler.cpp)
list(APPEND ORPHEUS_AUDIO_UTILS_SOURCES directional_sample_rate_converter.cpp)

# FTR041: host-neutral, real-time-safe one-shot voice pool. Always built.
list(APPEND ORPHEUS_AUDIO_UTILS_SOURCES trigger_voice.cpp)

# ORP128/FTR018 §1: base-interface default (readSamplesEndingAt). Host-neutral,
# dependency-free — always built.
list(APPEND ORPHEUS_AUDIO_UTILS_SOURCES audio_file_reader.cpp)

# ORP128/FTR018 §2: variable-rate + reverse scrub resampler. Host-neutral,
# dependency-free — always built.
list(APPEND ORPHEUS_AUDIO_UTILS_SOURCES scrub_resampler.cpp)

# ORP160/FTR063: prepared premium tape-varispeed processor. Always built.
list(APPEND ORPHEUS_AUDIO_UTILS_SOURCES tape_varispeed.cpp)

# ORP134 G7: lock-free input ring + capture-stream contract. Host-neutral,
# dependency-free — always built.
list(APPEND ORPHEUS_AUDIO_UTILS_SOURCES audio_input.cpp)

# ORP134 G6: offline analysis facade (FFT/STFT + wrappers over LoudnessMeter
# and buffer statistics). Host-neutral, dependency-free — always built.
list(APPEND ORPHEUS_AUDIO_UTILS_SOURCES audio_analysis.cpp)

# R2: versioned media fingerprints and deterministic resolution.
list(APPEND ORPHEUS_AUDIO_UTILS_SOURCES media_integrity.cpp)

if(NOT ORPHEUS_SNDFILE_PROVIDER_RESOLVED STREQUAL "None")
    list(APPEND ORPHEUS_AUDIO_UTILS_SOURCES audio_file_reader_libsndfile.cpp)
    list(APPEND ORPHEUS_AUDIO_UTILS_SOURCES audio_file_writer_libsndfile.cpp) # ORP134 G5 / FTR007
    list(APPEND ORPHEUS_AUDIO_UTILS_SOURCES resampling_audio_file_reader.cpp)
    list(APPEND ORPHEUS_AUDIO_UTILS_SOURCES waveform_processor.cpp)
    message(STATUS "✓ libsndfile FOUND - audio file reading/writing enabled (with waveform processing)")
else()
    # Provide stub implementations when libsndfile is not available
    # This ensures the factory symbols exist for linking
    list(APPEND ORPHEUS_AUDIO_UTILS_SOURCES create_audio_file_reader_stub.cpp)
    list(APPEND ORPHEUS_AUDIO_UTILS_SOURCES create_audio_file_writer_stub.cpp)
    message(STATUS "✗ libsndfile NOT FOUND - using stub implementation (audio file I/O disabled)")
endif()

add_library(orpheus_audio_utils STATIC
    ${ORPHEUS_AUDIO_UTILS_SOURCES}
    $<TARGET_OBJECTS:orpheus_channel_format_objects>
)

target_compile_features(orpheus_audio_utils PUBLIC cxx_std_20)

target_include_directories(orpheus_audio_utils
    PUBLIC
        $<BUILD_INTERFACE:${ORPHEUS_ROOT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${ORPHEUS_ROOT_SOURCE_DIR}/src/core
)

if(_orpheus_sndfile_target)
    target_link_libraries(orpheus_audio_utils PUBLIC ${_orpheus_sndfile_target})
endif()

if(WIN32)
    target_link_libraries(orpheus_audio_utils PUBLIC bcrypt)
elseif(NOT APPLE)
    find_package(OpenSSL REQUIRED COMPONENTS Crypto)
    target_link_libraries(orpheus_audio_utils PUBLIC OpenSSL::Crypto)
endif()

set_target_properties(orpheus_audio_utils PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(Orpheus::audio_utils ALIAS orpheus_audio_utils)
add_library(Orpheus::orpheus_audio_utils ALIAS orpheus_audio_utils)

add_library(orpheus_audio_io STATIC
    dummy_audio_driver.cpp
)

target_compile_features(orpheus_audio_io PUBLIC cxx_std_20)

target_include_directories(orpheus_audio_io
    PUBLIC
        $<BUILD_INTERFACE:${ORPHEUS_ROOT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${ORPHEUS_ROOT_SOURCE_DIR}/src/core
)

target_link_libraries(orpheus_audio_io
    PUBLIC
        orpheus_audio_utils
        orpheus_session
)

set_target_properties(orpheus_audio_io PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ORP127 T10: namespaced aliases for add_subdirectory (submodule) consumers.
add_library(Orpheus::audio_io ALIAS orpheus_audio_io)
add_library(Orpheus::orpheus_audio_io ALIAS orpheus_audio_io)

# Apply compiler warnings
include(${ORPHEUS_ROOT_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
orpheus_enable_warnings(orpheus_audio_utils)
orpheus_enable_warnings(orpheus_audio_io)
orpheus_enable_sanitizers(orpheus_audio_utils)
orpheus_enable_sanitizers(orpheus_audio_io)
