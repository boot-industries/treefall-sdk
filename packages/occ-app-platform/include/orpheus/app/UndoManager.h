/*
  ==============================================================================

    UndoManager.h
    Created: 27 Nov 2025
    Author:  Orpheus Clip Composer

  ==============================================================================
*/

#pragma once

#include <functional>
#include <memory>
#include <orpheus/app/Command.h>
#include <vector>

namespace orpheus {

/**
    Manages the history of undoable commands.
    Supports execute, undo, redo, and history limiting.
    Not thread-safe; intended for use on the Message Thread.
*/
class UndoManager {
public:
  explicit UndoManager(size_t maxDepth = 100);
  ~UndoManager() = default;

  /** Executes a command and pushes it onto the undo stack.
      Clears the redo stack.
  */
  void executeCommand(std::unique_ptr<Command> command);

  /** Undoes the last command. */
  void undo();

  /** Redoes the previously undone command. */
  void redo();

  /** Checks if undo is possible. */
  bool canUndo() const;

  /** Checks if redo is possible. */
  bool canRedo() const;

  /** Returns the command that undo() would apply, without advancing history. */
  const Command* peekUndoCommand() const noexcept;

  /** Returns the command that redo() would apply, without advancing history. */
  const Command* peekRedoCommand() const noexcept;

  /** Returns description of the next undoable action. */
  juce::String getUndoDescription() const;

  /** Returns description of the next redoable action. */
  juce::String getRedoDescription() const;

  /** Clears all history (e.g. on new session). */
  void clear();

  /** Sets the maximum history depth. */
  void setMaxDepth(size_t depth);

  /** Callback for when history changes (for UI updates). */
  std::function<void()> onHistoryChanged;

private:
  std::vector<std::unique_ptr<Command>> m_history;
  size_t m_currentIndex = 0; // Points to the slot where the NEXT command will be inserted
                             // (i.e. size() if no undo has happened)
  size_t m_maxDepth;

  void trimHistory();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UndoManager)
};

} // namespace orpheus
