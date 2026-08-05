/*
  ==============================================================================

    MessageThread.h
    Small guard for JUCE component setters that own UI state.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace shmui {

/**
 * Return whether the caller is on JUCE's message thread.
 *
 * A missing MessageManager is treated as the single-threaded bootstrap case;
 * once JUCE has created one, off-thread callers are rejected by
 * requireMessageThread().
 */
inline bool isMessageThread() noexcept {
  if (auto* manager = juce::MessageManager::getInstanceWithoutCreating())
    return manager->isThisTheMessageThread();

  return true;
}

/**
 * Assert and reject an off-thread UI mutation.
 */
inline bool requireMessageThread() noexcept {
  const bool onMessageThread = isMessageThread();
  jassert(onMessageThread);
  return onMessageThread;
}

} // namespace shmui
