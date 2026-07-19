/*
  ==============================================================================

    TransportContainer.h
    Created: shmui Component Library

    Slot-based, composable transport strip.

    Where TransportBar is a batteries-included DAW transport (play/stop/record/
    loop + musical time), TransportContainer is an empty three-region strip an
    app fills with its OWN buttons and readouts — Stop-All, Panic, Cue/PFL,
    latency/CPU, "now playing" — for broadcast/console layouts that are
    emergency/diagnostic rather than musical (OCC153 G8). TransportBar is left
    intact; this is additive.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "TransportBar.h"     // reuse TransportBarStyle
#include <vector>

namespace shmui
{

//==============================================================================
/**
 * @brief A composable transport strip with Left / Center / Right regions.
 *
 * The app owns the child components; the container only lays them out (via a
 * FlexBox per region) and paints the shared background/separators. Add items
 * left-to-right within a region; use addSpacer() for fixed gaps.
 *
 * Ownership: added components are NOT owned — the caller keeps them alive
 * (matching typical JUCE parent/child usage where children are members).
 */
class TransportContainer : public juce::Component
{
public:
    //==============================================================================
    enum class Region { Left, Center, Right };

    TransportContainer();
    ~TransportContainer() override = default;

    //==============================================================================
    /**
     * @brief Add a child component to a region.
     * @param region     which zone (Left/Center/Right)
     * @param child      component to place (not owned; must outlive the container)
     * @param flexWidth  fixed width in px, or -1 for intrinsic (uses child's width)
     */
    void addItem(Region region, juce::Component* child, int flexWidth = -1);

    /** Add a fixed-width spacer to a region. */
    void addSpacer(Region region, int px);

    /** Remove all items from all regions (does not delete the components). */
    void clearItems();

    //==============================================================================
    /** Set the shared style (background / separator / dimensions). */
    void setStyle(const TransportBarStyle& style);
    const TransportBarStyle& getStyle() const { return m_style; }

    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    //==============================================================================
    struct Item
    {
        juce::Component* component = nullptr;   // null => spacer
        int flexWidth = -1;                     // fixed px, or -1 intrinsic
    };

    std::vector<Item>& regionItems(Region region);
    void layoutRegion(juce::FlexBox& box, std::vector<Item>& items);

    std::vector<Item> m_left, m_center, m_right;
    TransportBarStyle m_style;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportContainer)
};

} // namespace shmui
