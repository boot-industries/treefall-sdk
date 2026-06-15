// SPDX-License-Identifier: MIT

#pragma once

#include <juce_core/juce_core.h>
#include <optional>

namespace occ {

struct ClipLoadPlan {
  juce::String sessionPath;
  juce::String audioEnginePath;
  bool copiedToProject = false;
};

inline ClipLoadPlan makeLinkedClipLoadPlan(const juce::File& sourceFile) {
  const auto path = sourceFile.getFullPathName();
  return {.sessionPath = path, .audioEnginePath = path, .copiedToProject = false};
}

inline juce::File makeUniqueProjectAudioDestination(const juce::File& sourceFile,
                                                    const juce::File& projectAudioDirectory) {
  auto destination = projectAudioDirectory.getChildFile(sourceFile.getFileName());
  int counter = 1;
  while (destination.exists()) {
    const auto nameWithoutExtension = sourceFile.getFileNameWithoutExtension();
    const auto extension = sourceFile.getFileExtension();
    destination = projectAudioDirectory.getChildFile(nameWithoutExtension + "_" +
                                                     juce::String(counter) + extension);
    ++counter;
  }
  return destination;
}

inline std::optional<ClipLoadPlan>
copyClipIntoProjectAudioDirectory(const juce::File& sourceFile,
                                  const juce::File& projectAudioDirectory) {
  if (!sourceFile.existsAsFile())
    return std::nullopt;

  if (!projectAudioDirectory.exists())
    projectAudioDirectory.createDirectory();

  const auto destination = makeUniqueProjectAudioDestination(sourceFile, projectAudioDirectory);
  if (!sourceFile.copyFileTo(destination))
    return std::nullopt;

  const auto copiedPath = destination.getFullPathName();
  return ClipLoadPlan{
      .sessionPath = copiedPath, .audioEnginePath = copiedPath, .copiedToProject = true};
}

} // namespace occ
