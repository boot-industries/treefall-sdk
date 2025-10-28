// SPDX-License-Identifier: MIT

#pragma once

#include "PreviewPlayer.h"
#include "WaveformDisplay.h"
#include <functional>
#include <juce_gui_extra/juce_gui_extra.h>

// Forward declaration
class AudioEngine;

//==============================================================================
/**
 * ClipEditDialog - Modal dialog for editing clip metadata
 *
 * ARCHITECTURE CHANGE (v0.2.0):
 * - PreviewPlayer now uses Cue Buss instead of separate audio device
 * - Requires AudioEngine reference for Cue Buss allocation
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

    // Loop state (synced between Edit Dialog and Clip Button)
    bool loopEnabled = false;
    bool stopOthersEnabled = false;

    // Playback handoff (continuity between main grid and Edit Dialog)
    bool wasPlayingOnMainGrid = false;  // true if clip was playing when Edit Dialog opened
    int64_t playheadPositionAtOpen = 0; // Playhead position in samples when dialog opened

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
  ClipEditDialog(AudioEngine* audioEngine, int buttonIndex);
  ~ClipEditDialog() override;

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
  bool keyPressed(const juce::KeyPress& key) override;

private:
  //==============================================================================
  void buildPhase1UI();       // File metadata
  void buildPhase2UI();       // In/Out points
  void buildPhase3UI();       // Fade times
  void updateTrimInfoLabel(); // Update trim duration display
  juce::String samplesToTimeString(int64_t samples,
                                   int sampleRate); // Convert samples to hh:mm:ss.tt
  int64_t timeStringToSamples(const juce::String& timeStr,
                              int sampleRate); // Convert hh:mm:ss.tt to samples

  //==============================================================================
  ClipMetadata m_metadata;
  AudioEngine* m_audioEngine = nullptr; // Non-owning reference
  int m_buttonIndex = -1;               // Button index (0-47) of main grid clip

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
  std::unique_ptr<WaveformDisplay> m_waveformDisplay;
  std::unique_ptr<juce::TextButton> m_zoomOutButton;
  std::unique_ptr<juce::Label> m_zoomLabel;
  std::unique_ptr<juce::TextButton> m_zoomInButton;

  void updateZoomLabel(); // Update zoom level display

  // Preview transport controls (professional icon buttons)
  std::unique_ptr<juce::DrawableButton> m_skipToStartButton;
  std::unique_ptr<juce::DrawableButton> m_playButton;
  std::unique_ptr<juce::DrawableButton> m_stopButton;
  std::unique_ptr<juce::DrawableButton> m_skipToEndButton;
  std::unique_ptr<juce::DrawableButton> m_loopButton;
  std::unique_ptr<juce::ToggleButton> m_stopOthersButton;
  std::unique_ptr<juce::Label> m_transportPositionLabel;

  // Preview audio player
  std::unique_ptr<PreviewPlayer> m_previewPlayer;
  std::unique_ptr<juce::Label> m_trimInLabel;
  std::unique_ptr<juce::TextEditor> m_trimInTimeEditor;
  std::unique_ptr<juce::TextButton> m_trimInDecButton;
  std::unique_ptr<juce::TextButton> m_trimInIncButton;
  std::unique_ptr<juce::TextButton> m_trimInHoldButton;
  std::unique_ptr<juce::TextButton> m_trimInClearButton;
  std::unique_ptr<juce::Label> m_trimOutLabel;
  std::unique_ptr<juce::TextEditor> m_trimOutTimeEditor;
  std::unique_ptr<juce::TextButton> m_trimOutDecButton;
  std::unique_ptr<juce::TextButton> m_trimOutIncButton;
  std::unique_ptr<juce::TextButton> m_trimOutHoldButton;
  std::unique_ptr<juce::TextButton> m_trimOutClearButton;
  std::unique_ptr<juce::Label> m_trimInfoLabel;

  // File info panel
  std::unique_ptr<juce::Label> m_fileInfoPanel;

  // Phase 3: Fade time controls
  std::unique_ptr<juce::Label> m_fadeInLabel;
  std::unique_ptr<juce::ComboBox> m_fadeInCombo;
  std::unique_ptr<juce::ComboBox> m_fadeInCurveCombo;

  std::unique_ptr<juce::Label> m_fadeOutLabel;
  std::unique_ptr<juce::ComboBox> m_fadeOutCombo;
  std::unique_ptr<juce::ComboBox> m_fadeOutCurveCombo;

  // Dialog buttons
  std::unique_ptr<juce::TextButton> m_okButton;
  std::unique_ptr<juce::TextButton> m_cancelButton;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipEditDialog)
};
