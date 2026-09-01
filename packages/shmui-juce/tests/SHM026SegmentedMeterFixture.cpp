#include "Components/LevelMeter.h"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <thread>

namespace shmui {
class LevelMeterTestAccess {
public:
  static std::array<float, 2> inputPair(const LevelMeter& meter, int channel) {
    const auto pair = LevelMeter::unpackLevelPair(
        meter.m_inputLevelPairs[static_cast<size_t>(channel)].load(std::memory_order_relaxed));
    return {pair.peak, pair.rms};
  }

  static float displayLevel(const LevelMeter& meter, int channel) {
    return meter.m_displayLevels[static_cast<size_t>(channel)];
  }

  static float displayRmsLevel(const LevelMeter& meter, int channel) {
    return meter.m_displayRmsLevels[static_cast<size_t>(channel)];
  }

  static float needleDb(const LevelMeter& meter, int channel) {
    return meter.m_peakRmsNeedleDb[static_cast<size_t>(channel)];
  }

  static int segmentCount(float axisLength, float segmentLength, float gap, float displayScale) {
    return LevelMeter::calculateSegmentLayout(axisLength, segmentLength, gap, displayScale).count;
  }

  static float resolvedSegmentGap(float axisLength, float segmentLength, float gap,
                                  float displayScale) {
    return LevelMeter::calculateSegmentLayout(axisLength, segmentLength, gap, displayScale).gap;
  }

  static float segmentLeadingInset(float axisLength, float segmentLength, float gap,
                                   float displayScale) {
    return LevelMeter::calculateSegmentLayout(axisLength, segmentLength, gap, displayScale)
        .leadingInset;
  }
};
} // namespace shmui

namespace {
int failures = 0;

void require(bool condition, const char* message) {
  if (condition)
    return;
  ++failures;
  std::fprintf(stderr, "SHM026 failure: %s\n", message);
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

int colourDistance(juce::Colour left, juce::Colour right) {
  return std::abs(static_cast<int>(left.getRed()) - static_cast<int>(right.getRed())) +
         std::abs(static_cast<int>(left.getGreen()) - static_cast<int>(right.getGreen())) +
         std::abs(static_cast<int>(left.getBlue()) - static_cast<int>(right.getBlue()));
}

void requireColourNear(juce::Colour actual, juce::Colour expected, const char* message) {
  require(colourDistance(actual, expected) <= 8, message);
}

float dbForSegment(int index, int count, float bodyLength, float gap, float leadingInset,
                   float signalAxisLength) {
  const float offset = leadingInset + static_cast<float>(index) * (bodyLength + gap);
  return -60.0f + ((offset + bodyLength * 0.5f) / signalAxisLength) * 66.0f;
}

juce::Colour expectedSegmentColour(const shmui::LevelMeterStyle& style, float db, int greenIndex,
                                   int greenCount) {
  if (db >= style.clipThreshold)
    return style.clipColor;
  if (db >= style.redThreshold)
    return style.meterColorHigh;
  if (db >= style.yellowThreshold) {
    const float t =
        juce::jlimit(0.0f, 1.0f,
                     (db - style.yellowThreshold) /
                         juce::jmax(0.001f, style.redThreshold - style.yellowThreshold));
    return style.meterColorMid.interpolatedWith(style.meterColorHigh, t);
  }

  const int transitionCount = juce::jmin(style.greenToYellowTransitionSegments, greenCount);
  if (transitionCount > 0 && greenIndex >= greenCount - transitionCount) {
    const float t = static_cast<float>(greenIndex - (greenCount - transitionCount) + 1) /
                    static_cast<float>(transitionCount);
    return style.meterColorLow.interpolatedWith(style.meterColorMid, t);
  }
  return style.meterColorLow;
}

void requireLegacyContinuousPairInput() {
  juce::Component host;
  shmui::LevelMeter meter;
  host.addToDesktop(juce::ComponentPeer::windowIsTemporary);
  host.setBounds(0, 0, 24, 240);
  host.addAndMakeVisible(meter);
  meter.setBounds(host.getLocalBounds());
  host.setVisible(true);

  require(meter.getStyle().fillStyle == shmui::MeterFillStyle::Continuous,
          "legacy default keeps continuous rendering");
  meter.setLevelPairDB(0, -6.0f, -30.0f);
  require(pumpUntil(
              [&meter] {
                return shmui::LevelMeterTestAccess::displayLevel(meter, 0) >
                       shmui::LevelMeterTestAccess::displayRmsLevel(meter, 0);
              },
              500),
          "continuous branch consumes the peak member of a paired input");

  host.removeFromDesktop();
}

void requirePackedPairPublication() {
  shmui::LevelMeter meter;
  constexpr std::array<float, 2> pairA{0.8f, 0.3f};
  constexpr std::array<float, 2> pairB{0.6f, 0.2f};
  meter.setLevelPair(0, pairA[0], pairA[1]);

  std::atomic<bool> start{false};
  const auto writer = [&meter, &start](std::array<float, 2> pair) {
    while (!start.load(std::memory_order_acquire))
      std::this_thread::yield();
    for (int index = 0; index < 20000; ++index)
      meter.setLevelPair(0, pair[0], pair[1]);
  };
  std::thread writerA(writer, pairA);
  std::thread writerB(writer, pairB);
  start.store(true, std::memory_order_release);

  for (int index = 0; index < 20000; ++index) {
    const auto observed = shmui::LevelMeterTestAccess::inputPair(meter, 0);
    const bool wholeA = observed == pairA;
    const bool wholeB = observed == pairB;
    require(wholeA || wholeB, "concurrent paired publications never produce a torn pair");
    if (!wholeA && !wholeB)
      break;
  }
  writerA.join();
  writerB.join();

  require(shmui::LevelMeterTestAccess::resolvedSegmentGap(240.0f, 2.0f, 0.5f, 1.0f) == 1.0f,
          "one-times scale resolves a positive half-point gap to one pixel");
  require(shmui::LevelMeterTestAccess::resolvedSegmentGap(240.0f, 2.0f, 0.5f, 2.0f) == 0.5f,
          "two-times scale resolves a positive half-point gap to one pixel");
}

void requireSegmentedRenderingAndNeedle() {
  constexpr int meterWidth = 20;
  constexpr int meterHeight = 330;
  constexpr float contentHeight = static_cast<float>(meterHeight - 2);
  constexpr float bodyLength = 2.0f;
  constexpr float gap = 1.0f;

  juce::Component host;
  shmui::LevelMeter meter;
  host.addToDesktop(juce::ComponentPeer::windowIsTemporary);
  host.setBounds(0, 0, meterWidth, meterHeight);
  host.addAndMakeVisible(meter);
  meter.setBounds(host.getLocalBounds());
  meter.setVertical(true);
  meter.setDBRange(-60.0f, 6.0f);
  meter.setBallistics(shmui::MeterBallistics::PeakRms);

  auto style =
      shmui::LevelMeterStyle::fromPresentation(shmui::tokens::meter::consolePresentation());
  style.fillStyle = shmui::MeterFillStyle::Segmented;
  style.segmentLength = bodyLength;
  style.segmentGap = 0.5f;
  style.greenToYellowTransitionSegments = 6;
  style.showPeakHold = false;
  style.showPeakRmsNeedle = true;
  style.peakRmsNeedleWidth = 2.0f;
  style.peakRmsNeedleReleaseDbPerSecond = 14.5f;
  style.yellowThreshold = -24.0f;
  style.redThreshold = -4.0f;
  style.clipThreshold = 0.0f;
  style.showScale = false;
  style.showTicks = false;
  style.cornerRadius = 0.0f;
  meter.setStyle(style);
  host.setVisible(true);

  meter.setLevelPairDB(0, 6.0f, -12.0f);
  require(
      pumpUntil([&meter] { return shmui::LevelMeterTestAccess::displayLevel(meter, 0) >= 0.999f; },
                500),
      "segmented fill accepts a rising raw peak on the next meter tick");
  require(
      pumpUntil([&meter] { return shmui::LevelMeterTestAccess::needleDb(meter, 0) >= 5.9f; }, 500),
      "peak/RMS needle reaches a new peak without attack interpolation");

  const auto image = meter.createComponentSnapshot(meter.getLocalBounds(), true);
  const int count =
      shmui::LevelMeterTestAccess::segmentCount(contentHeight, bodyLength, 0.5f, 1.0f);
  const float leading =
      shmui::LevelMeterTestAccess::segmentLeadingInset(contentHeight, bodyLength, 0.5f, 1.0f);
  require(count > 6 && count <= 1024, "segmented layout has a finite bounded grit count");

  int greenCount = 0;
  for (int index = 0; index < count; ++index) {
    if (dbForSegment(index, count, bodyLength, gap, leading, contentHeight) < style.yellowThreshold)
      ++greenCount;
  }
  require(greenCount >= 6, "fixture has enough green grits for the six-grit transition");

  int greenIndex = 0;
  int transitioningGreens = 0;
  for (int index = 0; index < count; ++index) {
    const float db = dbForSegment(index, count, bodyLength, gap, leading, contentHeight);
    const bool isGreen = db < style.yellowThreshold;
    const int currentGreenIndex = greenIndex;
    if (isGreen)
      ++greenIndex;
    const float offset = leading + static_cast<float>(index) * (bodyLength + gap);
    const int y =
        juce::roundToInt(static_cast<float>(meterHeight - 1) - offset - bodyLength * 0.5f);
    requireColourNear(image.getPixelAt(meterWidth / 2, y),
                      expectedSegmentColour(style, db, currentGreenIndex, greenCount),
                      "each fixed-position grit uses its midpoint dB colour");
    if (isGreen && currentGreenIndex >= greenCount - 6)
      ++transitioningGreens;
  }
  require(transitioningGreens == 6, "exactly six pre-yellow green grits transition to yellow");

  const int firstGapY = static_cast<int>(
      std::floor(static_cast<float>(meterHeight - 1) - leading - bodyLength - 0.5f));
  requireColourNear(image.getPixelAt(meterWidth / 2, firstGapY),
                    style.backgroundColor.brighter(0.1f), "one-pixel signal-axis gaps remain dark");

  const float firstNeedleDb = shmui::LevelMeterTestAccess::needleDb(meter, 0);
  meter.setLevelPairDB(0, -30.0f, -36.0f);
  pumpFor(70);
  const float releasedNeedleDb = shmui::LevelMeterTestAccess::needleDb(meter, 0);
  require(releasedNeedleDb <= firstNeedleDb, "peak/RMS needle releases after a lower peak");
  require(releasedNeedleDb >= -36.0f, "peak/RMS needle never falls below the current RMS value");

  host.removeFromDesktop();
}

void requireSanitizedSegmentStyle() {
  shmui::LevelMeter meter;
  auto style = meter.getStyle();
  style.fillStyle = shmui::MeterFillStyle::Segmented;
  style.segmentLength = std::numeric_limits<float>::quiet_NaN();
  style.segmentGap = std::numeric_limits<float>::infinity();
  style.greenToYellowTransitionSegments = 100000;
  style.peakRmsNeedleWidth = -1.0f;
  style.peakRmsNeedleReleaseDbPerSecond = -1.0f;
  meter.setStyle(style);
  const auto& sanitized = meter.getStyle();
  require(sanitized.segmentLength == 4.0f && sanitized.segmentGap == 1.0f,
          "non-finite segment geometry receives safe defaults");
  require(sanitized.greenToYellowTransitionSegments == 256, "transition count is bounded");
  require(sanitized.peakRmsNeedleWidth == 0.0f && sanitized.peakRmsNeedleReleaseDbPerSecond == 0.0f,
          "negative needle geometry is bounded");

  style.segmentLength = 0.0f;
  style.segmentGap = -1.0f;
  style.peakRmsNeedleWidth = std::numeric_limits<float>::infinity();
  style.peakRmsNeedleReleaseDbPerSecond = std::numeric_limits<float>::quiet_NaN();
  meter.setStyle(style);
  const auto& bounded = meter.getStyle();
  require(bounded.segmentLength == 4.0f && bounded.segmentGap == 0.0f,
          "zero and negative segment geometry is bounded");
  require(bounded.peakRmsNeedleWidth == 2.0f && bounded.peakRmsNeedleReleaseDbPerSecond == 14.5f,
          "non-finite needle settings receive safe defaults");
  require(shmui::LevelMeterTestAccess::segmentCount(1.0e9f, 1.0f, 0.0f, 2.0f) == 1024,
          "computed segment count is capped at 1024");
}
} // namespace

int main() {
  juce::ScopedJuceInitialiser_GUI juceRuntime;

  requireLegacyContinuousPairInput();
  requirePackedPairPublication();
  requireSegmentedRenderingAndNeedle();
  requireSanitizedSegmentStyle();

  return failures == 0 ? 0 : 1;
}
