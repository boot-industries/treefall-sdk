# OCC097 - Session Format and Loading

**Version:** 1.0
**Date:** 2025-10-30
**Status:** Reference Documentation

Complete reference for Orpheus Clip Composer session file format and loading implementation.

---

## Overview

Clip Composer sessions use JSON for human readability, version control friendliness, and cross-platform compatibility. This document provides the complete schema and implementation examples.

**See also:** OCC009 (Session Metadata Manifest), OCC022 (Clip Metadata Schema - authoritative)

---

## Session File Format (JSON)

### File Location

**Default:** `~/Documents/Orpheus Clip Composer/Sessions/`

**Extension:** `.occSession` (JSON format)

**Encoding:** UTF-8

---

### Complete Schema Example

```json
{
  "sessionMetadata": {
    "name": "Evening Broadcast",
    "version": "1.0.0",
    "createdDate": "2025-10-12T12:00:00Z",
    "modifiedDate": "2025-10-12T15:30:00Z",
    "author": "John Doe",
    "description": "Evening broadcast session for October 12th",
    "sampleRate": 48000,
    "bufferSize": 512
  },
  "clips": [
    {
      "handle": 1,
      "name": "Intro Music",
      "filePath": "/Users/username/Audio/intro.wav",
      "buttonIndex": 0,
      "tabIndex": 0,
      "clipGroup": 0,
      "trimIn": 0,
      "trimOut": 240000,
      "gain": 0.0,
      "color": "#FF5733",
      "fadeIn": 0,
      "fadeOut": 1000,
      "playbackMode": "OneShot",
      "loopEnabled": false,
      "loopStart": 0,
      "loopEnd": 240000,
      "cuePoints": [
        { "name": "Verse", "position": 48000 },
        { "name": "Chorus", "position": 96000 }
      ]
    },
    {
      "handle": 2,
      "name": "Applause SFX",
      "filePath": "/Users/username/Audio/applause.wav",
      "buttonIndex": 1,
      "tabIndex": 0,
      "clipGroup": 1,
      "trimIn": 0,
      "trimOut": 120000,
      "gain": -3.0,
      "color": "#33C1FF",
      "fadeIn": 100,
      "fadeOut": 500,
      "playbackMode": "OneShot",
      "loopEnabled": false,
      "loopStart": 0,
      "loopEnd": 120000,
      "cuePoints": []
    }
  ],
  "routing": {
    "clipGroups": [
      { "name": "Music", "gain": 0.0, "mute": false, "solo": false },
      { "name": "SFX", "gain": -3.0, "mute": false, "solo": false },
      { "name": "Voice", "gain": 0.0, "mute": false, "solo": false },
      { "name": "Backup", "gain": -6.0, "mute": false, "solo": false }
    ],
    "masterGain": 0.0,
    "masterMute": false
  },
  "preferences": {
    "autoSave": true,
    "autoSaveInterval": 300,
    "showWaveforms": true,
    "colorScheme": "Dark"
  }
}
```

---

## Field Definitions

### Session Metadata

| Field          | Type   | Required | Description                       |
| -------------- | ------ | -------- | --------------------------------- |
| `name`         | string | Yes      | Human-readable session name       |
| `version`      | string | Yes      | Schema version (semver: "1.0.0")  |
| `createdDate`  | string | Yes      | ISO 8601 timestamp                |
| `modifiedDate` | string | No       | ISO 8601 timestamp                |
| `author`       | string | No       | Creator name                      |
| `description`  | string | No       | Session description               |
| `sampleRate`   | number | Yes      | Sample rate (44100, 48000, 96000) |
| `bufferSize`   | number | No       | Audio buffer size (default: 512)  |

### Clip Metadata

| Field          | Type    | Required | Description                              |
| -------------- | ------- | -------- | ---------------------------------------- |
| `handle`       | number  | Yes      | Unique clip identifier (1-960)           |
| `name`         | string  | Yes      | Display name                             |
| `filePath`     | string  | Yes      | Absolute path to audio file              |
| `buttonIndex`  | number  | Yes      | Button position (0-119)                  |
| `tabIndex`     | number  | Yes      | Tab index (0-7)                          |
| `clipGroup`    | number  | Yes      | Routing group (0-3)                      |
| `trimIn`       | number  | Yes      | Trim start (samples)                     |
| `trimOut`      | number  | Yes      | Trim end (samples)                       |
| `gain`         | number  | Yes      | Gain in dB (-48.0 to +12.0)              |
| `color`        | string  | Yes      | Hex color code ("#RRGGBB")               |
| `fadeIn`       | number  | No       | Fade-in duration (samples, default: 0)   |
| `fadeOut`      | number  | No       | Fade-out duration (samples, default: 0)  |
| `playbackMode` | string  | No       | "OneShot" or "Loop" (default: "OneShot") |
| `loopEnabled`  | boolean | No       | Enable looping (default: false)          |
| `loopStart`    | number  | No       | Loop start (samples, default: 0)         |
| `loopEnd`      | number  | No       | Loop end (samples, default: duration)    |
| `cuePoints`    | array   | No       | Array of cue point objects               |

### Routing Configuration

| Field        | Type    | Required | Description                          |
| ------------ | ------- | -------- | ------------------------------------ |
| `clipGroups` | array   | Yes      | Array of 4 clip group objects        |
| `masterGain` | number  | No       | Master output gain dB (default: 0.0) |
| `masterMute` | boolean | No       | Master mute (default: false)         |

**Clip Group Object:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `name` | string | Yes | Group name |
| `gain` | number | Yes | Group gain dB (-48.0 to +12.0) |
| `mute` | boolean | Yes | Mute state |
| `solo` | boolean | No | Solo state (default: false) |

---

## Loading a Session (C++ Implementation)

### Complete Loading Flow

```cpp
void SessionManager::loadSession(const juce::File& sessionFile)
{
    // 1. Parse JSON (use juce::JSON or nlohmann/json)
    auto json = juce::JSON::parse(sessionFile);

    if (json.isVoid()) {
        showError("Invalid session file");
        return;
    }

    // 2. Validate schema version
    auto metadata = json["sessionMetadata"];
    auto version = metadata["version"].toString();

    if (!isCompatibleVersion(version)) {
        showError("Incompatible session version: " + version);
        return;
    }

    // 3. Stop all current playback
    transportController->stopAllClips();

    // 4. Clear existing session state
    clips.clear();
    clipGrid->clearAllButtons();

    // 5. Load session metadata
    sessionName = metadata["name"].toString();
    sampleRate = static_cast<int>(metadata["sampleRate"]);
    createdDate = metadata["createdDate"].toString();

    // 6. Load clips sequentially (message thread, I/O is OK)
    auto clipsArray = json["clips"];
    for (int i = 0; i < clipsArray.size(); ++i) {
        auto clipJson = clipsArray[i];
        if (!loadClipFromJson(clipJson)) {
            showWarning("Failed to load clip: " + clipJson["name"].toString());
            // Continue loading other clips
        }
    }

    // 7. Restore routing configuration
    auto routingJson = json["routing"];
    restoreRoutingFromJson(routingJson);

    // 8. Apply preferences
    auto preferencesJson = json["preferences"];
    applyPreferences(preferencesJson);

    // 9. Update UI
    clipGrid->refresh();
    routingPanel->refresh();

    // 10. Mark session as loaded
    currentSessionFile = sessionFile;
    isDirty = false;
}
```

---

### Loading Individual Clips

```cpp
bool SessionManager::loadClipFromJson(const juce::var& clipJson)
{
    // Parse clip metadata
    ClipMetadata clip;
    clip.handle = static_cast<int>(clipJson["handle"]);
    clip.name = clipJson["name"].toString().toStdString();
    clip.filePath = clipJson["filePath"].toString().toStdString();
    clip.buttonIndex = static_cast<int>(clipJson["buttonIndex"]);
    clip.tabIndex = static_cast<int>(clipJson["tabIndex"]);
    clip.clipGroup = static_cast<uint8_t>(clipJson["clipGroup"]);
    clip.trimIn = static_cast<int64_t>(clipJson["trimIn"]);
    clip.trimOut = static_cast<int64_t>(clipJson["trimOut"]);
    clip.gain = static_cast<float>(clipJson["gain"]);
    clip.color = clipJson["color"].toString().toStdString();

    // Optional fields (with defaults)
    clip.fadeIn = clipJson.hasProperty("fadeIn") ? static_cast<int64_t>(clipJson["fadeIn"]) : 0;
    clip.fadeOut = clipJson.hasProperty("fadeOut") ? static_cast<int64_t>(clipJson["fadeOut"]) : 0;
    clip.playbackMode = clipJson.hasProperty("playbackMode") ? clipJson["playbackMode"].toString().toStdString() : "OneShot";
    clip.loopEnabled = clipJson.hasProperty("loopEnabled") ? static_cast<bool>(clipJson["loopEnabled"]) : false;

    // Validate file path
    juce::File audioFile(clip.filePath);
    if (!audioFile.existsAsFile()) {
        showError("Audio file not found: " + clip.filePath);
        return false;
    }

    // Load audio file via SDK
    auto reader = orpheus::createAudioFileReader();
    auto result = reader->open(clip.filePath);

    if (!result.isOk()) {
        showError("Failed to load audio file: " + result.errorMessage);
        return false;
    }

    // Store metadata
    clip.sampleRate = result.value.sample_rate;
    clip.numChannels = result.value.num_channels;
    clip.durationSamples = result.value.duration_samples;
    clip.format = result.value.format;

    // Add to session
    clips.push_back(clip);

    // Pre-render waveform on background thread
    renderWaveformAsync(reader, clip);

    // Update UI button
    clipGrid->setButtonClip(clip.tabIndex, clip.buttonIndex, clip);

    return true;
}
```

---

### Restoring Routing Configuration

```cpp
void SessionManager::restoreRoutingFromJson(const juce::var& routingJson)
{
    // Restore clip groups
    auto groupsArray = routingJson["clipGroups"];
    for (int i = 0; i < groupsArray.size() && i < 4; ++i) {
        auto groupJson = groupsArray[i];

        std::string name = groupJson["name"].toString().toStdString();
        float gain = static_cast<float>(groupJson["gain"]);
        bool mute = static_cast<bool>(groupJson["mute"]);
        bool solo = groupJson.hasProperty("solo") ? static_cast<bool>(groupJson["solo"]) : false;

        // Apply to routing matrix (lock-free SDK call)
        routingMatrix->setGroupName(i, name);
        routingMatrix->setGroupGain(i, gain);
        routingMatrix->setGroupMute(i, mute);
        routingMatrix->setGroupSolo(i, solo);
    }

    // Restore master settings
    if (routingJson.hasProperty("masterGain")) {
        float masterGain = static_cast<float>(routingJson["masterGain"]);
        routingMatrix->setMasterGain(masterGain);
    }

    if (routingJson.hasProperty("masterMute")) {
        bool masterMute = static_cast<bool>(routingJson["masterMute"]);
        routingMatrix->setMasterMute(masterMute);
    }
}
```

---

## Saving a Session (C++ Implementation)

### Complete Save Flow

```cpp
void SessionManager::saveSession(const juce::File& sessionFile)
{
    // Build JSON from current state (on message thread, I/O is OK)
    juce::DynamicObject::Ptr root = new juce::DynamicObject();

    // 1. Session metadata
    auto metadataObj = new juce::DynamicObject();
    metadataObj->setProperty("name", sessionName);
    metadataObj->setProperty("version", "1.0.0");
    metadataObj->setProperty("createdDate", createdDate);
    metadataObj->setProperty("modifiedDate", getCurrentTimestamp());
    metadataObj->setProperty("author", authorName);
    metadataObj->setProperty("description", description);
    metadataObj->setProperty("sampleRate", sampleRate);
    metadataObj->setProperty("bufferSize", bufferSize);
    root->setProperty("sessionMetadata", metadataObj);

    // 2. Clips array
    juce::Array<juce::var> clipsArray;
    for (const auto& clip : clips) {
        auto clipObj = new juce::DynamicObject();
        clipObj->setProperty("handle", clip.handle);
        clipObj->setProperty("name", clip.name);
        clipObj->setProperty("filePath", clip.filePath);
        clipObj->setProperty("buttonIndex", clip.buttonIndex);
        clipObj->setProperty("tabIndex", clip.tabIndex);
        clipObj->setProperty("clipGroup", clip.clipGroup);
        clipObj->setProperty("trimIn", clip.trimIn);
        clipObj->setProperty("trimOut", clip.trimOut);
        clipObj->setProperty("gain", clip.gain);
        clipObj->setProperty("color", clip.color);
        clipObj->setProperty("fadeIn", clip.fadeIn);
        clipObj->setProperty("fadeOut", clip.fadeOut);
        clipObj->setProperty("playbackMode", clip.playbackMode);
        clipObj->setProperty("loopEnabled", clip.loopEnabled);
        clipObj->setProperty("loopStart", clip.loopStart);
        clipObj->setProperty("loopEnd", clip.loopEnd);

        // Cue points (if any)
        if (!clip.cuePoints.empty()) {
            juce::Array<juce::var> cuePointsArray;
            for (const auto& cue : clip.cuePoints) {
                auto cueObj = new juce::DynamicObject();
                cueObj->setProperty("name", cue.name);
                cueObj->setProperty("position", cue.position);
                cuePointsArray.add(cueObj);
            }
            clipObj->setProperty("cuePoints", cuePointsArray);
        }

        clipsArray.add(clipObj);
    }
    root->setProperty("clips", clipsArray);

    // 3. Routing configuration
    auto routingObj = new juce::DynamicObject();

    juce::Array<juce::var> groupsArray;
    for (int i = 0; i < 4; ++i) {
        auto groupObj = new juce::DynamicObject();
        groupObj->setProperty("name", routingMatrix->getGroupName(i));
        groupObj->setProperty("gain", routingMatrix->getGroupGain(i));
        groupObj->setProperty("mute", routingMatrix->getGroupMute(i));
        groupObj->setProperty("solo", routingMatrix->getGroupSolo(i));
        groupsArray.add(groupObj);
    }
    routingObj->setProperty("clipGroups", groupsArray);
    routingObj->setProperty("masterGain", routingMatrix->getMasterGain());
    routingObj->setProperty("masterMute", routingMatrix->getMasterMute());

    root->setProperty("routing", routingObj);

    // 4. Preferences
    auto preferencesObj = new juce::DynamicObject();
    preferencesObj->setProperty("autoSave", preferences.autoSave);
    preferencesObj->setProperty("autoSaveInterval", preferences.autoSaveInterval);
    preferencesObj->setProperty("showWaveforms", preferences.showWaveforms);
    preferencesObj->setProperty("colorScheme", preferences.colorScheme);
    root->setProperty("preferences", preferencesObj);

    // 5. Write to file (blocking I/O is OK on message thread)
    juce::var jsonVar(root.get());
    auto jsonString = juce::JSON::toString(jsonVar, true);  // pretty-print

    if (!sessionFile.replaceWithText(jsonString)) {
        showError("Failed to save session file");
        return;
    }

    // 6. Update state
    currentSessionFile = sessionFile;
    isDirty = false;

    showNotification("Session saved: " + sessionFile.getFileName());
}
```

---

## Version Migration

### Schema Version History

| Version | Date       | Changes        |
| ------- | ---------- | -------------- |
| 1.0.0   | 2025-10-12 | Initial schema |

### Future Migration Strategy

When schema changes occur:

1. Increment version in `sessionMetadata.version`
2. Implement migration function in `SessionManager::migrateSession()`
3. Always support at least 2 previous versions
4. Prompt user before migrating (non-destructive)

**Example migration (future):**

```cpp
bool SessionManager::migrateSession(juce::var& json, const juce::String& fromVersion)
{
    if (fromVersion == "1.0.0" && currentVersion == "2.0.0") {
        // Add new fields with defaults
        auto clipsArray = json["clips"];
        for (int i = 0; i < clipsArray.size(); ++i) {
            auto clip = clipsArray[i].getDynamicObject();
            if (!clip->hasProperty("newField")) {
                clip->setProperty("newField", defaultValue);
            }
        }

        // Update version
        json["sessionMetadata"].getDynamicObject()->setProperty("version", "2.0.0");
        return true;
    }

    return false;  // Unsupported migration
}
```

---

## Error Handling

### Common Load Errors

| Error                     | Cause              | Recovery                           |
| ------------------------- | ------------------ | ---------------------------------- |
| "Invalid JSON"            | Malformed file     | Prompt user to restore from backup |
| "Incompatible version"    | Future schema      | Prompt user to update OCC          |
| "Audio file not found"    | Moved/deleted file | Prompt user to locate file         |
| "Unsupported sample rate" | SDK limitation     | Offer to resample (future)         |

### Auto-Save and Backups

**Auto-save:**

- Every 5 minutes (configurable)
- Saves to `.occSession.autosave` (hidden file)
- Restored on crash recovery

**Backups:**

- Every manual save creates `.occSession.backup`
- Keep last 5 backups (FIFO)
- User can restore via "File → Restore Backup..."

---

## Related Documentation

- **OCC009** - Session Metadata Manifest (original spec)
- **OCC022** - Clip Metadata Schema (authoritative, most detailed)
- **OCC096** - SDK Integration Patterns (loading/saving code patterns)
- **OCC027** - API Contracts (SDK interface definitions)

---

**Last Updated:** 2025-10-30
**Maintainer:** OCC Development Team
**Status:** Reference Documentation
