/*
  ==============================================================================

    ServiceContext.h
    Created: 27 Nov 2025
    Author:  Orpheus Clip Composer

  ==============================================================================
*/

#pragma once

// Forward declarations for existing global classes
class SessionManager;
class AudioEngine;

// Forward declarations for future orpheus namespace classes
namespace orpheus {
class EventLogger;
class PlayoutLogger;
class SettingsService;
class UndoManager;
} // namespace orpheus

namespace orpheus {

/**
    Dependency injection container for application-wide services.
    Passed to components that need access to backend infrastructure.

    Lifecycle is managed by MainComponent.
    Pointers are guaranteed to be valid during the application lifetime
    (after initialization in MainComponent).
*/
struct ServiceContext {
  // Core Systems
  SessionManager* sessionManager = nullptr;
  AudioEngine* audioEngine = nullptr;

  // Infrastructure (Sprint 0-2)
  EventLogger* eventLogger = nullptr;
  PlayoutLogger* playoutLogger = nullptr;
  SettingsService* settingsService = nullptr;
  UndoManager* undoManager = nullptr;

  // Helper to ensure minimal viable services are present
  bool isValid() const {
    return sessionManager != nullptr && audioEngine != nullptr;
  }
};

} // namespace orpheus
