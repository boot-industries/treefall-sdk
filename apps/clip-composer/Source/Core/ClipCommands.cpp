/*
  ==============================================================================

    ClipCommands.cpp
    Created: 12 Jan 2026
    Author:  Orpheus Clip Composer

    Sprint 16: Undo/Redo Commands (OCC117)

  ==============================================================================
*/

#include "ClipCommands.h"

namespace orpheus {

//==============================================================================
// EditClipCommand

EditClipCommand::EditClipCommand(SessionManager* sessionManager, int tabIndex, int buttonIndex,
                                 const SessionManager::ClipData& oldData,
                                 const SessionManager::ClipData& newData)
    : m_sessionManager(sessionManager), m_tabIndex(tabIndex), m_buttonIndex(buttonIndex),
      m_oldData(oldData), m_newData(newData) {}

void EditClipCommand::execute() {
  if (m_sessionManager) {
    int originalTab = m_sessionManager->getActiveTab();
    m_sessionManager->setActiveTab(m_tabIndex);
    m_sessionManager->setClip(m_buttonIndex, m_newData);
    m_sessionManager->setActiveTab(originalTab);
  }
}

void EditClipCommand::undo() {
  if (m_sessionManager) {
    int originalTab = m_sessionManager->getActiveTab();
    m_sessionManager->setActiveTab(m_tabIndex);
    m_sessionManager->setClip(m_buttonIndex, m_oldData);
    m_sessionManager->setActiveTab(originalTab);
  }
}

juce::String EditClipCommand::getDescription() const {
  return "Edit Clip " + juce::String(m_tabIndex + 1) + ":" + juce::String(m_buttonIndex + 1);
}

size_t EditClipCommand::getSizeInBytes() const {
  return sizeof(*this) + m_oldData.filePath.size() + m_oldData.displayName.size() +
         m_newData.filePath.size() + m_newData.displayName.size();
}

//==============================================================================
// ClearClipCommand

ClearClipCommand::ClearClipCommand(SessionManager* sessionManager, int tabIndex, int buttonIndex)
    : m_sessionManager(sessionManager), m_tabIndex(tabIndex), m_buttonIndex(buttonIndex) {

  // Save current state
  if (m_sessionManager) {
    int originalTab = m_sessionManager->getActiveTab();
    m_sessionManager->setActiveTab(m_tabIndex);
    m_hadClip = m_sessionManager->hasClip(m_buttonIndex);
    if (m_hadClip) {
      m_savedData = m_sessionManager->getClip(m_buttonIndex);
    }
    m_sessionManager->setActiveTab(originalTab);
  }
}

void ClearClipCommand::execute() {
  if (m_sessionManager) {
    int originalTab = m_sessionManager->getActiveTab();
    m_sessionManager->setActiveTab(m_tabIndex);
    m_sessionManager->removeClip(m_buttonIndex);
    m_sessionManager->setActiveTab(originalTab);
  }
}

void ClearClipCommand::undo() {
  if (m_sessionManager && m_hadClip) {
    int originalTab = m_sessionManager->getActiveTab();
    m_sessionManager->setActiveTab(m_tabIndex);
    m_sessionManager->setClip(m_buttonIndex, m_savedData);
    m_sessionManager->setActiveTab(originalTab);
  }
}

juce::String ClearClipCommand::getDescription() const {
  return "Clear Clip " + juce::String(m_tabIndex + 1) + ":" + juce::String(m_buttonIndex + 1);
}

//==============================================================================
// ClearButtonsCommand

ClearButtonsCommand::ClearButtonsCommand(SessionManager* sessionManager, int startGlobalIndex,
                                         int endGlobalIndex)
    : m_sessionManager(sessionManager), m_startIndex(startGlobalIndex), m_endIndex(endGlobalIndex) {

  // Save current state of all buttons in range
  if (m_sessionManager) {
    int originalTab = m_sessionManager->getActiveTab();

    for (int globalIndex = m_startIndex; globalIndex <= m_endIndex; ++globalIndex) {
      int tabIndex = globalIndex / ::occ::BUTTONS_PER_TAB;
      int buttonIndex = globalIndex % ::occ::BUTTONS_PER_TAB;

      m_sessionManager->setActiveTab(tabIndex);
      if (m_sessionManager->hasClip(buttonIndex)) {
        m_savedClips[globalIndex] = m_sessionManager->getClip(buttonIndex);
      }
    }

    m_sessionManager->setActiveTab(originalTab);
  }
}

void ClearButtonsCommand::execute() {
  if (m_sessionManager) {
    int originalTab = m_sessionManager->getActiveTab();

    for (int globalIndex = m_startIndex; globalIndex <= m_endIndex; ++globalIndex) {
      int tabIndex = globalIndex / ::occ::BUTTONS_PER_TAB;
      int buttonIndex = globalIndex % ::occ::BUTTONS_PER_TAB;

      m_sessionManager->setActiveTab(tabIndex);
      m_sessionManager->removeClip(buttonIndex);
    }

    m_sessionManager->setActiveTab(originalTab);
  }
}

void ClearButtonsCommand::undo() {
  if (m_sessionManager) {
    int originalTab = m_sessionManager->getActiveTab();

    for (const auto& [globalIndex, clipData] : m_savedClips) {
      int tabIndex = globalIndex / ::occ::BUTTONS_PER_TAB;
      int buttonIndex = globalIndex % ::occ::BUTTONS_PER_TAB;

      m_sessionManager->setActiveTab(tabIndex);
      m_sessionManager->setClip(buttonIndex, clipData);
    }

    m_sessionManager->setActiveTab(originalTab);
  }
}

juce::String ClearButtonsCommand::getDescription() const {
  return "Clear Buttons " + juce::String(m_startIndex + 1) + ".." + juce::String(m_endIndex + 1);
}

size_t ClearButtonsCommand::getSizeInBytes() const {
  size_t size = sizeof(*this);
  for (const auto& [idx, clip] : m_savedClips) {
    size += sizeof(idx) + sizeof(clip) + clip.filePath.size() + clip.displayName.size();
  }
  return size;
}

//==============================================================================
// FillButtonsCommand

FillButtonsCommand::FillButtonsCommand(SessionManager* sessionManager, int startGlobalIndex,
                                       int endGlobalIndex, const juce::File& audioFile)
    : m_sessionManager(sessionManager), m_startIndex(startGlobalIndex), m_endIndex(endGlobalIndex),
      m_audioFile(audioFile) {

  // Save current state
  if (m_sessionManager) {
    int originalTab = m_sessionManager->getActiveTab();

    for (int globalIndex = m_startIndex; globalIndex <= m_endIndex; ++globalIndex) {
      int tabIndex = globalIndex / ::occ::BUTTONS_PER_TAB;
      int buttonIndex = globalIndex % ::occ::BUTTONS_PER_TAB;

      m_sessionManager->setActiveTab(tabIndex);
      if (m_sessionManager->hasClip(buttonIndex)) {
        m_savedClips[globalIndex] = m_sessionManager->getClip(buttonIndex);
      }
    }

    m_sessionManager->setActiveTab(originalTab);
  }
}

void FillButtonsCommand::execute() {
  if (m_sessionManager && m_audioFile.existsAsFile()) {
    int originalTab = m_sessionManager->getActiveTab();

    for (int globalIndex = m_startIndex; globalIndex <= m_endIndex; ++globalIndex) {
      int tabIndex = globalIndex / ::occ::BUTTONS_PER_TAB;
      int buttonIndex = globalIndex % ::occ::BUTTONS_PER_TAB;

      m_sessionManager->setActiveTab(tabIndex);
      m_sessionManager->loadClip(buttonIndex, m_audioFile.getFullPathName());
    }

    m_sessionManager->setActiveTab(originalTab);
  }
}

void FillButtonsCommand::undo() {
  if (m_sessionManager) {
    int originalTab = m_sessionManager->getActiveTab();

    for (int globalIndex = m_startIndex; globalIndex <= m_endIndex; ++globalIndex) {
      int tabIndex = globalIndex / ::occ::BUTTONS_PER_TAB;
      int buttonIndex = globalIndex % ::occ::BUTTONS_PER_TAB;

      m_sessionManager->setActiveTab(tabIndex);

      auto it = m_savedClips.find(globalIndex);
      if (it != m_savedClips.end()) {
        m_sessionManager->setClip(buttonIndex, it->second);
      } else {
        m_sessionManager->removeClip(buttonIndex);
      }
    }

    m_sessionManager->setActiveTab(originalTab);
  }
}

juce::String FillButtonsCommand::getDescription() const {
  return "Fill Buttons " + juce::String(m_startIndex + 1) + ".." + juce::String(m_endIndex + 1) +
         " with " + m_audioFile.getFileName();
}

size_t FillButtonsCommand::getSizeInBytes() const {
  size_t size = sizeof(*this);
  for (const auto& [idx, clip] : m_savedClips) {
    size += sizeof(idx) + sizeof(clip) + clip.filePath.size() + clip.displayName.size();
  }
  return size;
}

//==============================================================================
// CopyPageCommand

CopyPageCommand::CopyPageCommand(
    SessionManager* sessionManager, int sourceTabIndex,
    std::array<SessionManager::ClipData, ::occ::BUTTONS_PER_TAB>* pageClipboard)
    : m_sessionManager(sessionManager), m_sourceTabIndex(sourceTabIndex),
      m_pageClipboard(pageClipboard) {

  // Save previous clipboard state (for undo)
  if (m_pageClipboard) {
    m_previousClipboard = *m_pageClipboard;
    m_hadPreviousClipboard = true;
  }
}

void CopyPageCommand::execute() {
  if (m_sessionManager && m_pageClipboard) {
    int originalTab = m_sessionManager->getActiveTab();
    m_sessionManager->setActiveTab(m_sourceTabIndex);

    for (int i = 0; i < ::occ::BUTTONS_PER_TAB; ++i) {
      (*m_pageClipboard)[i] = m_sessionManager->getClip(i);
    }

    m_sessionManager->setActiveTab(originalTab);
  }
}

void CopyPageCommand::undo() {
  if (m_pageClipboard && m_hadPreviousClipboard) {
    *m_pageClipboard = m_previousClipboard;
  }
}

juce::String CopyPageCommand::getDescription() const {
  return "Copy Page " + juce::String(m_sourceTabIndex + 1);
}

//==============================================================================
// PastePageCommand

PastePageCommand::PastePageCommand(
    SessionManager* sessionManager, int targetTabIndex,
    const std::array<SessionManager::ClipData, ::occ::BUTTONS_PER_TAB>& pageClipboard)
    : m_sessionManager(sessionManager), m_targetTabIndex(targetTabIndex),
      m_clipboardData(pageClipboard) {

  // Save current page state
  if (m_sessionManager) {
    int originalTab = m_sessionManager->getActiveTab();
    m_sessionManager->setActiveTab(m_targetTabIndex);

    for (int i = 0; i < ::occ::BUTTONS_PER_TAB; ++i) {
      m_previousData[i] = m_sessionManager->getClip(i);
    }

    m_sessionManager->setActiveTab(originalTab);
  }
}

void PastePageCommand::execute() {
  if (m_sessionManager) {
    int originalTab = m_sessionManager->getActiveTab();
    m_sessionManager->setActiveTab(m_targetTabIndex);

    for (int i = 0; i < ::occ::BUTTONS_PER_TAB; ++i) {
      if (m_clipboardData[i].isValid()) {
        m_sessionManager->setClip(i, m_clipboardData[i]);
      } else {
        m_sessionManager->removeClip(i);
      }
    }

    m_sessionManager->setActiveTab(originalTab);
  }
}

void PastePageCommand::undo() {
  if (m_sessionManager) {
    int originalTab = m_sessionManager->getActiveTab();
    m_sessionManager->setActiveTab(m_targetTabIndex);

    for (int i = 0; i < ::occ::BUTTONS_PER_TAB; ++i) {
      if (m_previousData[i].isValid()) {
        m_sessionManager->setClip(i, m_previousData[i]);
      } else {
        m_sessionManager->removeClip(i);
      }
    }

    m_sessionManager->setActiveTab(originalTab);
  }
}

juce::String PastePageCommand::getDescription() const {
  return "Paste Page to Tab " + juce::String(m_targetTabIndex + 1);
}

size_t PastePageCommand::getSizeInBytes() const {
  size_t size = sizeof(*this);
  for (const auto& clip : m_clipboardData) {
    size += clip.filePath.size() + clip.displayName.size();
  }
  for (const auto& clip : m_previousData) {
    size += clip.filePath.size() + clip.displayName.size();
  }
  return size;
}

//==============================================================================
// ClearPageCommand

ClearPageCommand::ClearPageCommand(SessionManager* sessionManager, int tabIndex)
    : m_sessionManager(sessionManager), m_tabIndex(tabIndex) {

  // Save current page state
  if (m_sessionManager) {
    int originalTab = m_sessionManager->getActiveTab();
    m_sessionManager->setActiveTab(m_tabIndex);

    for (int i = 0; i < ::occ::BUTTONS_PER_TAB; ++i) {
      m_savedData[i] = m_sessionManager->getClip(i);
    }

    m_sessionManager->setActiveTab(originalTab);
  }
}

void ClearPageCommand::execute() {
  if (m_sessionManager) {
    int originalTab = m_sessionManager->getActiveTab();
    m_sessionManager->setActiveTab(m_tabIndex);

    for (int i = 0; i < ::occ::BUTTONS_PER_TAB; ++i) {
      m_sessionManager->removeClip(i);
    }

    m_sessionManager->setActiveTab(originalTab);
  }
}

void ClearPageCommand::undo() {
  if (m_sessionManager) {
    int originalTab = m_sessionManager->getActiveTab();
    m_sessionManager->setActiveTab(m_tabIndex);

    for (int i = 0; i < ::occ::BUTTONS_PER_TAB; ++i) {
      if (m_savedData[i].isValid()) {
        m_sessionManager->setClip(i, m_savedData[i]);
      }
    }

    m_sessionManager->setActiveTab(originalTab);
  }
}

juce::String ClearPageCommand::getDescription() const {
  return "Clear Page " + juce::String(m_tabIndex + 1);
}

size_t ClearPageCommand::getSizeInBytes() const {
  size_t size = sizeof(*this);
  for (const auto& clip : m_savedData) {
    size += clip.filePath.size() + clip.displayName.size();
  }
  return size;
}

//==============================================================================
// SwapPagesCommand

SwapPagesCommand::SwapPagesCommand(SessionManager* sessionManager, int tabIndex1, int tabIndex2)
    : m_sessionManager(sessionManager), m_tabIndex1(tabIndex1), m_tabIndex2(tabIndex2) {}

void SwapPagesCommand::execute() {
  if (m_sessionManager && m_tabIndex1 != m_tabIndex2) {
    int originalTab = m_sessionManager->getActiveTab();

    // Get all clips from both tabs
    std::array<SessionManager::ClipData, ::occ::BUTTONS_PER_TAB> page1, page2;

    m_sessionManager->setActiveTab(m_tabIndex1);
    for (int i = 0; i < ::occ::BUTTONS_PER_TAB; ++i) {
      page1[i] = m_sessionManager->getClip(i);
    }

    m_sessionManager->setActiveTab(m_tabIndex2);
    for (int i = 0; i < ::occ::BUTTONS_PER_TAB; ++i) {
      page2[i] = m_sessionManager->getClip(i);
    }

    // Swap: page1 → tab2, page2 → tab1
    m_sessionManager->setActiveTab(m_tabIndex1);
    for (int i = 0; i < ::occ::BUTTONS_PER_TAB; ++i) {
      if (page2[i].isValid()) {
        m_sessionManager->setClip(i, page2[i]);
      } else {
        m_sessionManager->removeClip(i);
      }
    }

    m_sessionManager->setActiveTab(m_tabIndex2);
    for (int i = 0; i < ::occ::BUTTONS_PER_TAB; ++i) {
      if (page1[i].isValid()) {
        m_sessionManager->setClip(i, page1[i]);
      } else {
        m_sessionManager->removeClip(i);
      }
    }

    m_sessionManager->setActiveTab(originalTab);
  }
}

void SwapPagesCommand::undo() {
  // Swap is symmetric - just execute again
  execute();
}

juce::String SwapPagesCommand::getDescription() const {
  return "Swap Pages " + juce::String(m_tabIndex1 + 1) + " and " + juce::String(m_tabIndex2 + 1);
}

//==============================================================================
// SwapClipsCommand

SwapClipsCommand::SwapClipsCommand(SessionManager* sessionManager, int globalIndex1,
                                   int globalIndex2)
    : m_sessionManager(sessionManager), m_globalIndex1(globalIndex1), m_globalIndex2(globalIndex2) {
}

void SwapClipsCommand::execute() {
  if (m_sessionManager && m_globalIndex1 != m_globalIndex2) {
    int originalTab = m_sessionManager->getActiveTab();

    int tab1 = m_globalIndex1 / ::occ::BUTTONS_PER_TAB;
    int btn1 = m_globalIndex1 % ::occ::BUTTONS_PER_TAB;
    int tab2 = m_globalIndex2 / ::occ::BUTTONS_PER_TAB;
    int btn2 = m_globalIndex2 % ::occ::BUTTONS_PER_TAB;

    // Get both clips
    m_sessionManager->setActiveTab(tab1);
    auto clip1 = m_sessionManager->getClip(btn1);

    m_sessionManager->setActiveTab(tab2);
    auto clip2 = m_sessionManager->getClip(btn2);

    // Swap
    m_sessionManager->setActiveTab(tab1);
    if (clip2.isValid()) {
      m_sessionManager->setClip(btn1, clip2);
    } else {
      m_sessionManager->removeClip(btn1);
    }

    m_sessionManager->setActiveTab(tab2);
    if (clip1.isValid()) {
      m_sessionManager->setClip(btn2, clip1);
    } else {
      m_sessionManager->removeClip(btn2);
    }

    m_sessionManager->setActiveTab(originalTab);
  }
}

void SwapClipsCommand::undo() {
  // Swap is symmetric
  execute();
}

juce::String SwapClipsCommand::getDescription() const {
  return "Swap Clips";
}

//==============================================================================
// LoadClipCommand

LoadClipCommand::LoadClipCommand(SessionManager* sessionManager, int tabIndex, int buttonIndex,
                                 const juce::File& audioFile)
    : m_sessionManager(sessionManager), m_tabIndex(tabIndex), m_buttonIndex(buttonIndex),
      m_audioFile(audioFile) {

  // Save current state
  if (m_sessionManager) {
    int originalTab = m_sessionManager->getActiveTab();
    m_sessionManager->setActiveTab(m_tabIndex);
    m_hadPreviousClip = m_sessionManager->hasClip(m_buttonIndex);
    if (m_hadPreviousClip) {
      m_previousData = m_sessionManager->getClip(m_buttonIndex);
    }
    m_sessionManager->setActiveTab(originalTab);
  }
}

void LoadClipCommand::execute() {
  if (m_sessionManager && m_audioFile.existsAsFile()) {
    int originalTab = m_sessionManager->getActiveTab();
    m_sessionManager->setActiveTab(m_tabIndex);
    m_sessionManager->loadClip(m_buttonIndex, m_audioFile.getFullPathName());
    m_sessionManager->setActiveTab(originalTab);
  }
}

void LoadClipCommand::undo() {
  if (m_sessionManager) {
    int originalTab = m_sessionManager->getActiveTab();
    m_sessionManager->setActiveTab(m_tabIndex);

    if (m_hadPreviousClip) {
      m_sessionManager->setClip(m_buttonIndex, m_previousData);
    } else {
      m_sessionManager->removeClip(m_buttonIndex);
    }

    m_sessionManager->setActiveTab(originalTab);
  }
}

juce::String LoadClipCommand::getDescription() const {
  return "Load Clip " + m_audioFile.getFileName();
}

//==============================================================================
// PasteClipCommand

PasteClipCommand::PasteClipCommand(SessionManager* sessionManager, int tabIndex, int buttonIndex,
                                   const SessionManager::ClipData& clipboardData)
    : m_sessionManager(sessionManager), m_tabIndex(tabIndex), m_buttonIndex(buttonIndex),
      m_clipboardData(clipboardData) {
  if (m_sessionManager) {
    int originalTab = m_sessionManager->getActiveTab();
    m_sessionManager->setActiveTab(m_tabIndex);
    m_hadPreviousClip = m_sessionManager->hasClip(m_buttonIndex);
    if (m_hadPreviousClip) {
      m_previousData = m_sessionManager->getClip(m_buttonIndex);
    }
    m_sessionManager->setActiveTab(originalTab);
  }
}

void PasteClipCommand::execute() {
  if (!m_sessionManager)
    return;

  int originalTab = m_sessionManager->getActiveTab();
  m_sessionManager->setActiveTab(m_tabIndex);
  m_sessionManager->setClip(m_buttonIndex, m_clipboardData);
  m_sessionManager->setActiveTab(originalTab);
}

void PasteClipCommand::undo() {
  if (!m_sessionManager)
    return;

  int originalTab = m_sessionManager->getActiveTab();
  m_sessionManager->setActiveTab(m_tabIndex);
  if (m_hadPreviousClip) {
    m_sessionManager->setClip(m_buttonIndex, m_previousData);
  } else {
    m_sessionManager->removeClip(m_buttonIndex);
  }
  m_sessionManager->setActiveTab(originalTab);
}

juce::String PasteClipCommand::getDescription() const {
  return "Paste Clip";
}

size_t PasteClipCommand::getSizeInBytes() const {
  return sizeof(PasteClipCommand);
}

//==============================================================================
// PasteSpecialCommand

PasteSpecialCommand::PasteSpecialCommand(SessionManager* sessionManager,
                                         const std::vector<int>& targetGlobalIndices,
                                         const SessionManager::ClipData& sourceClip,
                                         const PasteSpecialOptions& options)
    : m_sessionManager(sessionManager), m_targetIndices(targetGlobalIndices),
      m_sourceClip(sourceClip), m_options(options) {

  // Save current state of all target clips
  if (m_sessionManager) {
    int originalTab = m_sessionManager->getActiveTab();

    for (int globalIndex : m_targetIndices) {
      int tabIndex = globalIndex / ::occ::BUTTONS_PER_TAB;
      int buttonIndex = globalIndex % ::occ::BUTTONS_PER_TAB;

      m_sessionManager->setActiveTab(tabIndex);
      m_savedClips[globalIndex] = m_sessionManager->getClip(buttonIndex);
    }

    m_sessionManager->setActiveTab(originalTab);
  }
}

void PasteSpecialCommand::execute() {
  if (m_sessionManager) {
    int originalTab = m_sessionManager->getActiveTab();
    int autoFillIndex = 0;

    for (int globalIndex : m_targetIndices) {
      int tabIndex = globalIndex / ::occ::BUTTONS_PER_TAB;
      int buttonIndex = globalIndex % ::occ::BUTTONS_PER_TAB;

      m_sessionManager->setActiveTab(tabIndex);
      auto clipData = m_sessionManager->getClip(buttonIndex);

      applyOptions(clipData, m_sourceClip, autoFillIndex);

      m_sessionManager->setClip(buttonIndex, clipData);
      autoFillIndex++;
    }

    m_sessionManager->setActiveTab(originalTab);
  }
}

void PasteSpecialCommand::undo() {
  if (m_sessionManager) {
    int originalTab = m_sessionManager->getActiveTab();

    for (const auto& [globalIndex, clipData] : m_savedClips) {
      int tabIndex = globalIndex / ::occ::BUTTONS_PER_TAB;
      int buttonIndex = globalIndex % ::occ::BUTTONS_PER_TAB;

      m_sessionManager->setActiveTab(tabIndex);
      m_sessionManager->setClip(buttonIndex, clipData);
    }

    m_sessionManager->setActiveTab(originalTab);
  }
}

juce::String PasteSpecialCommand::getDescription() const {
  return "Paste Special (" + juce::String(m_targetIndices.size()) + " clips)";
}

size_t PasteSpecialCommand::getSizeInBytes() const {
  size_t size = sizeof(*this);
  size += m_targetIndices.size() * sizeof(int);
  size += m_sourceClip.filePath.size() + m_sourceClip.displayName.size();
  for (const auto& [idx, clip] : m_savedClips) {
    size += sizeof(idx) + sizeof(clip) + clip.filePath.size() + clip.displayName.size();
  }
  return size;
}

void PasteSpecialCommand::applyOptions(SessionManager::ClipData& target,
                                       const SessionManager::ClipData& source, int autoFillIndex) {
  // Gain
  if (m_options.gainAbsolute) {
    target.gainDb = source.gainDb;
  } else if (m_options.gainRelative) {
    target.gainDb += m_options.gainRelativeDb;
  }

  // Fades
  if (m_options.fadeIn) {
    target.fadeInSeconds = source.fadeInSeconds;
  }
  if (m_options.fadeInCurve) {
    target.fadeInCurve = source.fadeInCurve;
  }
  if (m_options.fadeOut) {
    target.fadeOutSeconds = source.fadeOutSeconds;
  }
  if (m_options.fadeOutCurve) {
    target.fadeOutCurve = source.fadeOutCurve;
  }

  // Color
  if (m_options.color) {
    target.color = source.color;
  }

  // Clip Group
  if (m_options.clipGroup) {
    target.clipGroup = source.clipGroup;
  }

  // Loop
  if (m_options.loop) {
    target.loopEnabled = source.loopEnabled;
  }

  // Stop Others
  if (m_options.stopOthers) {
    target.stopOthersEnabled = source.stopOthersEnabled;
  }

  // Note: MIDI note assignment would be handled here if ClipData had midiNote field
  // if (m_options.midiNote) {
  //     if (m_options.midiNoteAutoFill) {
  //         target.midiNote = source.midiNote + autoFillIndex;
  //     } else {
  //         target.midiNote = source.midiNote;
  //     }
  // }
  (void)autoFillIndex; // Silence unused warning until MIDI fields added
}

} // namespace orpheus
