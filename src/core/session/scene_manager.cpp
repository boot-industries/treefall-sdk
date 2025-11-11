// SPDX-License-Identifier: MIT
#include "orpheus/scene_manager.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>

#include "orpheus/json.hpp"
#include "orpheus/routing_matrix.h"
#include "session_graph.h"

namespace orpheus {

namespace {

// UUID generation (simple timestamp-based approach)
std::string generateSceneId() {
  static std::atomic<uint32_t> counter{0};

  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::system_clock::to_time_t(now);
  uint32_t id = counter.fetch_add(1, std::memory_order_relaxed);

  std::ostringstream oss;
  oss << "scene-" << timestamp << "-" << std::setfill('0') << std::setw(3) << (id % 1000);
  return oss.str();
}

// JSON serialization helpers
json::JsonValue serializeSceneToJson(const SceneSnapshot& scene) {
  json::JsonValue root;
  root.type = json::JsonValue::Type::kObject;

  // Scene metadata
  root.object["sceneId"] = json::JsonValue{};
  root.object["sceneId"].type = json::JsonValue::Type::kString;
  root.object["sceneId"].string = scene.sceneId;

  root.object["name"] = json::JsonValue{};
  root.object["name"].type = json::JsonValue::Type::kString;
  root.object["name"].string = scene.name;

  root.object["timestamp"] = json::JsonValue{};
  root.object["timestamp"].type = json::JsonValue::Type::kNumber;
  root.object["timestamp"].number = static_cast<double>(scene.timestamp);

  // Assigned clips (array of clip handles)
  root.object["assignedClips"] = json::JsonValue{};
  root.object["assignedClips"].type = json::JsonValue::Type::kArray;
  for (ClipHandle handle : scene.assignedClips) {
    json::JsonValue clip;
    clip.type = json::JsonValue::Type::kNumber;
    clip.number = static_cast<double>(handle);
    root.object["assignedClips"].array.push_back(clip);
  }

  // Clip groups (array of uint8_t)
  root.object["clipGroups"] = json::JsonValue{};
  root.object["clipGroups"].type = json::JsonValue::Type::kArray;
  for (uint8_t group : scene.clipGroups) {
    json::JsonValue g;
    g.type = json::JsonValue::Type::kNumber;
    g.number = static_cast<double>(group);
    root.object["clipGroups"].array.push_back(g);
  }

  // Group gains (array of float)
  root.object["groupGains"] = json::JsonValue{};
  root.object["groupGains"].type = json::JsonValue::Type::kArray;
  for (float gain : scene.groupGains) {
    json::JsonValue g;
    g.type = json::JsonValue::Type::kNumber;
    g.number = static_cast<double>(gain);
    root.object["groupGains"].array.push_back(g);
  }

  return root;
}

// JSON writing helper (manual JSON generation for simplicity)
void writeJsonToFile(const json::JsonValue& value, const std::string& filePath) {
  std::ostringstream oss;

  std::function<void(const json::JsonValue&, size_t)> writeValue = [&](const json::JsonValue& v,
                                                                       size_t indent) {
    switch (v.type) {
    case json::JsonValue::Type::kNull:
      oss << "null";
      break;
    case json::JsonValue::Type::kBoolean:
      oss << (v.boolean ? "true" : "false");
      break;
    case json::JsonValue::Type::kNumber:
      oss << json::FormatDouble(v.number);
      break;
    case json::JsonValue::Type::kString:
      oss << "\"" << json::EscapeString(v.string) << "\"";
      break;
    case json::JsonValue::Type::kArray: {
      oss << "[";
      for (size_t i = 0; i < v.array.size(); ++i) {
        if (i > 0)
          oss << ", ";
        writeValue(v.array[i], indent);
      }
      oss << "]";
      break;
    }
    case json::JsonValue::Type::kObject: {
      oss << "{\n";
      size_t count = 0;
      for (const auto& [key, val] : v.object) {
        if (count++ > 0)
          oss << ",\n";
        json::WriteIndent(oss, indent + 1);
        oss << "\"" << json::EscapeString(key) << "\": ";
        writeValue(val, indent + 1);
      }
      oss << "\n";
      json::WriteIndent(oss, indent);
      oss << "}";
      break;
    }
    }
  };

  writeValue(value, 0U);

  // Write to file
  std::ofstream file(filePath);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file for writing: " + filePath);
  }
  file << oss.str();
  file.close();
}

// JSON deserialization helper
SceneSnapshot deserializeSceneFromJson(const json::JsonValue& root) {
  SceneSnapshot scene;

  // Parse scene metadata
  if (auto* sceneId = json::RequireField(root, "sceneId")) {
    scene.sceneId = json::RequireString(*sceneId, "sceneId");
  }

  if (auto* name = json::RequireField(root, "name")) {
    scene.name = json::RequireString(*name, "name");
  }

  if (auto* timestamp = json::RequireField(root, "timestamp")) {
    scene.timestamp = static_cast<uint64_t>(json::RequireNumber(*timestamp, "timestamp"));
  }

  // Parse assigned clips
  if (auto* assignedClips = json::RequireField(root, "assignedClips")) {
    const auto& arr = json::ExpectArray(*assignedClips, "assignedClips");
    for (const auto& item : arr.array) {
      ClipHandle handle = static_cast<ClipHandle>(item.number);
      scene.assignedClips.push_back(handle);
    }
  }

  // Parse clip groups
  if (auto* clipGroups = json::RequireField(root, "clipGroups")) {
    const auto& arr = json::ExpectArray(*clipGroups, "clipGroups");
    for (const auto& item : arr.array) {
      uint8_t group = static_cast<uint8_t>(item.number);
      scene.clipGroups.push_back(group);
    }
  }

  // Parse group gains
  if (auto* groupGains = json::RequireField(root, "groupGains")) {
    const auto& arr = json::ExpectArray(*groupGains, "groupGains");
    for (const auto& item : arr.array) {
      float gain = static_cast<float>(item.number);
      scene.groupGains.push_back(gain);
    }
  }

  return scene;
}

} // anonymous namespace

// ============================================================================
// Scene Manager Implementation
// ============================================================================

class SceneManager : public ISceneManager {
public:
  explicit SceneManager(core::SessionGraph* sessionGraph)
      : sessionGraph_(sessionGraph), routingMatrix_(nullptr) {
    if (!sessionGraph) {
      throw std::invalid_argument("SessionGraph cannot be null");
    }
  }

  ~SceneManager() override = default;

  // Set routing matrix (optional, used for capturing/restoring routing state)
  void setRoutingMatrix(IRoutingMatrix* routingMatrix) {
    std::lock_guard<std::mutex> lock(mutex_);
    routingMatrix_ = routingMatrix;
  }

  // ========================================================================
  // Scene Capture & Recall
  // ========================================================================

  std::string captureScene(const std::string& name) override {
    std::lock_guard<std::mutex> lock(mutex_);

    SceneSnapshot scene;
    scene.sceneId = generateSceneId();
    scene.name = name;

    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    scene.timestamp = static_cast<uint64_t>(std::chrono::system_clock::to_time_t(now));

    // TODO: Capture clip assignments from SessionGraph
    // For now, we store empty vectors (OCC integration will populate this)
    // In a full implementation, this would iterate over registered clips
    // and store their handles in order

    // Capture routing state (if routing matrix is available)
    if (routingMatrix_) {
      auto config = routingMatrix_->getConfig();

      // Reserve space for clip groups (one per channel)
      scene.clipGroups.resize(config.num_channels, 255); // 255 = unassigned

      // Reserve space for group gains (one per group)
      scene.groupGains.resize(config.num_groups, 0.0f);

      // Note: In a full implementation, we would query the routing matrix
      // for each channel's group assignment and each group's gain.
      // Since IRoutingMatrix doesn't expose getChannelGroup() and getGroupGain(),
      // we'll use snapshots instead (Feature 1 routing snapshot API)
      auto snapshot = routingMatrix_->saveSnapshot(name);
      for (size_t i = 0; i < snapshot.channels.size(); ++i) {
        if (i < scene.clipGroups.size()) {
          scene.clipGroups[i] = snapshot.channels[i].group_index;
        }
      }
      for (size_t i = 0; i < snapshot.groups.size(); ++i) {
        if (i < scene.groupGains.size()) {
          scene.groupGains[i] = snapshot.groups[i].gain_db;
        }
      }
    }

    // Store scene in memory
    scenes_[scene.sceneId] = scene;

    return scene.sceneId;
  }

  SessionGraphError recallScene(const std::string& sceneId) override {
    std::lock_guard<std::mutex> lock(mutex_);

    // Find scene
    auto it = scenes_.find(sceneId);
    if (it == scenes_.end()) {
      return SessionGraphError::InvalidHandle;
    }

    const SceneSnapshot& scene = it->second;

    // TODO: Stop all playback (requires ITransportController reference)
    // For now, we assume the caller handles stopping playback

    // Restore routing state (if routing matrix is available)
    if (routingMatrix_) {
      // Build routing snapshot
      RoutingSnapshot snapshot;
      snapshot.name = scene.name;
      snapshot.timestamp_ms = static_cast<uint32_t>(scene.timestamp);

      // Restore channels
      auto config = routingMatrix_->getConfig();
      snapshot.channels.resize(config.num_channels);
      for (size_t i = 0; i < scene.clipGroups.size() && i < snapshot.channels.size(); ++i) {
        snapshot.channels[i].group_index = scene.clipGroups[i];
        // Other channel properties (gain, pan, mute, solo) use defaults
      }

      // Restore groups
      snapshot.groups.resize(config.num_groups);
      for (size_t i = 0; i < scene.groupGains.size() && i < snapshot.groups.size(); ++i) {
        snapshot.groups[i].gain_db = scene.groupGains[i];
        // Other group properties use defaults
      }

      // Load snapshot into routing matrix
      auto result = routingMatrix_->loadSnapshot(snapshot);
      if (result != SessionGraphError::OK) {
        return result;
      }
    }

    // TODO: Restore clip assignments (requires SessionGraph API extension)
    // For now, we only restore routing state

    return SessionGraphError::OK;
  }

  // ========================================================================
  // Scene Management
  // ========================================================================

  std::vector<SceneSnapshot> listScenes() const override {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<SceneSnapshot> scenes;
    scenes.reserve(scenes_.size());

    for (const auto& [id, scene] : scenes_) {
      scenes.push_back(scene);
    }

    // Sort by timestamp (newest first)
    std::sort(scenes.begin(), scenes.end(), [](const SceneSnapshot& a, const SceneSnapshot& b) {
      return a.timestamp > b.timestamp;
    });

    return scenes;
  }

  SessionGraphError deleteScene(const std::string& sceneId) override {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = scenes_.find(sceneId);
    if (it == scenes_.end()) {
      return SessionGraphError::InvalidHandle;
    }

    scenes_.erase(it);
    return SessionGraphError::OK;
  }

  // ========================================================================
  // Scene Import/Export
  // ========================================================================

  SessionGraphError exportScene(const std::string& sceneId, const std::string& filePath) override {
    std::lock_guard<std::mutex> lock(mutex_);

    // Find scene
    auto it = scenes_.find(sceneId);
    if (it == scenes_.end()) {
      return SessionGraphError::InvalidHandle;
    }

    const SceneSnapshot& scene = it->second;

    try {
      // Serialize to JSON
      auto jsonValue = serializeSceneToJson(scene);

      // Write to file
      writeJsonToFile(jsonValue, filePath);

      return SessionGraphError::OK;
    } catch (const std::exception&) {
      // File I/O error
      return SessionGraphError::InternalError;
    }
  }

  std::string importScene(const std::string& filePath) override {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
      // Read file contents
      std::ifstream file(filePath);
      if (!file.is_open()) {
        return "";
      }

      std::ostringstream oss;
      oss << file.rdbuf();
      std::string jsonStr = oss.str();
      file.close();

      // Parse JSON
      json::JsonParser parser(jsonStr);
      auto jsonValue = parser.Parse();

      // Deserialize scene
      SceneSnapshot scene = deserializeSceneFromJson(jsonValue);

      // Generate new UUID (don't preserve original ID)
      scene.sceneId = generateSceneId();

      // Update timestamp to import time
      auto now = std::chrono::system_clock::now();
      scene.timestamp = static_cast<uint64_t>(std::chrono::system_clock::to_time_t(now));

      // Store scene
      scenes_[scene.sceneId] = scene;

      return scene.sceneId;
    } catch (const std::exception&) {
      // JSON parse error or file I/O error
      return "";
    }
  }

  // ========================================================================
  // Utility Methods
  // ========================================================================

  const SceneSnapshot* getScene(const std::string& sceneId) const override {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = scenes_.find(sceneId);
    if (it == scenes_.end()) {
      return nullptr;
    }

    return &it->second;
  }

  bool hasScene(const std::string& sceneId) const override {
    std::lock_guard<std::mutex> lock(mutex_);
    return scenes_.find(sceneId) != scenes_.end();
  }

  SessionGraphError clearAllScenes() override {
    std::lock_guard<std::mutex> lock(mutex_);
    scenes_.clear();
    return SessionGraphError::OK;
  }

private:
  [[maybe_unused]] core::SessionGraph* sessionGraph_; // Not owned
  IRoutingMatrix* routingMatrix_;                     // Not owned (optional)
  mutable std::mutex mutex_;                          // Protects scenes_
  std::map<std::string, SceneSnapshot> scenes_;       // In-memory storage
};

// ============================================================================
// Factory Function
// ============================================================================

std::unique_ptr<ISceneManager> createSceneManager(core::SessionGraph* sessionGraph) {
  return std::make_unique<SceneManager>(sessionGraph);
}

} // namespace orpheus
