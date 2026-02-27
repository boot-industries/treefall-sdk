/*
  ==============================================================================

    Command.h
    Created: 27 Nov 2025
    Author:  Orpheus Clip Composer

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>

namespace orpheus {

/**
    Base interface for all undoable commands in the application.
    Follows the GoF Command Pattern.
*/
class Command {
public:
  virtual ~Command() = default;

  /** Executes the command logic.
      Called immediately when the command is pushed to the UndoManager,
      and again during redo operations.
  */
  virtual void execute() = 0;

  /** Reverts the command logic.
      Called during undo operations.
  */
  virtual void undo() = 0;

  /** Returns a human-readable description of the command.
      Used for menu items (e.g. "Undo Delete Clip").
  */
  virtual juce::String getDescription() const = 0;

  /** Returns the approximate memory footprint of this command in bytes.
      Used by UndoManager to manage memory usage if needed.
  */
  virtual size_t getSizeInBytes() const {
    return sizeof(*this);
  }
};

} // namespace orpheus
