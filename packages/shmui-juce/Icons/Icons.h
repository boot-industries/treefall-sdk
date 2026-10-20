/*
  ==============================================================================

    Icons.h
    Created: shmui Icon Library

    SVG path-based icon library for JUCE applications.
    Like Lucide/Heroicons for desktop audio apps.

    Usage:
      juce::Path icon = shmui::Icons::getIcon(shmui::IconType::Play, 24.0f);
      shmui::Icons::drawIcon(g, shmui::IconType::Play, bounds, juce::Colours::white);

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace shmui {

//==============================================================================
/**
 * @brief Icon type enumeration for all available icons.
 */
enum class IconType {
  // Transport icons
  Play,
  Pause,
  Stop,
  Record,
  FastForward,
  Rewind,
  SkipNext,
  SkipPrev,
  Loop,

  // Audio icons
  VolumeHigh,
  VolumeMid,
  VolumeLow,
  VolumeMute,
  Microphone,
  Speaker,
  Headphones,
  Waveform,

  // Mixer icons
  Solo,
  Mute,
  Bypass,
  Pan,
  Fader,

  // File icons
  Folder,
  FolderOpen,
  File,
  FileAudio,
  Import,
  Export,
  Save,
  Load,

  // Edit icons
  Cut,
  Copy,
  Paste,
  Undo,
  Redo,
  Delete,
  Duplicate,
  Edit,

  // UI icons
  Menu,
  Close,
  Minimize,
  Maximize,
  Settings,
  Search,
  Filter,
  Plus,
  Minus,

  // Arrow icons
  ChevronUp,
  ChevronDown,
  ChevronLeft,
  ChevronRight,
  ArrowUp,
  ArrowDown,
  ArrowLeft,
  ArrowRight,

  // Status icons
  Check,
  X,
  Warning,
  Info,
  Error,
  Loading,
  Clock,

  // Console lamp and hotkey-legend icons (SHM027).
  // These are appended before NumIcons so that every existing enumerator
  // keeps its original numeric value.
  StopPeers,
  FadeIn,
  FadeOut,
  Shift,

  // Count (for iteration)
  NumIcons
};

//==============================================================================
/**
 * @brief How an icon's geometry is painted.
 */
enum class IconRenderMode {
  Filled, ///< Closed geometry painted with the destination colour.
  Stroked ///< Open geometry painted with a fixed-width stroke.
};

//==============================================================================
/**
 * @brief The rendering contract for one icon at one size.
 */
struct IconRenderSpec {
  /** @brief Whether the geometry is filled or stroked. */
  IconRenderMode mode{IconRenderMode::Filled};

  /** @brief Stroke width in logical pixels; only meaningful for Stroked mode. */
  float strokeWidth{0.0f};
};

//==============================================================================
/**
 * @brief Icon rendering utilities.
 */
namespace Icons {

/**
 * @brief Get the path data for an icon at a specific size.
 *
 * @param type Icon type to retrieve
 * @param size Icon size in pixels (default 24)
 * @return juce::Path containing the icon geometry
 */
juce::Path getIcon(IconType type, float size = 24.0f);

/**
 * @brief Draw an icon to a graphics context.
 *
 * @param g Graphics context to draw to
 * @param type Icon type to draw
 * @param bounds Bounding rectangle for the icon
 * @param colour Fill colour for the icon
 * @param strokeWidth Optional explicit stroke width in the same units as @p bounds.
 *                    When greater than zero the icon is stroked with that width.
 *                    When zero (the default) the icon is painted with its own
 *                    render mode: filled icons are filled, and the console
 *                    open-geometry icons are stroked with the console stroke
 *                    policy for the surface size.
 */
void drawIcon(juce::Graphics& g, IconType type, juce::Rectangle<float> bounds, juce::Colour colour,
              float strokeWidth = 0.0f);

/**
 * @brief The rendering contract for an icon at a given size.
 *
 * @param type Icon type
 * @param size Icon size in the same units as the drawing surface
 * @return The mode and stroke width the shared layer uses for that icon
 */
IconRenderSpec getIconRenderSpec(IconType type, float size);

/**
 * @brief The console stroke policy.
 *
 * The console open-geometry icons (Loop, Stop Peers) are stroked with a fixed
 * logical-pixel weight that depends only on the size of the surface they are
 * drawn on, so 24-grid artwork never collapses into a hairline at lamp size.
 *
 * @param size Icon size in logical pixels
 * @return The stroke width in logical pixels
 */
float getConsoleStrokeWidth(float size);

/**
 * @brief Get the name string for an icon type.
 *
 * @param type Icon type
 * @return Human-readable icon name
 */
juce::String getIconName(IconType type);

/**
 * @brief Standard icon sizes.
 */
namespace Sizes {
constexpr float XSmall = 16.0f;
constexpr float Small = 20.0f;
constexpr float Medium = 24.0f;
constexpr float Large = 32.0f;
constexpr float XLarge = 48.0f;
} // namespace Sizes

} // namespace Icons

} // namespace shmui
