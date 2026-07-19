/*
  ==============================================================================

    AgentState.h
    Created: Shmui-to-JUCE Audio Visualization Port

    Unified agent state enum for all visualization components.
    Ensures consistent state naming across BarVisualizer, OrbVisualizer, etc.

  ==============================================================================
*/

#pragma once

namespace shmui
{

/**
 * @brief Unified agent state for all visualization components.
 *
 * Represents the current state of an AI/voice assistant.
 * Used by BarVisualizer, OrbVisualizer, and other components
 * to drive state-based animations.
 *
 * State mapping to React:
 * - Idle         -> undefined / null
 * - Connecting   -> "connecting"
 * - Initializing -> "initializing"
 * - Listening    -> "listening"
 * - Thinking     -> "thinking"
 * - Speaking     -> "speaking" / "talking"
 */
enum class AgentState
{
    Idle,           ///< Inactive state (no activity)
    Connecting,     ///< Initial connection animation
    Initializing,   ///< Setup/initialization animation
    Listening,      ///< Listening for user input
    Thinking,       ///< Processing/thinking
    Speaking        ///< Output/speaking/talking
};

/**
 * @brief Convert AgentState to string for debugging.
 */
inline const char* agentStateToString(AgentState state)
{
    switch (state)
    {
        case AgentState::Idle:         return "idle";
        case AgentState::Connecting:   return "connecting";
        case AgentState::Initializing: return "initializing";
        case AgentState::Listening:    return "listening";
        case AgentState::Thinking:     return "thinking";
        case AgentState::Speaking:     return "speaking";
        default:                       return "unknown";
    }
}

} // namespace shmui
