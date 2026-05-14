// SPDX-License-Identifier: MIT

#include "SessionManager.h"
#include <algorithm> // For std::transform
#include <cctype>    // For std::isupper, std::isspace, std::isalpha, std::toupper
#include <juce_audio_formats/juce_audio_formats.h>

namespace {

constexpr const char* kSessionVersion = "0.3.0";
constexpr const char* kSessionJsonName = "session.json";
constexpr const char* kManifestJsonName = "manifest.json";
constexpr const char* kMediaFolderName = "media";

juce::String makeIsoUtcTimestamp() {
  auto now = juce::Time::getCurrentTime();
  auto utcMillis =
      now.toMilliseconds() - (static_cast<juce::int64>(now.getUTCOffsetSeconds()) * 1000);
  return juce::Time(utcMillis).formatted("%Y-%m-%dT%H:%M:%SZ");
}

juce::String makeUuidString() {
  return juce::Uuid().toString().trim();
}

juce::String sanitizeFileName(juce::String fileName) {
  fileName = fileName.replaceCharacter('/', '_').replaceCharacter('\\', '_');
  return fileName;
}

} // namespace

//==============================================================================
SessionManager::SessionManager() {
  // Initialize default tab labels
  for (int i = 0; i < NUM_TABS; ++i) {
    m_tabLabels[i] = (juce::String("Tab ") + juce::String(i + 1)).toStdString();
  }
  m_sessionLineage.sessionId = makeUuidString().toStdString();
  m_sessionLineage.createdAtUtc = makeIsoUtcTimestamp().toStdString();
  m_sessionLineage.updatedAtUtc = m_sessionLineage.createdAtUtc;
}

//==============================================================================
void SessionManager::setActiveTab(int tabIndex) {
  if (tabIndex >= 0 && tabIndex < NUM_TABS) {
    m_currentTab = tabIndex;
    DBG("SessionManager: Active tab set to " << tabIndex);
  }
}

std::string SessionManager::getTabLabel(int tabIndex) const {
  if (tabIndex >= 0 && tabIndex < NUM_TABS)
    return m_tabLabels[tabIndex];
  return "";
}

void SessionManager::setTabLabel(int tabIndex, const std::string& label) {
  if (tabIndex >= 0 && tabIndex < NUM_TABS) {
    m_tabLabels[tabIndex] = label;
    m_isDirty = true;
    DBG("SessionManager: Tab " << tabIndex << " label set to: " << label);
  }
}

//==============================================================================
bool SessionManager::loadClip(int buttonIndex, const juce::String& filePath) {
  return loadClipForTab(buttonIndex, filePath, m_currentTab);
}

bool SessionManager::loadClipForTab(int buttonIndex, const juce::String& filePath, int tabIndex,
                                    bool allowMissingMedia) {
  if (tabIndex < 0 || tabIndex >= NUM_TABS) {
    DBG("SessionManager: Invalid tab index " << tabIndex << " for loadClip");
    return false;
  }
  if (buttonIndex < 0 || buttonIndex >= BUTTONS_PER_TAB) {
    DBG("SessionManager: Invalid button index " << buttonIndex << " for loadClip");
    return false;
  }

  juce::File audioFile(filePath);

  // Validate file exists
  if (!audioFile.existsAsFile() && !allowMissingMedia) {
    DBG("SessionManager: File not found: " << filePath);
    return false;
  }

  // Extract metadata
  auto clipData = extractMetadata(filePath, allowMissingMedia);

  if (!clipData.isValid()) {
    DBG("SessionManager: Failed to read metadata from: " << filePath);
    return false;
  }

  // Set tab index
  clipData.tabIndex = tabIndex;

  // Store in map with composite key (tab, button)
  int key = makeKey(tabIndex, buttonIndex);
  m_clips[key] = clipData;
  m_isDirty = true;

  if (clipData.mediaAvailable) {
    DBG("SessionManager: Loaded clip " << clipData.displayName << " onto tab " << tabIndex
                                       << ", button " << buttonIndex << " (" << clipData.sampleRate
                                       << " Hz, " << clipData.numChannels << " ch, "
                                       << clipData.durationSamples << " samples)");
  } else {
    recordMissingMedia(tabIndex, buttonIndex, filePath, "Media file is unavailable");
    DBG("SessionManager: Loaded unresolved clip " << clipData.displayName << " onto tab "
                                                  << tabIndex << ", button " << buttonIndex
                                                  << " (missing media)");
  }

  return true;
}

void SessionManager::setClip(int buttonIndex, const ClipData& clipData) {
  if (buttonIndex < 0 || buttonIndex >= BUTTONS_PER_TAB)
    return;

  int key = makeKey(m_currentTab, buttonIndex);
  auto clipWithTab = clipData;
  clipWithTab.tabIndex = m_currentTab;
  m_clips[key] = clipWithTab;
  m_isDirty = true;

  DBG("SessionManager: Updated clip metadata for tab " << m_currentTab << ", button " << buttonIndex
                                                       << " - Name: " << clipWithTab.displayName
                                                       << ", Group: " << clipWithTab.clipGroup);
}

void SessionManager::removeClip(int buttonIndex) {
  removeClip(buttonIndex, m_currentTab);
}

void SessionManager::removeClip(int buttonIndex, int tabIndex) {
  if (tabIndex < 0 || tabIndex >= NUM_TABS || buttonIndex < 0 || buttonIndex >= BUTTONS_PER_TAB)
    return;

  int key = makeKey(tabIndex, buttonIndex);
  auto it = m_clips.find(key);
  if (it != m_clips.end()) {
    DBG("SessionManager: Removed clip from tab " << tabIndex << ", button " << buttonIndex);
    m_clips.erase(it);
    m_isDirty = true;
  }
}

void SessionManager::swapClips(int buttonIndex1, int buttonIndex2) {
  m_isDirty = true;
  int key1 = makeKey(m_currentTab, buttonIndex1);
  int key2 = makeKey(m_currentTab, buttonIndex2);

  auto it1 = m_clips.find(key1);
  auto it2 = m_clips.find(key2);

  bool has1 = (it1 != m_clips.end());
  bool has2 = (it2 != m_clips.end());

  if (has1 && has2) {
    // Both buttons have clips - swap them
    std::swap(it1->second, it2->second);
    DBG("SessionManager: Swapped clips between buttons " << buttonIndex1 << " and "
                                                         << buttonIndex2);
  } else if (has1 && !has2) {
    // Only button 1 has a clip - move it to button 2
    m_clips[key2] = it1->second;
    m_clips.erase(it1);
    DBG("SessionManager: Moved clip from button " << buttonIndex1 << " to " << buttonIndex2);
  } else if (!has1 && has2) {
    // Only button 2 has a clip - move it to button 1
    m_clips[key1] = it2->second;
    m_clips.erase(it2);
    DBG("SessionManager: Moved clip from button " << buttonIndex2 << " to " << buttonIndex1);
  } else {
    // Neither button has a clip - nothing to swap
    DBG("SessionManager: No clips to swap (both buttons empty)");
  }
}

SessionManager::ClipData SessionManager::getClip(int buttonIndex) const {
  if (buttonIndex < 0 || buttonIndex >= BUTTONS_PER_TAB)
    return ClipData();

  int key = makeKey(m_currentTab, buttonIndex);
  auto it = m_clips.find(key);
  if (it != m_clips.end())
    return it->second;

  return ClipData(); // Empty/invalid
}

bool SessionManager::hasClip(int buttonIndex) const {
  if (buttonIndex < 0 || buttonIndex >= BUTTONS_PER_TAB)
    return false;

  int key = makeKey(m_currentTab, buttonIndex);
  return m_clips.find(key) != m_clips.end();
}

SessionManager::ClipData SessionManager::getClip(int buttonIndex, int tabIndex) const {
  if (tabIndex < 0 || tabIndex >= NUM_TABS || buttonIndex < 0 || buttonIndex >= BUTTONS_PER_TAB)
    return ClipData();

  int key = makeKey(tabIndex, buttonIndex);
  auto it = m_clips.find(key);
  if (it != m_clips.end())
    return it->second;

  return ClipData(); // Empty/invalid
}

SessionManager::ClipData SessionManager::getClipByGlobalIndex(int globalClipIndex) const {
  if (globalClipIndex < 0)
    return ClipData();

  const int tabIndex = globalClipIndex / BUTTONS_PER_TAB;
  const int buttonIndex = globalClipIndex % BUTTONS_PER_TAB;
  return getClip(buttonIndex, tabIndex);
}

bool SessionManager::hasClip(int buttonIndex, int tabIndex) const {
  if (tabIndex < 0 || tabIndex >= NUM_TABS || buttonIndex < 0 || buttonIndex >= BUTTONS_PER_TAB)
    return false;

  int key = makeKey(tabIndex, buttonIndex);
  return m_clips.find(key) != m_clips.end();
}

bool SessionManager::hasClipByGlobalIndex(int globalClipIndex) const {
  if (globalClipIndex < 0)
    return false;

  const int tabIndex = globalClipIndex / BUTTONS_PER_TAB;
  const int buttonIndex = globalClipIndex % BUTTONS_PER_TAB;
  return hasClip(buttonIndex, tabIndex);
}

void SessionManager::setClip(int buttonIndex, const ClipData& clipData, int tabIndex) {
  if (tabIndex < 0 || tabIndex >= NUM_TABS || buttonIndex < 0 || buttonIndex >= BUTTONS_PER_TAB)
    return;

  int key = makeKey(tabIndex, buttonIndex);
  auto clipWithTab = clipData;
  clipWithTab.tabIndex = tabIndex;
  m_clips[key] = clipWithTab;
  m_isDirty = true;

  DBG("SessionManager: Updated clip metadata for tab " << tabIndex << ", button " << buttonIndex
                                                       << " - Name: " << clipWithTab.displayName
                                                       << ", Group: " << clipWithTab.clipGroup);
}

bool SessionManager::clearTab(int tabIndex) {
  if (tabIndex < 0 || tabIndex >= NUM_TABS)
    return false;

  bool removedAnyClips = false;
  for (int buttonIndex = 0; buttonIndex < BUTTONS_PER_TAB; ++buttonIndex) {
    const int key = makeKey(tabIndex, buttonIndex);
    auto it = m_clips.find(key);
    if (it != m_clips.end()) {
      m_clips.erase(it);
      removedAnyClips = true;
    }
  }

  if (removedAnyClips) {
    m_isDirty = true;
    DBG("SessionManager: Cleared tab " << tabIndex);
  }

  return removedAnyClips;
}

//==============================================================================
SessionManager::ClipData SessionManager::extractMetadata(const juce::String& filePath,
                                                         bool allowMissingMedia) {
  ClipData data;
  data.filePath = filePath.toStdString();

  juce::File file(filePath);

  // Display name: filename without extension
  data.displayName = file.getFileNameWithoutExtension().toStdString();

  // Color based on file extension (basic heuristic)
  auto ext = file.getFileExtension().toLowerCase();
  if (ext == ".wav")
    data.color = juce::Colours::blue;
  else if (ext == ".aiff" || ext == ".aif")
    data.color = juce::Colours::green;
  else if (ext == ".flac")
    data.color = juce::Colours::purple;
  else
    data.color = juce::Colours::grey;

  // Read audio metadata using JUCE
  juce::AudioFormatManager formatManager;
  formatManager.registerBasicFormats(); // WAV, AIFF, FLAC

  std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

  if (reader) {
    data.sampleRate = static_cast<int>(reader->sampleRate);
    data.numChannels = static_cast<int>(reader->numChannels);
    data.durationSamples = reader->lengthInSamples;

    // Initialize trim points to full duration
    data.trimInSamples = 0;
    data.trimOutSamples = data.durationSamples;
    data.mediaAvailable = true;
    data.mediaStatus.clear();
  } else {
    DBG("SessionManager: Failed to create reader for: " << filePath);
    if (!allowMissingMedia) {
      data.filePath.clear(); // Mark as invalid
    } else {
      data.sampleRate = 0;
      data.numChannels = 0;
      data.durationSamples = 0;
      data.trimInSamples = 0;
      data.trimOutSamples = 0;
      data.mediaAvailable = false;
      data.mediaStatus = "Missing media";
    }
  }

  return data;
}

//==============================================================================
bool SessionManager::saveSession(const juce::File& file) {
  // Create JSON structure
  juce::var sessionJson = juce::var(new juce::DynamicObject());
  auto* sessionObj = sessionJson.getDynamicObject();

  serializeSessionMetadata(*sessionObj);
  serializeClipArray(*sessionObj, file.getParentDirectory(), juce::File(), false, nullptr);

  // Write to file
  juce::String jsonString = juce::JSON::toString(sessionJson, true); // Pretty print

  if (file.replaceWithText(jsonString)) {
    m_currentFile = file;
    m_isDirty = false;
    m_lastPackageManifest = {};
    m_sessionLineage.updatedAtUtc = makeIsoUtcTimestamp().toStdString();
    DBG("SessionManager: Saved session to: " << file.getFullPathName());
    return true;
  }

  DBG("SessionManager: Failed to write session file: " << file.getFullPathName());
  return false;
}

bool SessionManager::loadSession(const juce::File& file) {
  if (!file.existsAsFile()) {
    DBG("SessionManager: Session file not found: " << file.getFullPathName());
    return false;
  }

  // Parse JSON
  juce::String jsonText = file.loadFileAsString();
  juce::var sessionJson = juce::JSON::parse(jsonText);

  if (!sessionJson.isObject()) {
    DBG("SessionManager: Invalid JSON in session file");
    return false;
  }

  // Clear current session
  clearSession();

  // Load session metadata
  auto* sessionObj = sessionJson.getDynamicObject();
  if (sessionObj) {
    applySessionMetadata(*sessionObj, file);
  }

  // Load clips
  auto clipsArray = sessionObj->getProperty("clips");
  if (clipsArray.isArray()) {
    for (int i = 0; i < clipsArray.size(); ++i) {
      auto clipJson = clipsArray[i];
      if (!clipJson.isObject())
        continue;

      auto* clipObj = clipJson.getDynamicObject();
      int tabIndex = clipObj->getProperty("tabIndex");
      int buttonIndex = clipObj->getProperty("buttonIndex");
      juce::String filePath = clipObj->getProperty("filePath").toString();
      juce::File resolvedPath = resolveSessionPath(filePath, file.getParentDirectory());

      const bool allowMissingMedia = true;

      // Load clip into the requested tab without mutating the active UI tab.
      if (loadClipForTab(buttonIndex, resolvedPath.getFullPathName(), tabIndex,
                         allowMissingMedia)) {
        // Restore additional metadata from session
        int key = makeKey(tabIndex, buttonIndex);
        auto& clipData = m_clips[key];

        // Restore display name and clip group
        if (clipObj->hasProperty("displayName")) {
          clipData.displayName = clipObj->getProperty("displayName").toString().toStdString();
        }
        if (clipObj->hasProperty("clipGroup")) {
          clipData.clipGroup = static_cast<int>(clipObj->getProperty("clipGroup"));
        }

        // Phase 2: Restore trim points
        if (clipObj->hasProperty("trimInSamples")) {
          clipData.trimInSamples = static_cast<int64_t>(clipObj->getProperty("trimInSamples"));
        }
        if (clipObj->hasProperty("trimOutSamples")) {
          clipData.trimOutSamples = static_cast<int64_t>(clipObj->getProperty("trimOutSamples"));
        }

        // Phase 3: Restore fade times
        if (clipObj->hasProperty("fadeInSeconds")) {
          clipData.fadeInSeconds = static_cast<double>(clipObj->getProperty("fadeInSeconds"));
        }
        if (clipObj->hasProperty("fadeOutSeconds")) {
          clipData.fadeOutSeconds = static_cast<double>(clipObj->getProperty("fadeOutSeconds"));
        }
        if (clipObj->hasProperty("fadeInCurve")) {
          clipData.fadeInCurve = clipObj->getProperty("fadeInCurve").toString().toStdString();
        }
        if (clipObj->hasProperty("fadeOutCurve")) {
          clipData.fadeOutCurve = clipObj->getProperty("fadeOutCurve").toString().toStdString();
        }

        // Restore playback modes
        if (clipObj->hasProperty("gainDb")) {
          clipData.gainDb = static_cast<double>(clipObj->getProperty("gainDb"));
        }
        if (clipObj->hasProperty("loopEnabled")) {
          clipData.loopEnabled = static_cast<bool>(clipObj->getProperty("loopEnabled"));
        }
        if (clipObj->hasProperty("stopOthersEnabled")) {
          clipData.stopOthersEnabled = static_cast<bool>(clipObj->getProperty("stopOthersEnabled"));
        }

        // Restore color (parse hex string like "ff3498db")
        if (clipObj->hasProperty("color")) {
          clipData.color = juce::Colour::fromString(clipObj->getProperty("color").toString());
        }

        if (clipObj->hasProperty("mediaAvailable")) {
          clipData.mediaAvailable = static_cast<bool>(clipObj->getProperty("mediaAvailable"));
        }
        if (clipObj->hasProperty("mediaStatus")) {
          clipData.mediaStatus = clipObj->getProperty("mediaStatus").toString().toStdString();
        }

        if (!clipData.mediaAvailable) {
          recordMissingMedia(tabIndex, buttonIndex, clipData.filePath, "Session media unresolved");
        }
      }
    }
  }

  rebuildMissingMediaState();

  m_currentFile = file;
  m_isDirty = false;
  m_sessionLineage.updatedAtUtc = makeIsoUtcTimestamp().toStdString();
  DBG("SessionManager: Loaded session from: " << file.getFullPathName());
  return true;
}

void SessionManager::clearSession() {
  m_clips.clear();
  m_isDirty = true;
  m_sessionName = "Untitled";
  m_currentFile = juce::File();
  m_missingMediaResolutions.clear();
  m_lastPackageManifest = {};

  // Reset tab labels to defaults
  for (int i = 0; i < NUM_TABS; ++i) {
    m_tabLabels[i] = (juce::String("Tab ") + juce::String(i + 1)).toStdString();
  }

  m_sessionLineage = {};
  m_sessionLineage.sessionId = makeUuidString().toStdString();
  m_sessionLineage.createdAtUtc = makeIsoUtcTimestamp().toStdString();
  m_sessionLineage.updatedAtUtc = m_sessionLineage.createdAtUtc;

  DBG("SessionManager: Cleared session");
}

//==============================================================================
// Item 29: Clip Group Management

std::string SessionManager::getClipGroupName(int groupIndex) const {
  if (groupIndex >= 0 && groupIndex < NUM_CLIP_GROUPS)
    return m_clipGroupNames[groupIndex];
  return "Group " + std::to_string(groupIndex + 1); // Fallback
}

void SessionManager::setClipGroupName(int groupIndex, const std::string& name) {
  if (groupIndex >= 0 && groupIndex < NUM_CLIP_GROUPS) {
    m_clipGroupNames[groupIndex] = name;
    DBG("SessionManager: Group " << groupIndex << " name set to: " << name);
  }
}

std::string SessionManager::getClipGroupAbbreviation(int groupIndex) const {
  if (groupIndex < 0 || groupIndex >= NUM_CLIP_GROUPS)
    return "G" + std::to_string(groupIndex + 1);

  std::string name = m_clipGroupNames[groupIndex];

  // If it's the default name, return short form
  if (name.find("Group ") == 0) {
    return "G" + std::to_string(groupIndex + 1);
  }

  // Create abbreviation from custom name
  std::string abbrev;

  // Strategy 1: Use first 3 chars if short enough
  if (name.length() <= 3) {
    abbrev = name;
    std::transform(abbrev.begin(), abbrev.end(), abbrev.begin(), ::toupper);
    return abbrev;
  }

  // Strategy 2: Use uppercase letters if present (e.g., "Sound Effects" -> "SE")
  for (char c : name) {
    if (std::isupper(c)) {
      abbrev += c;
      if (abbrev.length() >= 3)
        break;
    }
  }

  if (!abbrev.empty() && abbrev.length() <= 3)
    return abbrev;

  // Strategy 3: Use first letter of each word
  abbrev.clear();
  bool newWord = true;
  for (char c : name) {
    if (std::isspace(c)) {
      newWord = true;
    } else if (newWord && std::isalpha(c)) {
      abbrev += std::toupper(c);
      newWord = false;
      if (abbrev.length() >= 3)
        break;
    }
  }

  if (!abbrev.empty())
    return abbrev.substr(0, 3);

  // Strategy 4: Just use first 3 chars
  abbrev = name.substr(0, 3);
  std::transform(abbrev.begin(), abbrev.end(), abbrev.begin(), ::toupper);
  return abbrev;
}

//==============================================================================
void SessionManager::setSessionLineage(const SessionLineage& lineage) {
  m_sessionLineage = lineage;
  if (m_sessionLineage.sessionId.empty()) {
    m_sessionLineage.sessionId = makeUuidString().toStdString();
  }
  if (m_sessionLineage.createdAtUtc.empty()) {
    m_sessionLineage.createdAtUtc = makeIsoUtcTimestamp().toStdString();
  }
  m_sessionLineage.updatedAtUtc = makeIsoUtcTimestamp().toStdString();
  m_isDirty = true;
}

void SessionManager::clearMissingMedia() {
  m_missingMediaResolutions.clear();
}

bool SessionManager::relinkMissingMedia(int tabIndex, int buttonIndex,
                                        const juce::File& replacementFile,
                                        MissingMediaResolution* outResolution) {
  if (tabIndex < 0 || tabIndex >= NUM_TABS || buttonIndex < 0 || buttonIndex >= BUTTONS_PER_TAB ||
      !replacementFile.existsAsFile()) {
    return false;
  }

  auto clip = getClip(buttonIndex, tabIndex);
  if (!clip.isValid()) {
    return false;
  }

  auto relinked = extractMetadata(replacementFile.getFullPathName(), false);
  if (!relinked.isValid()) {
    return false;
  }

  relinked.displayName = clip.displayName.empty() ? relinked.displayName : clip.displayName;
  relinked.color = clip.color;
  relinked.clipGroup = clip.clipGroup;
  relinked.tabIndex = tabIndex;
  relinked.trimInSamples = juce::jlimit<int64_t>(0, relinked.durationSamples, clip.trimInSamples);
  relinked.trimOutSamples =
      juce::jlimit<int64_t>(relinked.trimInSamples, relinked.durationSamples, clip.trimOutSamples);
  relinked.fadeInSeconds = clip.fadeInSeconds;
  relinked.fadeOutSeconds = clip.fadeOutSeconds;
  relinked.fadeInCurve = clip.fadeInCurve;
  relinked.fadeOutCurve = clip.fadeOutCurve;
  relinked.gainDb = clip.gainDb;
  relinked.loopEnabled = clip.loopEnabled;
  relinked.stopOthersEnabled = clip.stopOthersEnabled;
  relinked.mediaAvailable = true;
  relinked.mediaStatus.clear();

  setClip(buttonIndex, relinked, tabIndex);
  rebuildMissingMediaState();

  MissingMediaResolution resolution;
  resolution.tabIndex = tabIndex;
  resolution.buttonIndex = buttonIndex;
  resolution.originalPath = clip.filePath;
  resolution.resolvedPath = replacementFile.getFullPathName().toStdString();
  resolution.candidatePath = resolution.resolvedPath;
  resolution.reason = "Relinked manually";
  resolution.matchedByFilename = true;

  if (outResolution) {
    *outResolution = resolution;
  }

  return true;
}

bool SessionManager::relinkMissingMediaByGlobalIndex(int globalClipIndex,
                                                     const juce::File& replacementFile,
                                                     MissingMediaResolution* outResolution) {
  if (globalClipIndex < 0) {
    return false;
  }

  const int tabIndex = globalClipIndex / BUTTONS_PER_TAB;
  const int buttonIndex = globalClipIndex % BUTTONS_PER_TAB;
  return relinkMissingMedia(tabIndex, buttonIndex, replacementFile, outResolution);
}

bool SessionManager::relinkMissingMediaFromSearchRoot(int tabIndex, int buttonIndex,
                                                      const juce::File& searchRoot,
                                                      MissingMediaResolution* outResolution) {
  auto clip = getClip(buttonIndex, tabIndex);
  if (!clip.isValid()) {
    return false;
  }

  auto candidates = findMissingMediaCandidates(clip, searchRoot, 5);
  if (candidates.isEmpty()) {
    return false;
  }

  return relinkMissingMedia(tabIndex, buttonIndex, candidates[0], outResolution);
}

juce::Array<juce::File> SessionManager::findMissingMediaCandidates(const ClipData& clip,
                                                                   const juce::File& searchRoot,
                                                                   int maxResults) const {
  juce::Array<juce::File> results;
  if (!searchRoot.exists() || !clip.isValid()) {
    return results;
  }

  juce::File originalFile(clip.filePath);
  const juce::String exactName = originalFile.getFileName();
  const juce::String stem = originalFile.getFileNameWithoutExtension();
  const juce::String extension = originalFile.getFileExtension();

  auto appendUnique = [&results](const juce::Array<juce::File>& files) {
    for (const auto& file : files) {
      bool alreadyPresent = false;
      for (const auto& existing : results) {
        if (existing.getFullPathName() == file.getFullPathName()) {
          alreadyPresent = true;
          break;
        }
      }
      if (!alreadyPresent) {
        results.add(file);
      }
    }
  };

  if (exactName.isNotEmpty()) {
    appendUnique(searchRoot.findChildFiles(juce::File::findFiles, true, exactName));
  }

  if (stem.isNotEmpty()) {
    appendUnique(searchRoot.findChildFiles(juce::File::findFiles, true, stem + ".*"));
  }

  struct CandidateScore {
    juce::File file;
    int score = 0;
  };

  std::vector<CandidateScore> scored;
  juce::AudioFormatManager formatManager;
  formatManager.registerBasicFormats();

  for (const auto& file : results) {
    int score = 0;
    if (file.getFileName() == exactName) {
      score += 100;
    }
    if (file.getFileNameWithoutExtension() == stem) {
      score += 50;
    }
    if (file.getFileExtension().equalsIgnoreCase(extension)) {
      score += 10;
    }

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader) {
      if (clip.sampleRate > 0 && static_cast<int>(reader->sampleRate) == clip.sampleRate) {
        score += 25;
      }
      if (clip.durationSamples > 0 &&
          reader->lengthInSamples == static_cast<juce::int64>(clip.durationSamples)) {
        score += 25;
      }
    }

    scored.push_back({file, score});
  }

  std::sort(scored.begin(), scored.end(), [](const CandidateScore& a, const CandidateScore& b) {
    if (a.score != b.score) {
      return a.score > b.score;
    }
    return a.file.getFullPathName() < b.file.getFullPathName();
  });

  juce::Array<juce::File> ranked;
  for (const auto& candidate : scored) {
    ranked.add(candidate.file);
    if (maxResults > 0 && ranked.size() >= maxResults) {
      break;
    }
  }

  return ranked;
}

void SessionManager::rebuildMissingMediaState() {
  m_missingMediaResolutions.clear();

  for (const auto& [key, clip] : m_clips) {
    juce::File clipFile(clip.filePath);
    const bool fileExists = clipFile.existsAsFile();
    const int tabIndex = key / 100;
    const int buttonIndex = key % 100;

    if (fileExists) {
      if (!clip.mediaAvailable) {
        auto resolved = clip;
        resolved.mediaAvailable = true;
        resolved.mediaStatus.clear();
        setClip(buttonIndex, resolved, tabIndex);
      }
      continue;
    }

    recordMissingMedia(tabIndex, buttonIndex, clip.filePath, "Missing media");
  }
}

void SessionManager::recordMissingMedia(int tabIndex, int buttonIndex,
                                        const juce::String& originalPath,
                                        const juce::String& reason,
                                        const juce::String& candidatePath, bool matchedByFilename,
                                        bool matchedByMetadata) {
  MissingMediaResolution resolution;
  resolution.tabIndex = tabIndex;
  resolution.buttonIndex = buttonIndex;
  resolution.originalPath = originalPath.toStdString();
  resolution.candidatePath = candidatePath.toStdString();
  resolution.reason = reason.toStdString();
  resolution.matchedByFilename = matchedByFilename;
  resolution.matchedByMetadata = matchedByMetadata;

  auto existing =
      std::find_if(m_missingMediaResolutions.begin(), m_missingMediaResolutions.end(),
                   [tabIndex, buttonIndex](const MissingMediaResolution& entry) {
                     return entry.tabIndex == tabIndex && entry.buttonIndex == buttonIndex;
                   });

  if (existing != m_missingMediaResolutions.end()) {
    *existing = resolution;
  } else {
    m_missingMediaResolutions.push_back(resolution);
  }

  auto clip = getClip(buttonIndex, tabIndex);
  if (clip.isValid()) {
    clip.mediaAvailable = false;
    clip.mediaStatus = reason.toStdString();
    setClip(buttonIndex, clip, tabIndex);
  }
}

juce::String SessionManager::makeUtcTimestamp() const {
  return makeIsoUtcTimestamp();
}

juce::String SessionManager::makeUuidString() const {
  return ::makeUuidString();
}

juce::File SessionManager::resolveSessionPath(const juce::String& path,
                                              const juce::File& baseDirectory) const {
  if (path.isEmpty()) {
    return {};
  }

  if (juce::File::isAbsolutePath(path)) {
    return juce::File(path);
  }

  if (baseDirectory.exists()) {
    return baseDirectory.getChildFile(path);
  }

  return juce::File::getCurrentWorkingDirectory().getChildFile(path);
}

juce::String SessionManager::makePortablePath(const juce::File& absoluteFile,
                                              const juce::File& baseDirectory) const {
  if (!absoluteFile.exists()) {
    return absoluteFile.getFullPathName();
  }

  if (baseDirectory.exists() && absoluteFile.isAChildOf(baseDirectory)) {
    auto relative = absoluteFile.getRelativePathFrom(baseDirectory);
    if (relative.isNotEmpty()) {
      return "./" + relative;
    }
  }

  return absoluteFile.getFullPathName();
}

void SessionManager::serializeSessionMetadata(juce::DynamicObject& sessionObj) const {
  sessionObj.setProperty("name", juce::String(m_sessionName));
  sessionObj.setProperty("version", juce::String(kSessionVersion));

  juce::Array<juce::var> tabLabelsArray;
  for (const auto& label : m_tabLabels) {
    tabLabelsArray.add(juce::var(label));
  }
  sessionObj.setProperty("tabLabels", juce::var(tabLabelsArray));

  auto* lineageObj = new juce::DynamicObject();
  lineageObj->setProperty("sessionId", juce::String(m_sessionLineage.sessionId));
  lineageObj->setProperty("parentSessionId", juce::String(m_sessionLineage.parentSessionId));
  lineageObj->setProperty("packageId", juce::String(m_sessionLineage.packageId));
  lineageObj->setProperty("sourceSessionPath", juce::String(m_sessionLineage.sourceSessionPath));
  lineageObj->setProperty("createdAtUtc", juce::String(m_sessionLineage.createdAtUtc));
  lineageObj->setProperty("updatedAtUtc", juce::String(m_sessionLineage.updatedAtUtc));
  sessionObj.setProperty("lineage", juce::var(lineageObj));

  auto* packageObj = new juce::DynamicObject();
  packageObj->setProperty("packageId", juce::String(m_lastPackageManifest.packageId));
  packageObj->setProperty("sessionId", juce::String(m_lastPackageManifest.sessionId));
  packageObj->setProperty("sessionName", juce::String(m_lastPackageManifest.sessionName));
  packageObj->setProperty("sessionVersion", juce::String(m_lastPackageManifest.sessionVersion));
  packageObj->setProperty("sourceSessionPath",
                          juce::String(m_lastPackageManifest.sourceSessionPath));
  packageObj->setProperty("packagePath", juce::String(m_lastPackageManifest.packagePath));
  packageObj->setProperty("mediaFolderName", juce::String(m_lastPackageManifest.mediaFolderName));
  packageObj->setProperty("createdAtUtc", juce::String(m_lastPackageManifest.createdAtUtc));
  packageObj->setProperty("mediaCount", m_lastPackageManifest.mediaCount);
  packageObj->setProperty("missingMediaCount", m_lastPackageManifest.missingMediaCount);
  packageObj->setProperty("copiedMedia", m_lastPackageManifest.copiedMedia);
  sessionObj.setProperty("package", juce::var(packageObj));

  juce::Array<juce::var> missingArray;
  for (const auto& entry : m_missingMediaResolutions) {
    auto* entryObj = new juce::DynamicObject();
    entryObj->setProperty("tabIndex", entry.tabIndex);
    entryObj->setProperty("buttonIndex", entry.buttonIndex);
    entryObj->setProperty("originalPath", juce::String(entry.originalPath));
    entryObj->setProperty("resolvedPath", juce::String(entry.resolvedPath));
    entryObj->setProperty("candidatePath", juce::String(entry.candidatePath));
    entryObj->setProperty("reason", juce::String(entry.reason));
    entryObj->setProperty("matchedByFilename", entry.matchedByFilename);
    entryObj->setProperty("matchedByMetadata", entry.matchedByMetadata);
    missingArray.add(juce::var(entryObj));
  }
  sessionObj.setProperty("missingMedia", juce::var(missingArray));
}

void SessionManager::serializeClipArray(juce::DynamicObject& sessionObj,
                                        const juce::File& sessionBaseDir,
                                        const juce::File& packageMediaDir, bool packageMode,
                                        SessionPackageManifest* outManifest) const {
  juce::Array<juce::var> clipsArray;
  std::map<std::string, juce::String> copiedMediaCache;

  for (const auto& [compositeKey, clipData] : m_clips) {
    juce::var clipJson = juce::var(new juce::DynamicObject());
    auto* clipObj = clipJson.getDynamicObject();

    const int buttonIndex = compositeKey % 100;
    juce::File sourceFile = resolveSessionPath(juce::String(clipData.filePath), sessionBaseDir);
    juce::String serializedPath = makePortablePath(sourceFile, sessionBaseDir);
    bool copiedMedia = false;

    if (packageMode && clipData.mediaAvailable && sourceFile.existsAsFile()) {
      juce::String cacheKey = sourceFile.getFullPathName();
      auto cacheIt = copiedMediaCache.find(cacheKey.toStdString());
      if (cacheIt != copiedMediaCache.end()) {
        serializedPath = cacheIt->second;
        copiedMedia = true;
      } else {
        juce::String safeFileName = sanitizeFileName(sourceFile.getFileName());
        juce::File destination = packageMediaDir.getChildFile(safeFileName);
        int suffix = 1;
        while (destination.exists()) {
          destination =
              packageMediaDir.getChildFile(sourceFile.getFileNameWithoutExtension() + "_" +
                                           juce::String(suffix) + sourceFile.getFileExtension());
          ++suffix;
        }

        if (!destination.getParentDirectory().exists()) {
          destination.getParentDirectory().createDirectory();
        }

        if (sourceFile.copyFileTo(destination)) {
          serializedPath = "./" + juce::String(kMediaFolderName) + "/" + destination.getFileName();
          copiedMediaCache.emplace(cacheKey.toStdString(), serializedPath);
          copiedMedia = true;
          if (outManifest) {
            outManifest->copiedMedia = true;
            ++outManifest->mediaCount;
          }
        }
      }
    }

    const bool shouldCountAsMissing =
        clipData.filePath.empty() || !clipData.mediaAvailable ||
        (packageMode && clipData.mediaAvailable && sourceFile.existsAsFile() && !copiedMedia);
    if (shouldCountAsMissing && outManifest) {
      ++outManifest->missingMediaCount;
    }

    clipObj->setProperty("tabIndex", clipData.tabIndex);
    clipObj->setProperty("buttonIndex", buttonIndex);
    clipObj->setProperty("filePath", serializedPath);
    clipObj->setProperty("displayName", juce::String(clipData.displayName));
    clipObj->setProperty("clipGroup", clipData.clipGroup);
    clipObj->setProperty("trimInSamples",
                         juce::var(static_cast<juce::int64>(clipData.trimInSamples)));
    clipObj->setProperty("trimOutSamples",
                         juce::var(static_cast<juce::int64>(clipData.trimOutSamples)));
    clipObj->setProperty("fadeInSeconds", clipData.fadeInSeconds);
    clipObj->setProperty("fadeOutSeconds", clipData.fadeOutSeconds);
    clipObj->setProperty("fadeInCurve", juce::String(clipData.fadeInCurve));
    clipObj->setProperty("fadeOutCurve", juce::String(clipData.fadeOutCurve));
    clipObj->setProperty("gainDb", clipData.gainDb);
    clipObj->setProperty("loopEnabled", clipData.loopEnabled);
    clipObj->setProperty("stopOthersEnabled", clipData.stopOthersEnabled);
    clipObj->setProperty("mediaAvailable", clipData.mediaAvailable);
    clipObj->setProperty("mediaStatus", juce::String(clipData.mediaStatus));
    clipObj->setProperty("color", juce::String(clipData.color.toString()));

    clipsArray.add(clipJson);
  }

  sessionObj.setProperty("clips", juce::var(clipsArray));
}

bool SessionManager::applySessionMetadata(const juce::DynamicObject& sessionObj,
                                          const juce::File& sourceFile) {
  m_sessionName = sessionObj.getProperty("name").toString().toStdString();

  auto versionValue = sessionObj.getProperty("version");
  if (versionValue.isString()) {
    m_lastPackageManifest.sessionVersion = versionValue.toString().toStdString();
  }

  auto tabLabelsArray = sessionObj.getProperty("tabLabels");
  if (tabLabelsArray.isArray()) {
    for (int i = 0; i < juce::jmin(NUM_TABS, tabLabelsArray.size()); ++i) {
      m_tabLabels[i] = tabLabelsArray[i].toString().toStdString();
    }
  }

  auto lineageValue = sessionObj.getProperty("lineage");
  if (auto* lineageObj = lineageValue.getDynamicObject()) {
    m_sessionLineage.sessionId = lineageObj->getProperty("sessionId").toString().toStdString();
    m_sessionLineage.parentSessionId =
        lineageObj->getProperty("parentSessionId").toString().toStdString();
    m_sessionLineage.packageId = lineageObj->getProperty("packageId").toString().toStdString();
    m_sessionLineage.sourceSessionPath =
        lineageObj->getProperty("sourceSessionPath").toString().toStdString();
    m_sessionLineage.createdAtUtc =
        lineageObj->getProperty("createdAtUtc").toString().toStdString();
    m_sessionLineage.updatedAtUtc =
        lineageObj->getProperty("updatedAtUtc").toString().toStdString();
  }

  auto packageValue = sessionObj.getProperty("package");
  if (auto* packageObj = packageValue.getDynamicObject()) {
    m_lastPackageManifest.packageId = packageObj->getProperty("packageId").toString().toStdString();
    m_lastPackageManifest.sessionId = packageObj->getProperty("sessionId").toString().toStdString();
    m_lastPackageManifest.sessionName =
        packageObj->getProperty("sessionName").toString().toStdString();
    m_lastPackageManifest.sessionVersion =
        packageObj->getProperty("sessionVersion").toString().toStdString();
    m_lastPackageManifest.sourceSessionPath =
        packageObj->getProperty("sourceSessionPath").toString().toStdString();
    m_lastPackageManifest.packagePath =
        packageObj->getProperty("packagePath").toString().toStdString();
    m_lastPackageManifest.mediaFolderName =
        packageObj->getProperty("mediaFolderName").toString().toStdString();
    m_lastPackageManifest.createdAtUtc =
        packageObj->getProperty("createdAtUtc").toString().toStdString();
    m_lastPackageManifest.mediaCount = static_cast<int>(packageObj->getProperty("mediaCount"));
    m_lastPackageManifest.missingMediaCount =
        static_cast<int>(packageObj->getProperty("missingMediaCount"));
    m_lastPackageManifest.copiedMedia = static_cast<bool>(packageObj->getProperty("copiedMedia"));
  } else {
    m_lastPackageManifest = {};
    m_lastPackageManifest.sessionId = m_sessionLineage.sessionId;
    m_lastPackageManifest.sessionName = m_sessionName;
    m_lastPackageManifest.sessionVersion = kSessionVersion;
    m_lastPackageManifest.sourceSessionPath = sourceFile.getFullPathName().toStdString();
    m_lastPackageManifest.packagePath =
        sourceFile.getParentDirectory().getFullPathName().toStdString();
    m_lastPackageManifest.mediaFolderName = kMediaFolderName;
  }

  m_missingMediaResolutions.clear();
  auto missingValue = sessionObj.getProperty("missingMedia");
  if (missingValue.isArray()) {
    for (int i = 0; i < missingValue.size(); ++i) {
      auto entryValue = missingValue[i];
      if (!entryValue.isObject()) {
        continue;
      }

      auto* entryObj = entryValue.getDynamicObject();
      MissingMediaResolution resolution;
      resolution.tabIndex = static_cast<int>(entryObj->getProperty("tabIndex"));
      resolution.buttonIndex = static_cast<int>(entryObj->getProperty("buttonIndex"));
      resolution.originalPath = entryObj->getProperty("originalPath").toString().toStdString();
      resolution.resolvedPath = entryObj->getProperty("resolvedPath").toString().toStdString();
      resolution.candidatePath = entryObj->getProperty("candidatePath").toString().toStdString();
      resolution.reason = entryObj->getProperty("reason").toString().toStdString();
      resolution.matchedByFilename = static_cast<bool>(entryObj->getProperty("matchedByFilename"));
      resolution.matchedByMetadata = static_cast<bool>(entryObj->getProperty("matchedByMetadata"));
      m_missingMediaResolutions.push_back(resolution);
    }
  }

  m_sessionLineage.sourceSessionPath = sourceFile.getFullPathName().toStdString();
  if (m_sessionLineage.sessionId.empty()) {
    m_sessionLineage.sessionId = makeUuidString().toStdString();
  }
  if (m_sessionLineage.createdAtUtc.empty()) {
    m_sessionLineage.createdAtUtc = makeIsoUtcTimestamp().toStdString();
  }
  if (m_sessionLineage.updatedAtUtc.empty()) {
    m_sessionLineage.updatedAtUtc = makeIsoUtcTimestamp().toStdString();
  }

  return true;
}

bool SessionManager::exportSessionPackage(const juce::File& packageDirectory,
                                          SessionPackageManifest* outManifest) {
  if (packageDirectory == juce::File()) {
    return false;
  }

  if (!packageDirectory.exists() && !packageDirectory.createDirectory()) {
    return false;
  }

  juce::File mediaDirectory = packageDirectory.getChildFile(kMediaFolderName);
  if (!mediaDirectory.exists() && !mediaDirectory.createDirectory()) {
    return false;
  }

  SessionPackageManifest manifest;
  manifest.packageId = makeUuidString().toStdString();
  manifest.sessionId = m_sessionLineage.sessionId.empty() ? makeUuidString().toStdString()
                                                          : m_sessionLineage.sessionId;
  manifest.sessionName = m_sessionName;
  manifest.sessionVersion = kSessionVersion;
  manifest.sourceSessionPath = m_currentFile.getFullPathName().toStdString();
  manifest.packagePath = packageDirectory.getFullPathName().toStdString();
  manifest.mediaFolderName = kMediaFolderName;
  manifest.createdAtUtc = makeIsoUtcTimestamp().toStdString();
  manifest.mediaCount = 0;
  manifest.missingMediaCount = 0;
  manifest.copiedMedia = false;

  m_lastPackageManifest = manifest;
  m_sessionLineage.packageId = manifest.packageId;
  m_sessionLineage.updatedAtUtc = manifest.createdAtUtc;

  juce::var sessionJson = juce::var(new juce::DynamicObject());
  auto* sessionObj = sessionJson.getDynamicObject();
  serializeSessionMetadata(*sessionObj);
  const juce::File sessionBaseDir = m_currentFile.existsAsFile()
                                        ? m_currentFile.getParentDirectory()
                                        : juce::File::getCurrentWorkingDirectory();
  serializeClipArray(*sessionObj, sessionBaseDir, mediaDirectory, true, &m_lastPackageManifest);

  juce::File sessionFile = packageDirectory.getChildFile(kSessionJsonName);
  juce::File manifestFile = packageDirectory.getChildFile(kManifestJsonName);

  juce::String sessionText = juce::JSON::toString(sessionJson, true);
  if (!sessionFile.replaceWithText(sessionText)) {
    return false;
  }

  auto* manifestObj = new juce::DynamicObject();
  manifestObj->setProperty("packageId", juce::String(m_lastPackageManifest.packageId));
  manifestObj->setProperty("sessionId", juce::String(m_lastPackageManifest.sessionId));
  manifestObj->setProperty("sessionName", juce::String(m_lastPackageManifest.sessionName));
  manifestObj->setProperty("sessionVersion", juce::String(m_lastPackageManifest.sessionVersion));
  manifestObj->setProperty("sourceSessionPath",
                           juce::String(m_lastPackageManifest.sourceSessionPath));
  manifestObj->setProperty("packagePath", juce::String(m_lastPackageManifest.packagePath));
  manifestObj->setProperty("mediaFolderName", juce::String(m_lastPackageManifest.mediaFolderName));
  manifestObj->setProperty("createdAtUtc", juce::String(m_lastPackageManifest.createdAtUtc));
  manifestObj->setProperty("mediaCount", m_lastPackageManifest.mediaCount);
  manifestObj->setProperty("missingMediaCount", m_lastPackageManifest.missingMediaCount);
  manifestObj->setProperty("copiedMedia", m_lastPackageManifest.copiedMedia);

  if (!manifestFile.replaceWithText(juce::JSON::toString(juce::var(manifestObj), true))) {
    return false;
  }

  if (outManifest) {
    *outManifest = m_lastPackageManifest;
  }

  DBG("SessionManager: Exported session package to: " << packageDirectory.getFullPathName());
  return true;
}

bool SessionManager::importSessionPackage(const juce::File& packageDirectory) {
  if (packageDirectory == juce::File()) {
    return false;
  }

  juce::File sessionFile = packageDirectory.getChildFile(kSessionJsonName);
  if (!sessionFile.existsAsFile()) {
    return false;
  }

  juce::File manifestFile = packageDirectory.getChildFile(kManifestJsonName);
  if (manifestFile.existsAsFile()) {
    juce::var manifestJson = juce::JSON::parse(manifestFile.loadFileAsString());
    if (auto* manifestObj = manifestJson.getDynamicObject()) {
      m_lastPackageManifest.packageId =
          manifestObj->getProperty("packageId").toString().toStdString();
      m_lastPackageManifest.sessionId =
          manifestObj->getProperty("sessionId").toString().toStdString();
      m_lastPackageManifest.sessionName =
          manifestObj->getProperty("sessionName").toString().toStdString();
      m_lastPackageManifest.sessionVersion =
          manifestObj->getProperty("sessionVersion").toString().toStdString();
      m_lastPackageManifest.sourceSessionPath =
          manifestObj->getProperty("sourceSessionPath").toString().toStdString();
      m_lastPackageManifest.packagePath =
          manifestObj->getProperty("packagePath").toString().toStdString();
      m_lastPackageManifest.mediaFolderName =
          manifestObj->getProperty("mediaFolderName").toString().toStdString();
      m_lastPackageManifest.createdAtUtc =
          manifestObj->getProperty("createdAtUtc").toString().toStdString();
      m_lastPackageManifest.mediaCount = static_cast<int>(manifestObj->getProperty("mediaCount"));
      m_lastPackageManifest.missingMediaCount =
          static_cast<int>(manifestObj->getProperty("missingMediaCount"));
      m_lastPackageManifest.copiedMedia =
          static_cast<bool>(manifestObj->getProperty("copiedMedia"));
    }
  }

  if (!loadSession(sessionFile)) {
    return false;
  }

  m_lastPackageManifest.packagePath = packageDirectory.getFullPathName().toStdString();
  if (m_lastPackageManifest.packageId.empty()) {
    m_lastPackageManifest.packageId = m_sessionLineage.packageId;
  }
  if (m_lastPackageManifest.sessionId.empty()) {
    m_lastPackageManifest.sessionId = m_sessionLineage.sessionId;
  }
  if (m_lastPackageManifest.sessionName.empty()) {
    m_lastPackageManifest.sessionName = m_sessionName;
  }
  if (m_lastPackageManifest.sessionVersion.empty()) {
    m_lastPackageManifest.sessionVersion = kSessionVersion;
  }

  DBG("SessionManager: Imported session package from: " << packageDirectory.getFullPathName());
  return true;
}
