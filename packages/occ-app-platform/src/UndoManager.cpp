/*
  ==============================================================================

    UndoManager.cpp
    Created: 27 Nov 2025
    Author:  Orpheus Clip Composer

  ==============================================================================
*/

#include <orpheus/app/UndoManager.h>

namespace orpheus {

UndoManager::UndoManager(size_t maxDepth) : m_maxDepth(maxDepth) {}

void UndoManager::executeCommand(std::unique_ptr<Command> command) {
  if (!command)
    return;

  // If we are in the middle of the stack (after undoing), clear the redo history
  if (m_currentIndex < m_history.size()) {
    m_history.erase(m_history.begin() + m_currentIndex, m_history.end());
  }

  // Execute the command
  command->execute();

  // Push to history
  m_history.push_back(std::move(command));
  m_currentIndex++;

  // Enforce limit
  trimHistory();

  if (onHistoryChanged)
    onHistoryChanged();
}

void UndoManager::undo() {
  if (!canUndo())
    return;

  m_currentIndex--;
  m_history[m_currentIndex]->undo();

  if (onHistoryChanged)
    onHistoryChanged();
}

void UndoManager::redo() {
  if (!canRedo())
    return;

  m_history[m_currentIndex]->execute();
  m_currentIndex++;

  if (onHistoryChanged)
    onHistoryChanged();
}

bool UndoManager::canUndo() const {
  return m_currentIndex > 0;
}

bool UndoManager::canRedo() const {
  return m_currentIndex < m_history.size();
}

const Command* UndoManager::peekUndoCommand() const noexcept {
  return canUndo() ? m_history[m_currentIndex - 1].get() : nullptr;
}

const Command* UndoManager::peekRedoCommand() const noexcept {
  return canRedo() ? m_history[m_currentIndex].get() : nullptr;
}

juce::String UndoManager::getUndoDescription() const {
  if (canUndo())
    return "Undo " + m_history[m_currentIndex - 1]->getDescription();

  return "Undo";
}

juce::String UndoManager::getRedoDescription() const {
  if (canRedo())
    return "Redo " + m_history[m_currentIndex]->getDescription();

  return "Redo";
}

void UndoManager::clear() {
  m_history.clear();
  m_currentIndex = 0;

  if (onHistoryChanged)
    onHistoryChanged();
}

void UndoManager::setMaxDepth(size_t depth) {
  m_maxDepth = depth;
  trimHistory();
}

void UndoManager::trimHistory() {
  if (m_history.size() <= m_maxDepth)
    return;

  // Remove oldest commands
  size_t removeCount = m_history.size() - m_maxDepth;

  // If current index is within the removed range, adjust it
  if (m_currentIndex < removeCount)
    m_currentIndex = 0;
  else
    m_currentIndex -= removeCount;

  m_history.erase(m_history.begin(), m_history.begin() + removeCount);
}

} // namespace orpheus
