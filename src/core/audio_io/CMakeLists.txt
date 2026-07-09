# SPDX-License-Identifier: MIT

# Find libsndfile (optional, for audio file reading)
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(SNDFILE sndfile)
endif()

if(NOT SNDFILE_FOUND)
    # Try find_library as fallback
    find_library(SNDFILE_LIBRARIES NAMES sndfile libsndfile)
    find_path(SNDFILE_INCLUDE_DIRS NAMES sndfile.h)

    if(SNDFILE_LIBRARIES AND SNDFILE_INCLUDE_DIRS)
        set(SNDFILE_FOUND TRUE)
    else()
        message(WARNING "libsndfile not found. Audio file reading will not be available.")
        message(WARNING "Install with: brew install libsndfile (macOS) or vcpkg install libsndfile (Windows)")
        set(SNDFILE_FOUND FALSE)
    endif()
endif()

# Build audio utilities with available components
set(ORPHEUS_AUDIO_UTILS_SOURCES)

# ORP127 G6: host-neutral, dependency-free sample-rate conversion. Always built.
list(APPEND ORPHEUS_AUDIO_UTILS_SOURCES polyphase_resampler.cpp)

# ORP128/FTR018 §1: base-interface default (readSamplesEndingAt). Host-neutral,
# dependency-free — always built.
list(APPEND ORPHEUS_AUDIO_UTILS_SOURCES audio_file_reader.cpp)

# ORP128/FTR018 §2: variable-rate + reverse scrub resampler. Host-neutral,
# dependency-free — always built.
list(APPEND ORPHEUS_AUDIO_UTILS_SOURCES scrub_resampler.cpp)

if(SNDFILE_FOUND)
    list(APPEND ORPHEUS_AUDIO_UTILS_SOURCES audio_file_reader_libsndfile.cpp)
    list(APPEND ORPHEUS_AUDIO_UTILS_SOURCES resampling_audio_file_reader.cpp)
    list(APPEND ORPHEUS_AUDIO_UTILS_SOURCES waveform_processor.cpp)
    message(STATUS "✓ libsndfile FOUND - audio file reading enabled (with waveform processing)")
else()
    # Provide stub implementation when libsndfile is not available
    # This ensures createAudioFileReader() symbol exists for linking
    list(APPEND ORPHEUS_AUDIO_UTILS_SOURCES create_audio_file_reader_stub.cpp)
    message(STATUS "✗ libsndfile NOT FOUND - using stub implementation (audio file reading disabled)")
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

if(SNDFILE_FOUND)
    target_include_directories(orpheus_audio_utils PRIVATE ${SNDFILE_INCLUDE_DIRS})
    target_link_libraries(orpheus_audio_utils PUBLIC ${SNDFILE_LIBRARIES})
    if(SNDFILE_LIBRARY_DIRS)
        target_link_directories(orpheus_audio_utils PUBLIC ${SNDFILE_LIBRARY_DIRS})
    endif()
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
