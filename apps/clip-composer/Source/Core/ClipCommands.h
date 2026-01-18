/*
  ==============================================================================

    ClipCommands.h
    Created: 12 Jan 2026
    Author:  Orpheus Clip Composer

    Sprint 16: Undo/Redo Commands (OCC117)
    Concrete Command implementations for clip and page operations.

  ==============================================================================
*/

#pragma once

#include "../Session/SessionManager.h"
#include "Command.h"
#include <array>
#include <map>
#include <vector>

namespace orpheus {

//==============================================================================
/**
    Command: Edit a single clip's properties.
*/
class EditClipCommand : public Command {
public:
  EditClipCommand(SessionManager* sessionManager, int tabIndex, int buttonIndex,
                  const SessionManager::ClipData& oldData, const SessionManager::ClipData& newData);

  void execute() override;
  void undo() override;
  juce::String getDescription() const override;
  size_t getSizeInBytes() const override;

private:
  SessionManager* m_sessionManager;
  int m_tabIndex;
  int m_buttonIndex;
  SessionManager::ClipData m_oldData;
  SessionManager::ClipData m_newData;
};

//==============================================================================
/**
    Command: Clear a single clip.
*/
class ClearClipCommand : public Command {
public:
  ClearClipCommand(SessionManager* sessionManager, int tabIndex, int buttonIndex);

  void execute() override;
  void undo() override;
  juce::String getDescription() const override;

private:
  SessionManager* m_sessionManager;
  int m_tabIndex;
  int m_buttonIndex;
  SessionManager::ClipData m_savedData;
  bool m_hadClip = false;
};

//==============================================================================
/**
    Command: Clear a range of buttons.
*/
class ClearButtonsCommand : public Command {
public:
  ClearButtonsCommand(SessionManager* sessionManager, int startGlobalIndex, int endGlobalIndex);

  void execute() override;
  void undo() override;
  juce::String getDescription() const override;
  size_t getSizeInBytes() const override;

private:
  SessionManager* m_sessionManager;
  int m_startIndex;
  int m_endIndex;
  std::map<int, SessionManager::ClipData> m_savedClips; // globalIndex → ClipData
};

//==============================================================================
/**
    Command: Fill a range of buttons with the same audio file.
*/
class FillButtonsCommand : public Command {
public:
  FillButtonsCommand(SessionManager* sessionManager, int startGlobalIndex, int endGlobalIndex,
                     const juce::File& audioFile);

  void execute() override;
  void undo() override;
  juce::String getDescription() const override;
  size_t getSizeInBytes() const override;

private:
  SessionManager* m_sessionManager;
  int m_startIndex;
  int m_endIndex;
  juce::File m_audioFile;
  std::map<int, SessionManager::ClipData> m_savedClips;
};

//==============================================================================
/**
    Command: Copy page (stores page data for later paste).
    Note: This doesn't modify state directly, just stores clipboard data.
*/
class CopyPageCommand : public Command {
public:
  CopyPageCommand(SessionManager* sessionManager, int sourceTabIndex,
                  std::array<SessionManager::ClipData, 48>* pageClipboard);

  void execute() override;
  void undo() override;
  juce::String getDescription() const override;

private:
  SessionManager* m_sessionManager;
  int m_sourceTabIndex;
  std::array<SessionManager::ClipData, 48>* m_pageClipboard;
  std::array<SessionManager::ClipData, 48> m_previousClipboard;
  bool m_hadPreviousClipboard = false;
};

//==============================================================================
/**
    Command: Paste page from clipboard.
*/
class PastePageCommand : public Command {
public:
  PastePageCommand(SessionManager* sessionManager, int targetTabIndex,
                   const std::array<SessionManager::ClipData, 48>& pageClipboard);

  void execute() override;
  void undo() override;
  juce::String getDescription() const override;
  size_t getSizeInBytes() const override;

private:
  SessionManager* m_sessionManager;
  int m_targetTabIndex;
  std::array<SessionManager::ClipData, 48> m_clipboardData;
  std::array<SessionManager::ClipData, 48> m_previousData;
};

//==============================================================================
/**
    Command: Clear an entire page.
*/
class ClearPageCommand : public Command {
public:
  ClearPageCommand(SessionManager* sessionManager, int tabIndex);

  void execute() override;
  void undo() override;
  juce::String getDescription() const override;
  size_t getSizeInBytes() const override;

private:
  SessionManager* m_sessionManager;
  int m_tabIndex;
  std::array<SessionManager::ClipData, 48> m_savedData;
};

//==============================================================================
/**
    Command: Swap two pages.
*/
class SwapPagesCommand : public Command {
public:
  SwapPagesCommand(SessionManager* sessionManager, int tabIndex1, int tabIndex2);

  void execute() override;
  void undo() override;
  juce::String getDescription() const override;

private:
  SessionManager* m_sessionManager;
  int m_tabIndex1;
  int m_tabIndex2;
};

//==============================================================================
/**
    Command: Swap two clips within the same or different tabs.
*/
class SwapClipsCommand : public Command {
public:
  SwapClipsCommand(SessionManager* sessionManager, int globalIndex1, int globalIndex2);

  void execute() override;
  void undo() override;
  juce::String getDescription() const override;

private:
  SessionManager* m_sessionManager;
  int m_globalIndex1;
  int m_globalIndex2;
};

//==============================================================================
/**
    Command: Load a clip to a button.
*/
class LoadClipCommand : public Command {
public:
  LoadClipCommand(SessionManager* sessionManager, int tabIndex, int buttonIndex,
                  const juce::File& audioFile);

  void execute() override;
  void undo() override;
  juce::String getDescription() const override;

private:
  SessionManager* m_sessionManager;
  int m_tabIndex;
  int m_buttonIndex;
  juce::File m_audioFile;
  SessionManager::ClipData m_previousData;
  bool m_hadPreviousClip = false;
};

//==============================================================================
/**
    Options for Paste Special operation.
*/
struct PasteSpecialOptions {
  // Levels
  bool mute = false;
  bool pan = false;
  bool gainAbsolute = false;
  bool gainRelative = false;
  float gainRelativeDb = 0.0f;

  // Fades
  bool fadeIn = false;
  bool fadeInCurve = false;
  bool fadeOut = false;
  bool fadeOutCurve = false;

  // External Triggers (for future MIDI note assignment)
  bool midiNote = false;
  bool midiNoteAutoFill = false; // Sequential MIDI note assignment

  // Misc
  bool color = false;
  bool clipGroup = false;
  bool loop = false;
  bool stopOthers = false;

  // Count selected options
  int countSelected() const {
    int count = 0;
    if (mute)
      count++;
    if (pan)
      count++;
    if (gainAbsolute || gainRelative)
      count++;
    if (fadeIn)
      count++;
    if (fadeInCurve)
      count++;
    if (fadeOut)
      count++;
    if (fadeOutCurve)
      count++;
    if (midiNote)
      count++;
    if (color)
      count++;
    if (clipGroup)
      count++;
    if (loop)
      count++;
    if (stopOthers)
      count++;
    return count;
  }
};

//==============================================================================
/**
    Command: Paste Special - selectively paste clip properties.
*/
class PasteSpecialCommand : public Command {
public:
  PasteSpecialCommand(SessionManager* sessionManager, const std::vector<int>& targetGlobalIndices,
                      const SessionManager::ClipData& sourceClip,
                      const PasteSpecialOptions& options);

  void execute() override;
  void undo() override;
  juce::String getDescription() const override;
  size_t getSizeInBytes() const override;

private:
  void applyOptions(SessionManager::ClipData& target, const SessionManager::ClipData& source,
                    int autoFillIndex);

  SessionManager* m_sessionManager;
  std::vector<int> m_targetIndices;
  SessionManager::ClipData m_sourceClip;
  PasteSpecialOptions m_options;
  std::map<int, SessionManager::ClipData> m_savedClips;
};

} // namespace orpheus
