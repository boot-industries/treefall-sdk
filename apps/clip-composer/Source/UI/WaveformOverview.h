// SPDX-License-Identifier: MIT
//
// WaveformOverview — full-file minimap that sits above the main zoomed
// WaveformDisplay in the Clip Edit dialog. Matches the design-kit spec
// (orpheus_design-system_2605/preview/components-waveform.html) for the
// "overview minimap":
//   * 24 px height, rounded 4 px corners, lab-surface-1 bg, lab-stroke border.
//   * Faint blue waveform line (#4a9eff at 0.5 alpha).
//   * Translucent green viewport window — shows what region of the file the
//     main waveform is zoomed into.
//   * Yellow playhead tick.
//   * Magenta IN / cyan OUT trim ticks.
//
// Operator narrative: the sound designer needs to see "where am I in this
// 4-minute file" at a glance, even when the main waveform is zoomed to a
// 22-second window. The minimap is read-only here (no scrubbing — that
// would couple it to the main waveform's zoom logic, deferred to a future
// pass: TODO(occ149c-minimap-scrub)).

#pragma once

#include "DesignTokens.h"
#include <atomic>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

class WaveformOverview : public juce::Component {
public:
  WaveformOverview() {
    setInterceptsMouseClicks(false, false); // Passive minimap - don't intercept mouse events
  }

  /** Set the audio file to render. WaveformOverview holds a downsampled
   *  copy of the file's peak data so paint() can run cheaply on the UI
   *  thread. */
  void setAudioFile(juce::AudioFormatReader* reader) {
    m_peaks.clear();
    if (reader == nullptr || reader->lengthInSamples == 0) {
      m_durationSamples = 0;
      m_sampleRate = 0;
      repaint();
      return;
    }
    m_durationSamples = reader->lengthInSamples;
    m_sampleRate = reader->sampleRate;

    // Downsample to ~800 peak buckets — matches the kit's 800-unit viewBox.
    constexpr int kBuckets = 800;
    m_peaks.resize(kBuckets, 0.0f);
    const int64_t samplesPerBucket = std::max<int64_t>(int64_t{1}, m_durationSamples / kBuckets);
    constexpr int kReadBlock = 8192;
    const int channels = reader->numChannels > 0 ? static_cast<int>(reader->numChannels) : 1;
    juce::AudioBuffer<float> buffer(channels, kReadBlock);

    for (int b = 0; b < kBuckets; ++b) {
      const int64_t startSample = b * samplesPerBucket;
      int64_t remaining = std::min<int64_t>(samplesPerBucket, m_durationSamples - startSample);
      if (remaining <= 0)
        break;

      float peak = 0.0f;
      int64_t cursor = startSample;
      while (remaining > 0) {
        const int chunk = static_cast<int>(std::min<int64_t>(int64_t{kReadBlock}, remaining));
        reader->read(&buffer, 0, chunk, cursor, true, true);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
          const float* data = buffer.getReadPointer(ch);
          for (int i = 0; i < chunk; ++i) {
            const float v = std::abs(data[i]);
            if (v > peak)
              peak = v;
          }
        }
        cursor += chunk;
        remaining -= chunk;
      }
      m_peaks[static_cast<size_t>(b)] = peak;
    }
    repaint();
  }

  void clearAudio() {
    m_peaks.clear();
    m_durationSamples = 0;
    m_sampleRate = 0;
    repaint();
  }

  /** Set the playhead position in samples. */
  void setPlayheadSamples(int64_t samples) {
    int64_t clamped = std::max<int64_t>(int64_t{0}, std::min<int64_t>(m_durationSamples, samples));
    if (m_playheadSamples != clamped) {
      m_playheadSamples = clamped;
      repaint();
    }
  }

  /** Set IN / OUT trim points in samples. */
  void setTrimSamples(int64_t trimIn, int64_t trimOut) {
    bool changed = false;
    if (m_trimInSamples != trimIn) {
      m_trimInSamples = trimIn;
      changed = true;
    }
    if (m_trimOutSamples != trimOut) {
      m_trimOutSamples = trimOut;
      changed = true;
    }
    if (changed)
      repaint();
  }

  /** Set the viewport window (the region currently visible in the main
   *  zoomed WaveformDisplay). Expressed as start/end sample positions. */
  void setViewportSamples(int64_t startSample, int64_t endSample) {
    bool changed = false;
    if (m_viewportStartSamples != startSample) {
      m_viewportStartSamples = startSample;
      changed = true;
    }
    if (m_viewportEndSamples != endSample) {
      m_viewportEndSamples = endSample;
      changed = true;
    }
    if (changed)
      repaint();
  }

  void paint(juce::Graphics& g) override {
    using namespace OCC::Design;
    auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty())
      return;

    constexpr float kRadius = 4.0f;

    // Card: lab-surface-1 over the dialog chassis, hairline border.
    g.setColour(juce::Colour(0xff0f1f23)); // matches the design kit lab-surface-1
    g.fillRoundedRectangle(bounds, kRadius);
    g.setColour(juce::Colour(kBorderDefault));
    g.drawRoundedRectangle(bounds.reduced(0.5f), kRadius, 1.0f);

    if (m_durationSamples <= 0 || m_peaks.empty())
      return;

    auto interior = bounds.reduced(1.0f);
    const float w = interior.getWidth();
    const float h = interior.getHeight();
    const float midY = interior.getY() + h * 0.5f;

    // Faint blue waveform line (peaks above + reflected below the midline).
    juce::Path wave;
    wave.startNewSubPath(interior.getX(), midY);
    const size_t bucketCount = m_peaks.size();
    for (size_t i = 0; i < bucketCount; ++i) {
      const float x = interior.getX() + (w * static_cast<float>(i) / bucketCount);
      const float peak = m_peaks[i];
      const float y = midY - peak * (h * 0.45f);
      wave.lineTo(x, y);
    }
    for (size_t i = bucketCount; i-- > 0;) {
      const float x = interior.getX() + (w * static_cast<float>(i) / bucketCount);
      const float peak = m_peaks[i];
      const float y = midY + peak * (h * 0.45f);
      wave.lineTo(x, y);
    }
    wave.closeSubPath();
    g.setColour(juce::Colour(Waveform::kWaveformBlue).withAlpha(0.50f));
    g.fillPath(wave);

    // Viewport window — translucent green box + tone-coloured vertical bars.
    if (m_viewportEndSamples > m_viewportStartSamples) {
      const float vx0 =
          interior.getX() + w * (static_cast<float>(m_viewportStartSamples) / m_durationSamples);
      const float vx1 =
          interior.getX() + w * (static_cast<float>(m_viewportEndSamples) / m_durationSamples);
      auto window = juce::Rectangle<float>(vx0, interior.getY() - 1.0f, vx1 - vx0, h + 2.0f);
      g.setColour(juce::Colour(0x2619e0b3)); // lab-tone @ 0.15
      g.fillRect(window);
      g.setColour(juce::Colour(0xff19e0b3));
      g.drawLine(vx0, interior.getY() - 1.0f, vx0, interior.getBottom() + 1.0f, 2.0f);
      g.drawLine(vx1, interior.getY() - 1.0f, vx1, interior.getBottom() + 1.0f, 2.0f);
    }

    // IN trim tick.
    if (m_trimInSamples > 0 && m_trimInSamples < m_durationSamples) {
      const float x =
          interior.getX() + w * (static_cast<float>(m_trimInSamples) / m_durationSamples);
      g.setColour(juce::Colour(Waveform::kTrimInMagenta).withAlpha(0.9f));
      g.drawLine(x, interior.getY() + 2.0f, x, interior.getBottom() - 2.0f, 1.0f);
    }

    // OUT trim tick.
    if (m_trimOutSamples > 0 && m_trimOutSamples < m_durationSamples) {
      const float x =
          interior.getX() + w * (static_cast<float>(m_trimOutSamples) / m_durationSamples);
      g.setColour(juce::Colour(Waveform::kTrimOutCyan).withAlpha(0.9f));
      g.drawLine(x, interior.getY() + 2.0f, x, interior.getBottom() - 2.0f, 1.0f);
    }

    // Playhead — full-height yellow line with soft glow.
    if (m_playheadSamples > 0 && m_playheadSamples <= m_durationSamples) {
      const float x =
          interior.getX() + w * (static_cast<float>(m_playheadSamples) / m_durationSamples);
      g.setColour(juce::Colour(Waveform::kPlayheadYellow).withAlpha(0.4f));
      g.drawLine(x, interior.getY(), x, interior.getBottom(), 3.0f);
      g.setColour(juce::Colour(Waveform::kPlayheadYellow));
      g.drawLine(x, interior.getY(), x, interior.getBottom(), 1.0f);
    }
  }

private:
  std::vector<float> m_peaks;
  int64_t m_durationSamples = 0;
  double m_sampleRate = 0.0;
  int64_t m_playheadSamples = 0;
  int64_t m_trimInSamples = 0;
  int64_t m_trimOutSamples = 0;
  int64_t m_viewportStartSamples = 0;
  int64_t m_viewportEndSamples = 0;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformOverview)
};
