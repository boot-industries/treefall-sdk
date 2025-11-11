// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <memory>
#include <orpheus/transport_controller.h> // For ClipHandle, SessionGraphError
#include <string>
#include <vector>

namespace orpheus {

// Forward declarations
namespace core {
class SessionGraph;
} // namespace core

class IRoutingMatrix;

// ============================================================================
// Scene Snapshot Structure
// ============================================================================

/// Lightweight scene snapshot (metadata only, no audio data)
///
/// A scene represents the complete state of button assignments, routing,
/// and group configurations at a specific point in time. This is useful
/// for theater/broadcast workflows where users need to quickly recall
/// different show configurations.
///
/// @note Does NOT store audio file data, only clip handles and routing metadata
struct SceneSnapshot {
  std::string sceneId; ///< Unique identifier (UUID)
  std::string name;    ///< User-friendly name (e.g., "Act 1", "Intro Music")
  uint64_t timestamp;  ///< Creation time (Unix epoch in seconds)

  // Clip assignments (which clips loaded on which buttons)
  std::vector<ClipHandle> assignedClips; ///< Clip handles per button/position

  // Routing configuration (from Feature 1: Routing Matrix)
  std::vector<uint8_t> clipGroups; ///< Group assignment per clip (0-3, or 255 for unassigned)
  std::vector<float> groupGains;   ///< Gain per Clip Group in dB (-inf to +12.0)

  /// Default constructor
  SceneSnapshot() : sceneId(""), name(""), timestamp(0) {}
};

// ============================================================================
// Scene Manager Interface
// ============================================================================

/// Scene manager interface for preset workflows
///
/// This interface provides functionality to save and recall complete session
/// states (button assignments, routing, group gains) for quick workflow
/// switching in theater, broadcast, and live production environments.
///
/// Key Features:
/// - Lightweight snapshots (metadata only, no audio file copying)
/// - UUID-based scene identification (timestamp + counter)
/// - JSON export/import for portability and backup
/// - In-memory storage with optional disk persistence
/// - State restoration without audio file reloading
///
/// Thread Safety:
/// - captureScene(), recallScene(), deleteScene(): UI thread only (mutex protected)
/// - listScenes(), exportScene(), importScene(): UI thread only
///
/// Typical Usage:
/// @code
///   auto sceneManager = createSceneManager(sessionGraph);
///
///   // Capture current state
///   std::string sceneId = sceneManager->captureScene("Act 1 - Opening");
///
///   // Later, recall the scene
///   auto result = sceneManager->recallScene(sceneId);
///   if (result == SessionGraphError::OK) {
///     // Scene restored successfully
///   }
///
///   // Export to file for backup
///   sceneManager->exportScene(sceneId, "/path/to/scenes/act1_opening.json");
/// @endcode
class ISceneManager {
public:
  virtual ~ISceneManager() = default;

  // ========================================================================
  // Scene Capture & Recall (UI Thread)
  // ========================================================================

  /// Capture current session state as a scene
  ///
  /// This method creates a lightweight snapshot of the current session state,
  /// including:
  /// - Clip assignments (which clips are loaded)
  /// - Routing configuration (clip-to-group assignments)
  /// - Group gains (per-group gain settings)
  ///
  /// @param name User-friendly scene name (e.g., "Show Opening", "Act 2")
  /// @return Scene ID (UUID) for later recall, or empty string on error
  ///
  /// @note This does NOT copy audio files, only metadata
  /// @note Scene ID format: "scene-{timestamp}-{counter}" (e.g., "scene-1699564800-001")
  virtual std::string captureScene(const std::string& name) = 0;

  /// Recall scene (restore button states and routing)
  ///
  /// This method restores the session state from a previously captured scene:
  /// 1. Stops all playback
  /// 2. Reconfigures clip-to-group assignments
  /// 3. Restores group gains
  /// 4. Does NOT reload audio files (assumes clips already loaded)
  ///
  /// @param sceneId Scene identifier from captureScene()
  /// @return SessionGraphError::OK on success, error code on failure
  ///
  /// @note If audio files are not loaded, routing will be configured but clips won't play
  /// @note This is safe to call during playback (will stop all clips first)
  virtual SessionGraphError recallScene(const std::string& sceneId) = 0;

  // ========================================================================
  // Scene Management (UI Thread)
  // ========================================================================

  /// List all saved scenes
  ///
  /// @return Vector of scene snapshots (ordered by timestamp, newest first)
  ///
  /// @note This returns copies of scene metadata, safe to iterate and query
  virtual std::vector<SceneSnapshot> listScenes() const = 0;

  /// Delete scene
  ///
  /// Removes a scene from the in-memory storage. This operation is permanent
  /// unless the scene was exported to a file.
  ///
  /// @param sceneId Scene identifier
  /// @return SessionGraphError::OK on success, InvalidHandle if scene not found
  virtual SessionGraphError deleteScene(const std::string& sceneId) = 0;

  // ========================================================================
  // Scene Import/Export (UI Thread)
  // ========================================================================

  /// Export scene to JSON file
  ///
  /// Serializes a scene to a portable JSON format for backup or sharing.
  /// The JSON file contains all scene metadata (name, timestamp, clip assignments,
  /// routing configuration, group gains) but NO audio file data.
  ///
  /// @param sceneId Scene identifier
  /// @param filePath Output file path (e.g., "/path/to/scenes/my_scene.json")
  /// @return SessionGraphError::OK on success, InvalidHandle if scene not found
  ///
  /// @note File is overwritten if it exists
  /// @note JSON format is human-readable and can be edited manually
  virtual SessionGraphError exportScene(const std::string& sceneId,
                                        const std::string& filePath) = 0;

  /// Import scene from JSON file
  ///
  /// Deserializes a scene from a JSON file and adds it to the in-memory
  /// storage. The scene is assigned a new UUID (based on import time).
  ///
  /// @param filePath Input file path (e.g., "/path/to/scenes/my_scene.json")
  /// @return Scene ID of imported scene, or empty string on error
  ///
  /// @note Original scene ID is NOT preserved (new UUID generated)
  /// @note If file is invalid JSON, returns empty string
  virtual std::string importScene(const std::string& filePath) = 0;

  // ========================================================================
  // Utility Methods (UI Thread)
  // ========================================================================

  /// Get scene by ID (query only)
  ///
  /// @param sceneId Scene identifier
  /// @return Scene snapshot if found, nullptr if not found
  ///
  /// @note Returns pointer to internal storage, do NOT modify
  virtual const SceneSnapshot* getScene(const std::string& sceneId) const = 0;

  /// Check if scene exists
  ///
  /// @param sceneId Scene identifier
  /// @return true if scene exists, false otherwise
  virtual bool hasScene(const std::string& sceneId) const = 0;

  /// Clear all scenes
  ///
  /// Removes all scenes from in-memory storage. This operation is permanent
  /// unless scenes were exported to files.
  ///
  /// @return SessionGraphError::OK on success
  virtual SessionGraphError clearAllScenes() = 0;
};

// ============================================================================
// Factory Function
// ============================================================================

/// Create scene manager instance
///
/// @param sessionGraph The session graph containing clip metadata
/// @return Unique pointer to scene manager
///
/// @note The scene manager does NOT take ownership of sessionGraph
/// @note Caller must ensure sessionGraph outlives the scene manager
std::unique_ptr<ISceneManager> createSceneManager(core::SessionGraph* sessionGraph);

} // namespace orpheus
