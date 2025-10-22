// SPDX-License-Identifier: MIT

#pragma once

#include <functional>
#include <juce_gui_extra/juce_gui_extra.h>

//==============================================================================
/**
 * ClipEditDialog - Modal dialog for editing clip metadata
 *
 * Features (phased implementation):
 * - Phase 1: File metadata (name, color, clip group)
 * - Phase 2: In/Out points with waveform display
 * - Phase 3: Fade In/Out times (0.0s - 3.0s in 0.1s increments)
 * - Phase 4: BPM detection and beatgrid configuration
 * - Phase 5: Time signature (4/4, 3/4, 6/8, 9/8, etc.)
 */
class ClipEditDialog : public juce::Component {
public:
  //==============================================================================
  struct ClipMetadata {
    juce::String displayName;
    juce::String filePath;
    juce::Colour color;
    int clipGroup = 0; // 0-3

    // Phase 2: Trim points (samples)
    int64_t trimInSamples = 0;
    int64_t trimOutSamples = 0;

    // Phase 3: Fade times (seconds)
    double fadeInSeconds = 0.0;
    double fadeOutSeconds = 0.0;
    juce::String fadeInCurve = "Linear"; // Linear, EqualPower, Exponential
    juce::String fadeOutCurve = "Linear";

    // Phase 4: Beat mapping
    double detectedBPM = 0.0;
    bool beatgridEnabled = false;

    // Phase 5: Time signature
    int timeSignatureNumerator = 4;
    int timeSignatureDenominator = 4;

    // Audio file info (read-only)
    int sampleRate = 48000;
    int numChannels = 2;
    int64_t durationSamples = 0;
  };

  //==============================================================================
  ClipEditDialog();
  ~ClipEditDialog() override = default;

  //==============================================================================
  // Set metadata to edit
  void setClipMetadata(const ClipMetadata& metadata);

  // Get edited metadata (call when user clicks OK)
  ClipMetadata getClipMetadata() const {
    return m_metadata;
  }

  //==============================================================================
  // Callbacks
  std::function<void(const ClipMetadata&)> onOkClicked;
  std::function<void()> onCancelClicked;

  //==============================================================================
  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  //==============================================================================
  void buildPhase1UI();       // File metadata
  void buildPhase2UI();       // In/Out points
  void buildPhase3UI();       // Fade times
  void updateTrimInfoLabel(); // Update trim duration display

  //==============================================================================
  ClipMetadata m_metadata;

  // Phase 1: Basic metadata controls
  std::unique_ptr<juce::Label> m_nameLabel;
  std::unique_ptr<juce::TextEditor> m_nameEditor;

  std::unique_ptr<juce::Label> m_colorLabel;
  std::unique_ptr<juce::ComboBox> m_colorComboBox;

  std::unique_ptr<juce::Label> m_groupLabel;
  std::unique_ptr<juce::ComboBox> m_groupComboBox;

  std::unique_ptr<juce::Label> m_filePathLabel;
  std::unique_ptr<juce::TextEditor> m_filePathEditor;

  // Phase 2: In/Out point controls
  std::unique_ptr<juce::Component> m_waveformDisplay;
  std::unique_ptr<juce::Label> m_trimInLabel;
  std::unique_ptr<juce::Slider> m_trimInSlider;
  std::unique_ptr<juce::Label> m_trimOutLabel;
  std::unique_ptr<juce::Slider> m_trimOutSlider;
  std::unique_ptr<juce::Label> m_trimInfoLabel;

  // Phase 3: Fade time controls
  std::unique_ptr<juce::Label> m_fadeInLabel;
  std::unique_ptr<juce::Slider> m_fadeInSlider;
  std::unique_ptr<juce::ComboBox> m_fadeInCurveCombo;

  std::unique_ptr<juce::Label> m_fadeOutLabel;
  std::unique_ptr<juce::Slider> m_fadeOutSlider;
  std::unique_ptr<juce::ComboBox> m_fadeOutCurveCombo;

  // Dialog buttons
  std::unique_ptr<juce::TextButton> m_okButton;
  std::unique_ptr<juce::TextButton> m_cancelButton;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipEditDialog)
};
