#include "Audio/AudioAnalyzer.h"
#include "Components/BarVisualizer.h"
#include "Components/LevelMeter.h"
#include "Components/WaveformVisualizer.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <memory>
#include <vector>

namespace {
int failures = 0;

void require(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::fprintf(stderr, "SHM014 failure: %s\n", message);
  }
}

void requireFiniteNormalized(const std::vector<float>& values, const char* message) {
  for (const float value : values) {
    if (!std::isfinite(value) || value < 0.0f || value > 1.0f) {
      require(false, message);
      return;
    }
  }
}

bool pumpUntil(const std::function<bool()>& condition, int timeoutMs) {
  const auto deadline = juce::Time::getMillisecondCounterHiRes() + static_cast<double>(timeoutMs);
  while (juce::Time::getMillisecondCounterHiRes() < deadline) {
    if (condition())
      return true;

    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
  }

  return condition();
}

void pumpFor(int durationMs) {
  (void)pumpUntil([] { return false; }, durationMs);
}

void requireLevelMeterFollowsAncestorShowingState() {
  juce::Component host;
  shmui::LevelMeter meter;
  host.setBounds(0, 0, 120, 240);
  host.addToDesktop(juce::ComponentPeer::windowIsTemporary);
  host.setVisible(false);
  host.addAndMakeVisible(meter);
  meter.setBounds(host.getLocalBounds());
  meter.enableHistory(4);
  meter.setLevel(0, 1.1f);

  host.setVisible(true);
  require(pumpUntil([&meter] { return meter.getHistorySize() > 0; }, 500),
          "meter added under a hidden host starts when the host is shown");

  host.setVisible(false);
  meter.reset();
  meter.clearHistory();
  meter.setLevel(0, 1.1f);
  pumpFor(150);
  require(meter.getHistorySize() == 0 && !meter.hasClipped(),
          "meter remains idle while an ancestor is hidden");

  host.setVisible(true);
  require(pumpUntil([&meter] { return meter.getHistorySize() > 0; }, 500),
          "meter resumes when the hidden ancestor is shown");

  host.removeFromDesktop();
}

int colourDistance(juce::Colour left, juce::Colour right) {
  return std::abs(static_cast<int>(left.getRed()) - static_cast<int>(right.getRed())) +
         std::abs(static_cast<int>(left.getGreen()) - static_cast<int>(right.getGreen())) +
         std::abs(static_cast<int>(left.getBlue()) - static_cast<int>(right.getBlue()));
}

void requireColourNear(juce::Colour actual, juce::Colour expected, const char* message) {
  if (colourDistance(actual, expected) <= 12)
    return;

  ++failures;
  std::fprintf(stderr, "SHM014 failure: %s (actual #%02X%02X%02X, expected #%02X%02X%02X)\n",
               message, actual.getRed(), actual.getGreen(), actual.getBlue(), expected.getRed(),
               expected.getGreen(), expected.getBlue());
}

void requireLevelMeterUsesThresholdGradient(bool vertical) {
  const int width = vertical ? 40 : 240;
  const int height = vertical ? 240 : 40;

  juce::Component host;
  shmui::LevelMeter meter;
  host.addToDesktop(juce::ComponentPeer::windowIsTemporary);
  host.setBounds(0, 0, width, height);
  host.setVisible(true);
  host.addAndMakeVisible(meter);
  meter.setBounds(host.getLocalBounds());
  meter.setVertical(vertical);
  meter.setBallistics(shmui::MeterBallistics::Peak);
  meter.setDBRange(-60.0f, 6.0f);

  auto style =
      shmui::LevelMeterStyle::fromPresentation(shmui::tokens::meter::consolePresentation());
  style.meterWidth = 24.0f;
  style.cornerRadius = 0.0f;
  style.showPeakHold = false;
  style.showClipIndicator = false;
  style.showScale = false;
  style.showTicks = false;
  meter.setStyle(style);

  meter.setLevelDB(0, 6.0f);
  const auto displayed = pumpUntil([&meter] { return meter.hasClipped(); }, 500);
  require(displayed, "full-scale meter input reaches the presentation state");
  if (!displayed) {
    host.removeFromDesktop();
    return;
  }

  auto image = meter.createComponentSnapshot(meter.getLocalBounds(), true);
  const auto normalisedForDb = [](float dB) { return (dB + 60.0f) / 66.0f; };
  const auto sample = [&image](float normalised) {
    const auto coordinate = juce::jlimit(0, 239, juce::roundToInt(normalised * 239.0f));
    return image.getPixelAt(20, coordinate);
  };

  const auto sampleAtDb = [&](float dB) {
    const auto normalised = normalisedForDb(dB);
    return vertical
               ? sample(1.0f - normalised)
               : image.getPixelAt(juce::jlimit(0, 239, juce::roundToInt(normalised * 239.0f)), 20);
  };

  requireColourNear(sampleAtDb(-12.0f), style.meterColorMid,
                    "meter caution threshold uses the yellow stop");
  requireColourNear(sampleAtDb(-60.0f), style.meterColorLow,
                    "meter safe floor uses the green stop");
  requireColourNear(sampleAtDb(-3.0f), style.meterColorHigh,
                    "meter warning threshold uses the orange stop");
  requireColourNear(sampleAtDb(0.0f), style.clipColor, "meter clip threshold uses the red stop");

  host.removeFromDesktop();
}
} // namespace

int main() {
  juce::ScopedJuceInitialiser_GUI juceRuntime;
  using shmui::AudioAnalyzer;

  require(AudioAnalyzer::calculateRMS(nullptr, 1) == 0.0f, "null RMS input is rejected");
  require(AudioAnalyzer::calculateRMS(nullptr, 0) == 0.0f, "empty RMS input is rejected");

  AudioAnalyzer analyzer;
  analyzer.setSmoothingTimeConstant(std::numeric_limits<float>::quiet_NaN());
  analyzer.setSensitivity(std::numeric_limits<float>::infinity());
  analyzer.pushSamples(nullptr, 32);

  constexpr int impulseIndex = AudioAnalyzer::kMaxBufferSize;
  juce::AudioBuffer<float> stereo(2, impulseIndex + 256);
  stereo.clear();
  stereo.getWritePointer(0)[impulseIndex] = 1.0f;
  stereo.getWritePointer(1)[impulseIndex] = 1.0f;
  analyzer.processBlock(stereo);

  require(analyzer.getPeakLevel() > 0.9f,
          "stereo impulse after the first chunk reaches the analyzer");

  std::vector<float> frequencyData;
  analyzer.getFrequencyData(frequencyData);
  require(frequencyData.size() == static_cast<std::size_t>(AudioAnalyzer::kWaveformFFTSize / 2),
          "waveform publication has its available span");
  requireFiniteNormalized(frequencyData, "frequency publication is finite and normalized");

  AudioAnalyzer retainedAnalyzer;
  std::vector<float> centeredImpulse(static_cast<std::size_t>(AudioAnalyzer::kWaveformFFTSize),
                                     0.0f);
  centeredImpulse[centeredImpulse.size() / 2] = 1.0f;
  retainedAnalyzer.pushSamples(centeredImpulse.data(), static_cast<int>(centeredImpulse.size()));
  std::vector<float> publishedFrame;
  std::vector<float> retainedFrame;
  retainedAnalyzer.getFrequencyData(publishedFrame);
  retainedAnalyzer.getFrequencyData(retainedFrame);
  bool publishedSignal = false;
  for (const float value : publishedFrame) {
    if (value > 0.0f) {
      publishedSignal = true;
      break;
    }
  }
  require(publishedSignal, "published FFT contains the centered impulse");
  require(publishedFrame == retainedFrame,
          "latest FFT frame is retained when no newer frame is ready");

  analyzer.processBlock(stereo);
  std::vector<float> usableBands;
  analyzer.getFrequencyBands(usableBands, 5, 0, analyzer.getFrequencyBinCount());
  require(usableBands.size() == 5, "usable band request keeps its count");
  requireFiniteNormalized(usableBands, "usable frequency bands are finite and normalized");

  std::vector<float> bands{1.0f};
  analyzer.getFrequencyBands(bands, 0, -100, -1);
  require(bands.empty(), "nonpositive band count is empty");

  bands.assign(5, 1.0f);
  analyzer.getFrequencyBands(bands, 5, -100, -1);
  requireFiniteNormalized(bands, "invalid band range is zero-filled");
  for (const float value : bands)
    require(value == 0.0f, "invalid band range remains zero");

  auto owner = std::make_shared<AudioAnalyzer>();
  shmui::BarVisualizer bars;
  bars.setAudioAnalyzer(owner);
  owner.reset();
  bars.setAudioAnalyzer(nullptr);
  bars.setVisible(false);
  bars.setDemoMode(true);
  require(!bars.isTimerRunning(), "hidden bar visualizer is idle");

  std::vector<float> source(100000, 0.5f);
  shmui::ScrollingWaveformVisualizer scrolling;
  scrolling.setDataSource(&source);
  source.clear();
  scrolling.setDataSource(nullptr);
  scrolling.start();
  require(!scrolling.isTimerRunning(), "hidden scrolling visualizer is idle");
  scrolling.stop();

  shmui::LiveWaveformVisualizer live;
  live.setHistorySize(0);
  live.setUpdateRate(0);
  live.setSensitivity(std::numeric_limits<float>::quiet_NaN());
  live.setActive(true);
  require(live.getHistory().empty(), "hidden live visualizer does not tick");

  requireLevelMeterUsesThresholdGradient(true);
  requireLevelMeterUsesThresholdGradient(false);
  requireLevelMeterFollowsAncestorShowingState();

  return failures == 0 ? 0 : 1;
}
