# SPDX-License-Identifier: MIT
#
# occ-app-platform: Shared application infrastructure for Orpheus JUCE apps.
# Provides: ServiceContext (DI), UndoManager, Database (SQLite),
#           EventLogger, PlayoutLogger, DisplayPreferences,
#           ExternalToolManager, ApplicationPaths, Command interface.
#
# Dependencies: JUCE (juce_core, juce_events, juce_data_structures), SQLite3
# Consumers: Clip Composer, Wave Finder, FX Engine (future)

cmake_minimum_required(VERSION 3.22)

# Source files
set(OCC_APP_PLATFORM_SOURCES
  src/UndoManager.cpp
  src/ApplicationPaths.cpp
  src/Database.cpp
  src/DisplayPreferences.cpp
  src/ExternalToolManager.cpp
  src/EventLogger.cpp
  src/PlayoutLogger.cpp
)

add_library(occ_app_platform STATIC ${OCC_APP_PLATFORM_SOURCES})

target_include_directories(occ_app_platform
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(occ_app_platform PRIVATE cxx_std_20)

# JUCE modules (must be available from parent project)
target_link_libraries(occ_app_platform
  PUBLIC
    juce::juce_core
    juce::juce_events
    juce::juce_data_structures
    sqlite3
)
