/*
  ==============================================================================

    Interpolation.h
    Created: Shmui-to-JUCE Audio Visualization Port

    Interpolation and smoothing utilities for animation.
    Contains the tuned constants from shmui for consistent visual behavior.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <cmath>

namespace shmui
{

/**
 * @brief Interpolation and smoothing utilities for visualization animations.
 *
 * Provides exponential smoothing, easing functions, and other interpolation
 * utilities translated from shmui's animation system.
 */
namespace Interpolation
{

//==============================================================================
// Tuned Animation Constants from shmui

/** Exponential smoothing factor for volume/level */
constexpr float kVolumeSmoothingFactor = 0.2f;

/** Transition step per frame for state changes */
constexpr float kTransitionStep = 0.02f;

/** Fade step per frame for fade-out effects */
constexpr float kFadeStep = 0.03f;

/** Color lerp factor for smooth color transitions */
constexpr float kColorLerpFactor = 0.08f;

/** Animation speed range for orb */
constexpr float kMinAnimSpeed = 0.1f;
constexpr float kMaxAnimSpeed = 1.0f;

//==============================================================================
// Smoothing Functions

/**
 * @brief Apply exponential smoothing to a value.
 *
 * This is the core smoothing function used throughout shmui:
 * current += (target - current) * factor
 *
 * @param current Current value
 * @param target Target value to approach
 * @param factor Smoothing factor (0-1, higher = faster approach)
 * @return Smoothed value
 */
inline float smooth(float current, float target, float factor = kVolumeSmoothingFactor)
{
    return current + (target - current) * factor;
}

/**
 * @brief Apply frame-rate independent exponential smoothing.
 *
 * Adjusts smoothing based on delta time for consistent behavior
 * regardless of frame rate.
 *
 * @param current Current value
 * @param target Target value
 * @param factor Base smoothing factor
 * @param deltaTime Time since last frame in seconds
 * @param targetFps Target frame rate for reference (default 60)
 * @return Smoothed value
 */
inline float smoothDelta(float current, float target, float factor,
                        float deltaTime, float targetFps = 60.0f)
{
    // Adjust factor based on actual frame time vs target
    const float adjustedFactor = 1.0f - std::pow(1.0f - factor, deltaTime * targetFps);
    return current + (target - current) * adjustedFactor;
}

//==============================================================================
// Easing Functions

/**
 * @brief Linear interpolation between two values.
 *
 * @param a Start value
 * @param b End value
 * @param t Interpolation factor (0-1)
 * @return Interpolated value
 */
inline float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

/**
 * @brief Ease-in quadratic.
 */
inline float easeInQuad(float t)
{
    return t * t;
}

/**
 * @brief Ease-out quadratic.
 */
inline float easeOutQuad(float t)
{
    return t * (2.0f - t);
}

/**
 * @brief Ease-in-out quadratic.
 */
inline float easeInOutQuad(float t)
{
    return t < 0.5f
           ? 2.0f * t * t
           : -1.0f + (4.0f - 2.0f * t) * t;
}

/**
 * @brief Ease-in-out cubic.
 */
inline float easeInOutCubic(float t)
{
    return t < 0.5f
           ? 4.0f * t * t * t
           : (t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f;
}

/**
 * @brief Smooth step function (Hermite interpolation).
 */
inline float smoothstep(float edge0, float edge1, float x)
{
    const float t = juce::jlimit(0.0f, 1.0f, (x - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

/**
 * @brief Smoother step function (Ken Perlin's improved version).
 */
inline float smootherstep(float edge0, float edge1, float x)
{
    const float t = juce::jlimit(0.0f, 1.0f, (x - edge0) / (edge1 - edge0));
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

//==============================================================================
// Clamping Utilities

/**
 * @brief Clamp value to 0-1 range.
 */
inline float clamp01(float value)
{
    if (!std::isfinite(value))
        return 0.0f;
    return juce::jlimit(0.0f, 1.0f, value);
}

/**
 * @brief Map a value from one range to another.
 *
 * @param value Input value
 * @param inMin Input range minimum
 * @param inMax Input range maximum
 * @param outMin Output range minimum
 * @param outMax Output range maximum
 * @return Mapped value
 */
inline float mapRange(float value, float inMin, float inMax, float outMin, float outMax)
{
    return outMin + (outMax - outMin) * ((value - inMin) / (inMax - inMin));
}

//==============================================================================
// Wave Functions

/**
 * @brief Sine wave oscillation.
 *
 * @param time Current time
 * @param frequency Oscillation frequency
 * @param amplitude Wave amplitude
 * @param offset Vertical offset
 * @return Oscillating value
 */
inline float sineWave(float time, float frequency, float amplitude = 1.0f, float offset = 0.0f)
{
    return std::sin(time * frequency) * amplitude + offset;
}

/**
 * @brief Cosine wave oscillation.
 */
inline float cosineWave(float time, float frequency, float amplitude = 1.0f, float offset = 0.0f)
{
    return std::cos(time * frequency) * amplitude + offset;
}

//==============================================================================
// Seeded Random Number Generator

/**
 * @brief Splitmix32 pseudo-random number generator.
 *
 * Same algorithm used in orb.tsx for deterministic random values.
 */
class SeedRandom
{
public:
    explicit SeedRandom(uint32_t seed) : state(seed) {}

    /** Generate next random value in 0-1 range */
    float next()
    {
        state = (state + 0x9e3779b9);
        uint32_t t = state ^ (state >> 16);
        t = t * 0x21f0aaad;
        t = t ^ (t >> 15);
        t = t * 0x735a2d97;
        t = t ^ (t >> 15);
        return static_cast<float>(t) / static_cast<float>(0xFFFFFFFF);
    }

    /** Reset with new seed */
    void reset(uint32_t seed)
    {
        state = seed;
    }

private:
    uint32_t state;
};

/**
 * @brief Simple seeded random function (stateless).
 *
 * Uses sine-based pseudo-random as in ScrollingWaveform.
 *
 * @param seed Input seed value
 * @return Pseudo-random value 0-1
 */
inline float seededRandom(float seed)
{
    const float x = std::sin(seed * 10000.0f) * 10000.0f;
    return x - std::floor(x);
}

} // namespace Interpolation

} // namespace shmui
