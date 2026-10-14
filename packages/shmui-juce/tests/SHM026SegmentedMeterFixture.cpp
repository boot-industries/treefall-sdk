#include "Components/LevelMeter.h"

#include <array>
#include <atomic>
#include <bit>
#include <chrono>
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
  static float peakHoldLevel(const LevelMeter& meter, int channel) {
    return meter.m_peakHolds[static_cast<size_t>(channel)];
  }
  static float fallbackUpperDbForPeak(float peak) {
    const auto bits = std::bit_cast<uint32_t>(peak);
    const auto bucket = LevelMeter::intervalPeakFallbackBucket(bits);
    const auto upperBits = LevelMeter::intervalPeakFallbackUpperBits(bucket);
    return 20.0f * std::log10(std::bit_cast<float>(upperBits));
  }

  static void injectFallbackPeak(LevelMeter& meter, int channel, float peak) {
    const auto bits = std::bit_cast<uint32_t>(peak);
    const auto bucket = LevelMeter::intervalPeakFallbackBucket(bits);
    const auto channelIndex = static_cast<size_t>(channel);
    const auto wordIndex = static_cast<size_t>(bucket / 64U);
    const auto bit = uint64_t{1} << (bucket % 64U);
    meter.m_intervalPeakFallbackBits[channelIndex][wordIndex].fetch_or(bit,
                                                                       std::memory_order_relaxed);
    meter.m_intervalPeakFallbackPending[channelIndex].store(true, std::memory_order_release);
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
  static uint64_t chromeRebuilds(const LevelMeter& meter) {
    return meter.m_chromeCacheRebuildCount;
  }

  static uint64_t segmentTableRebuilds(const LevelMeter& meter) {
    return meter.m_segmentTableRebuildCount;
  }

  static uint64_t ladderImageRebuilds(const LevelMeter& meter) {
    return meter.m_ladderImageRebuildCount;
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
float dbForSegmentRange(int index, float bodyLength, float gap, float leadingInset,
                        float signalAxisLength, float minDb, float maxDb) {
  const float offset = leadingInset + static_cast<float>(index) * (bodyLength + gap);
  return minDb + ((offset + bodyLength * 0.5f) / signalAxisLength) * (maxDb - minDb);
}

juce::Image renderMeter(shmui::LevelMeter& meter, int width, int height) {
  juce::Image image(juce::Image::ARGB, width, height, true);
  juce::Graphics graphics(image);
  graphics.fillAll(juce::Colours::black);
  meter.paintEntireComponent(graphics, true);
  return image;
}
juce::Image renderMeterAtScale(shmui::LevelMeter& meter, int width, int height, float scale) {
  juce::Image image(juce::Image::ARGB, juce::roundToInt(width * scale),
                    juce::roundToInt(height * scale), true);
  juce::Graphics graphics(image);
  graphics.addTransform(juce::AffineTransform::scale(scale));
  graphics.fillAll(juce::Colours::black);
  meter.paintEntireComponent(graphics, true);
  return image;
}

void requireCacheInvalidationAndPixels() {
  constexpr int width = 24;
  constexpr int height = 120;
  constexpr float bodyLength = 3.0f;
  constexpr float gap = 1.0f;

  shmui::LevelMeter meter;
  meter.setBounds(0, 0, width, height);
  meter.setDBRange(-60.0f, 0.0f);
  meter.setBallistics(shmui::MeterBallistics::PeakRms);
  auto style = meter.getStyle();
  style.backgroundColor = juce::Colours::black;
  style.meterColorLow = juce::Colours::lime;
  style.meterColorMid = juce::Colours::yellow;
  style.meterColorHigh = juce::Colours::red;
  style.clipColor = juce::Colours::magenta;
  style.peakHoldColor = juce::Colours::white;
  style.meterWidth = 18.0f;
  style.meterGap = 2.0f;
  style.yellowThreshold = -12.0f;
  style.redThreshold = -6.0f;
  style.clipThreshold = 0.0f;
  style.fillStyle = shmui::MeterFillStyle::Segmented;
  style.segmentLength = bodyLength;
  style.segmentGap = gap;
  style.greenToYellowTransitionSegments = 0;
  style.showScale = false;
  style.showTicks = false;
  style.showPeakHold = false;
  style.showClipIndicator = false;
  style.showPeakRmsNeedle = false;
  style.cornerRadius = 0.0f;
  meter.setStyle(style);
  meter.setLevelPairDB(0, -10.0f, -20.0f);

  auto image = renderMeter(meter, width, height);
  const float signalAxisLength = static_cast<float>(height - 2);
  const int count =
      shmui::LevelMeterTestAccess::segmentCount(signalAxisLength, bodyLength, gap, 1.0f);
  const float leading =
      shmui::LevelMeterTestAccess::segmentLeadingInset(signalAxisLength, bodyLength, gap, 1.0f);
  int sampleIndex = -1;
  for (int index = 0; index < count; ++index) {
    const float dB =
        dbForSegmentRange(index, bodyLength, gap, leading, signalAxisLength, -60.0f, 0.0f);
    if (dB >= -22.0f && dB <= -18.0f) {
      sampleIndex = index;
      break;
    }
  }
  require(sampleIndex >= 0, "cache fixture locates a grit near -20 dB");
  if (sampleIndex < 0)
    return;

  const float offset = leading + static_cast<float>(sampleIndex) * (bodyLength + gap);
  const int sampleY = juce::roundToInt(static_cast<float>(height - 1) - offset - bodyLength * 0.5f);
  const int sampleX = width / 2;
  requireColourNear(image.getPixelAt(sampleX, sampleY), style.meterColorLow,
                    "the first cached ladder image paints a lit low-level grit");

  const auto initialChrome = shmui::LevelMeterTestAccess::chromeRebuilds(meter);
  const auto initialTable = shmui::LevelMeterTestAccess::segmentTableRebuilds(meter);
  const auto initialLadder = shmui::LevelMeterTestAccess::ladderImageRebuilds(meter);
  const auto paintsBefore = shmui::LevelMeter::paintCount();
  for (int index = 0; index < 12; ++index) {
    meter.setLevelPairDB(0, index % 2 == 0 ? -12.0f : -32.0f, -38.0f);
    image = renderMeter(meter, width, height);
  }
  require(shmui::LevelMeter::paintCount() >= paintsBefore + 12,
          "paintCount observes each direct native fixture paint");
  require(shmui::LevelMeterTestAccess::chromeRebuilds(meter) == initialChrome &&
              shmui::LevelMeterTestAccess::segmentTableRebuilds(meter) == initialTable &&
              shmui::LevelMeterTestAccess::ladderImageRebuilds(meter) == initialLadder,
          "level-only paints reuse chrome, segment geometry, and ladder image");

  meter.setLevelPairDB(0, -30.0f, -40.0f);
  image = renderMeter(meter, width, height);
  requireColourNear(image.getPixelAt(sampleX, sampleY), style.backgroundColor.brighter(0.1f),
                    "a falling level clears formerly lit cached grits to the track colour");

  style.backgroundColor = juce::Colours::navy;
  style.meterColorLow = juce::Colours::cyan;
  meter.setStyle(style);
  meter.setLevelPairDB(0, -10.0f, -20.0f);
  image = renderMeter(meter, width, height);
  requireColourNear(image.getPixelAt(sampleX, sampleY), style.meterColorLow,
                    "style colour changes rebuild and repaint cached grits");
  requireColourNear(image.getPixelAt(0, 0), style.backgroundColor,
                    "style surface changes invalidate the cached chrome");
  require(shmui::LevelMeterTestAccess::chromeRebuilds(meter) > initialChrome &&
              shmui::LevelMeterTestAccess::segmentTableRebuilds(meter) > initialTable &&
              shmui::LevelMeterTestAccess::ladderImageRebuilds(meter) > initialLadder,
          "style changes rebuild every affected meter cache");

  const auto beforeRangeChrome = shmui::LevelMeterTestAccess::chromeRebuilds(meter);
  const auto beforeRangeTable = shmui::LevelMeterTestAccess::segmentTableRebuilds(meter);
  const auto beforeRangeImage = shmui::LevelMeterTestAccess::ladderImageRebuilds(meter);
  meter.setDBRange(-30.0f, 0.0f);
  meter.setLevelPairDB(0, -15.0f, -24.0f);
  image = renderMeter(meter, width, height);
  require(shmui::LevelMeterTestAccess::chromeRebuilds(meter) == beforeRangeChrome + 1 &&
              shmui::LevelMeterTestAccess::segmentTableRebuilds(meter) == beforeRangeTable + 1 &&
              shmui::LevelMeterTestAccess::ladderImageRebuilds(meter) == beforeRangeImage + 1,
          "dB range changes invalidate scale, segment geometry, and ladder");
  requireColourNear(image.getPixelAt(sampleX, sampleY), style.backgroundColor.brighter(0.1f),
                    "changing the dB range moves the same grit above the active level");
  meter.setLevelPairDB(0, -10.0f, -20.0f);

  const auto beforeOrientationChrome = shmui::LevelMeterTestAccess::chromeRebuilds(meter);
  const auto beforeOrientationTable = shmui::LevelMeterTestAccess::segmentTableRebuilds(meter);
  meter.setVertical(false);
  image = renderMeter(meter, width, height);
  require(shmui::LevelMeterTestAccess::chromeRebuilds(meter) == beforeOrientationChrome + 1 &&
              shmui::LevelMeterTestAccess::segmentTableRebuilds(meter) ==
                  beforeOrientationTable + 1,
          "orientation changes rebuild axis-dependent cached geometry");
  requireColourNear(image.getPixelAt(12, 60), style.meterColorLow,
                    "orientation changes preserve the live horizontal fill");

  const auto beforeResizeChrome = shmui::LevelMeterTestAccess::chromeRebuilds(meter);
  const auto beforeResizeTable = shmui::LevelMeterTestAccess::segmentTableRebuilds(meter);
  const auto beforeResizeImage = shmui::LevelMeterTestAccess::ladderImageRebuilds(meter);
  meter.setBounds(0, 0, 160, 64);
  image = renderMeter(meter, 160, 64);
  require(shmui::LevelMeterTestAccess::chromeRebuilds(meter) == beforeResizeChrome + 1 &&
              shmui::LevelMeterTestAccess::segmentTableRebuilds(meter) == beforeResizeTable + 1 &&
              shmui::LevelMeterTestAccess::ladderImageRebuilds(meter) == beforeResizeImage + 1,
          "resize changes rebuild dimension-dependent cached images and geometry");
  const auto beforeChannelsTable = shmui::LevelMeterTestAccess::segmentTableRebuilds(meter);
  meter.setNumChannels(2);
  meter.setLevelPairDB(0, -6.0f, -14.0f);
  meter.setLevelPairDB(1, -26.0f, -28.0f);
  image = renderMeter(meter, 160, 64);
  const int horizontalCount =
      shmui::LevelMeterTestAccess::segmentCount(158.0f, bodyLength, gap, 1.0f);
  const float horizontalLeading =
      shmui::LevelMeterTestAccess::segmentLeadingInset(158.0f, bodyLength, gap, 1.0f);
  int horizontalIndex = -1;
  for (int index = 0; index < horizontalCount; ++index) {
    const float dB =
        dbForSegmentRange(index, bodyLength, gap, horizontalLeading, 158.0f, -30.0f, 0.0f);
    if (dB >= -18.0f && dB <= -14.0f) {
      horizontalIndex = index;
      break;
    }
  }
  require(horizontalIndex >= 0, "horizontal fixture locates a grit near -15 dB");
  if (horizontalIndex >= 0) {
    const float xOffset =
        horizontalLeading + static_cast<float>(horizontalIndex) * (bodyLength + gap);
    const int horizontalX = juce::roundToInt(1.0f + xOffset + bodyLength * 0.5f);
    requireColourNear(image.getPixelAt(horizontalX, 18), style.meterColorLow,
                      "horizontal channel zero paints its current level");
    requireColourNear(image.getPixelAt(horizontalX, 46), style.backgroundColor.brighter(0.1f),
                      "adding a channel keeps the second lane independent and correctly placed");
  }
  require(shmui::LevelMeterTestAccess::segmentTableRebuilds(meter) == beforeChannelsTable,
          "channel-count changes reuse a still-valid same-sized lane cache");

  style.segmentGap = 0.5f;
  meter.setStyle(style);
  const auto beforeScaleChrome = shmui::LevelMeterTestAccess::chromeRebuilds(meter);
  const auto beforeScaleTable = shmui::LevelMeterTestAccess::segmentTableRebuilds(meter);
  const auto beforeScaleImage = shmui::LevelMeterTestAccess::ladderImageRebuilds(meter);
  image = renderMeterAtScale(meter, 160, 64, 2.0f);
  require(shmui::LevelMeterTestAccess::chromeRebuilds(meter) == beforeScaleChrome + 1 &&
              shmui::LevelMeterTestAccess::segmentTableRebuilds(meter) == beforeScaleTable + 1 &&
              shmui::LevelMeterTestAccess::ladderImageRebuilds(meter) == beforeScaleImage + 1,
          "a two-times graphics context rebuilds high-density caches");

  const float physicalGap =
      shmui::LevelMeterTestAccess::resolvedSegmentGap(158.0f, bodyLength, 0.5f, 2.0f);
  const int scaledCount = shmui::LevelMeterTestAccess::segmentCount(158.0f, bodyLength, 0.5f, 2.0f);
  const float scaledLeading =
      shmui::LevelMeterTestAccess::segmentLeadingInset(158.0f, bodyLength, 0.5f, 2.0f);
  int scaledIndex = -1;
  for (int index = 0; index < scaledCount; ++index) {
    const float dB =
        dbForSegmentRange(index, bodyLength, physicalGap, scaledLeading, 158.0f, -30.0f, 0.0f);
    if (dB >= -18.0f && dB <= -14.0f) {
      scaledIndex = index;
      break;
    }
  }
  require(scaledIndex >= 0, "two-times fixture locates a grit near -15 dB");
  if (scaledIndex >= 0) {
    const float xOffset =
        scaledLeading + static_cast<float>(scaledIndex) * (bodyLength + physicalGap);
    const int physicalX = juce::roundToInt((1.0f + xOffset + bodyLength * 0.5f) * 2.0f);
    requireColourNear(image.getPixelAt(physicalX, 36), style.meterColorLow,
                      "a two-times context paints the lit segment at physical pixels");
  }
  const int firstGapX =
      static_cast<int>(std::floor((1.0f + scaledLeading + bodyLength + physicalGap * 0.5f) * 2.0f));
  requireColourNear(
      image.getPixelAt(firstGapX, 36), style.backgroundColor.brighter(0.1f),
      "a half-point gap resolves to one transparent physical pixel at two-times scale");

  const auto priorTheme = shmui::defaultTheme();
  meter.useDefaultThemeStyle();
  auto alternateTheme = priorTheme;
  alternateTheme.meter.surface = juce::Colours::darkred;
  alternateTheme.meter.text = juce::Colours::white;
  alternateTheme.meter.tick = juce::Colours::yellow;
  shmui::setDefaultTheme(alternateTheme);
  image = renderMeter(meter, 160, 64);
  requireColourNear(image.getPixelAt(0, 0), alternateTheme.meter.surface,
                    "theme changes replace the cached background pixels");
  require(colourDistance(image.getPixelAt(80, 12), alternateTheme.meter.surface) > 20,
          "switching back to the theme's continuous fill repaints live levels");
  shmui::setDefaultTheme(priorTheme);
}

void requireIntervalPeakSurvivesThrottlingAndSettles() {
  constexpr int meterWidth = 24;
  constexpr int meterHeight = 120;
  constexpr float segmentLength = 2.0f;
  constexpr float segmentGap = 1.0f;

  juce::Component host;
  shmui::LevelMeter meter;
  host.addToDesktop(juce::ComponentPeer::windowIsTemporary);
  host.setBounds(0, 0, meterWidth, meterHeight);
  host.addAndMakeVisible(meter);
  meter.setBounds(host.getLocalBounds());
  meter.setVertical(true);
  meter.setDBRange(-60.0f, 6.0f);
  meter.setBallistics(shmui::MeterBallistics::PeakRms);
  meter.setMinRepaintIntervalMs(250.0);

  auto style = meter.getStyle();
  style.backgroundColor = juce::Colours::black;
  style.meterColorLow = juce::Colours::lime;
  style.meterColorMid = juce::Colours::yellow;
  style.meterColorHigh = juce::Colours::red;
  style.clipColor = juce::Colours::magenta;
  style.peakHoldColor = juce::Colours::white;
  style.meterWidth = 20.0f;
  style.fillStyle = shmui::MeterFillStyle::Segmented;
  style.segmentLength = segmentLength;
  style.segmentGap = segmentGap;
  style.yellowThreshold = -12.0f;
  style.redThreshold = -6.0f;
  style.clipThreshold = 0.0f;
  style.showPeakHold = false;
  style.showPeakRmsNeedle = true;
  style.showClipIndicator = true;
  style.showScale = false;
  style.showTicks = false;
  style.cornerRadius = 0.0f;
  meter.setStyle(style);

  bool clipCallbackSeen = false;
  float eventPeakDb = -200.0f;
  meter.onClip = [&](int channel) {
    require(channel == 0, "the interval clip callback reports its channel");
    clipCallbackSeen = true;
  };
  meter.onLevelEvent = [&](const shmui::LevelEvent& event) { eventPeakDb = event.peakDb; };
  host.setVisible(true);
  meter.setLevelPairDB(0, -30.0f, -40.0f);
  require(pumpUntil(
              [&] {
                return std::abs(shmui::LevelMeterTestAccess::displayLevel(meter, 0) -
                                (30.0f / 66.0f)) < 0.001f;
              },
              500),
          "the low baseline pair has reached an actual paint");
  pumpFor(20);

  const auto paintsBeforeSpike = shmui::LevelMeter::paintCount();
  meter.setLevelPairDB(0, 6.0f, -6.0f);
  meter.setLevelPairDB(0, -60.0f, -60.0f);
  require(pumpUntil([&] { return shmui::LevelMeter::paintCount() > paintsBeforeSpike; }, 1000),
          "a throttled repaint is scheduled for a transient interval peak");
  require(meter.hasClipped(), "a clip crossing survives a spike followed by a lower latest pair");
  require(shmui::LevelMeterTestAccess::displayLevel(meter, 0) < 0.5f,
          "the raw segmented fill still uses the newest lower pair");
  require(shmui::LevelMeterTestAccess::needleDb(meter, 0) >= 5.9f,
          "the Peak/RMS needle attacks to the retained interval peak");
  require(shmui::LevelMeterTestAccess::peakHoldLevel(meter, 0) >= 0.99f,
          "peak hold retains the interval maximum independently of raw fill");
  require(pumpUntil([&] { return clipCallbackSeen; }, 500),
          "the retained interval clip is delivered through the safe callback path");
  require(eventPeakDb >= 5.9f, "the clip event reports the retained interval peak");

  const auto image = meter.createComponentSnapshot(meter.getLocalBounds(), true);
  const float signalAxisLength = static_cast<float>(meterHeight - 2);
  const int segmentCount =
      shmui::LevelMeterTestAccess::segmentCount(signalAxisLength, segmentLength, segmentGap, 1.0f);
  const float leadingInset = shmui::LevelMeterTestAccess::segmentLeadingInset(
      signalAxisLength, segmentLength, segmentGap, 1.0f);
  int unlitIndex = -1;
  for (int index = 0; index < segmentCount; ++index) {
    const float dB = dbForSegmentRange(index, segmentLength, segmentGap, leadingInset,
                                       signalAxisLength, -60.0f, 6.0f);
    if (dB >= -18.0f && dB <= -12.0f) {
      unlitIndex = index;
      break;
    }
  }
  require(unlitIndex >= 0, "the interval fixture locates an unlit grit above the latest pair");
  if (unlitIndex >= 0) {
    const float offset =
        leadingInset + static_cast<float>(unlitIndex) * (segmentLength + segmentGap);
    const int y =
        juce::roundToInt(static_cast<float>(meterHeight - 1) - offset - segmentLength * 0.5f);
    requireColourNear(image.getPixelAt(meterWidth / 2, y), style.backgroundColor.brighter(0.1f),
                      "an interval needle does not turn the latest raw fill into a stale peak");
  }

  meter.setLevelPairDB(0, 6.0f, -6.0f);
  require(
      pumpUntil([&] { return shmui::LevelMeterTestAccess::displayLevel(meter, 0) > 0.999f; }, 1000),
      "the meter settles at a stable nonzero pair");
  pumpFor(20);
  const auto settledPaintCount = shmui::LevelMeter::paintCount();
  std::thread steadyPublisher([&meter] {
    for (int index = 0; index < 100; ++index) {
      meter.setLevelPairDB(0, 6.0f, -6.0f);
      std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }
  });
  pumpFor(400);
  steadyPublisher.join();
  require(shmui::LevelMeter::paintCount() == settledPaintCount,
          "repeated unchanged nonzero publications do not trigger settled paints");
  host.removeFromDesktop();
}

void requireContendedFallbackCannotForgeClip() {
  constexpr int meterWidth = 24;
  constexpr int meterHeight = 120;
  constexpr float belowThresholdDb = -0.05f;
  const float belowThresholdPeak = std::pow(10.0f, belowThresholdDb / 20.0f);
  const float fallbackUpperDb =
      shmui::LevelMeterTestAccess::fallbackUpperDbForPeak(belowThresholdPeak);
  require(fallbackUpperDb > belowThresholdDb && fallbackUpperDb - belowThresholdDb < 0.05f,
          "contention fallback rounds a peak upward by less than 0.05 dB");
  const float clipThresholdDb = (belowThresholdDb + fallbackUpperDb) * 0.5f;

  shmui::LevelMeter meter;
  meter.setBounds(0, 0, meterWidth, meterHeight);
  meter.setDBRange(-60.0f, 6.0f);
  meter.setBallistics(shmui::MeterBallistics::PeakRms);
  auto style = meter.getStyle();
  style.backgroundColor = juce::Colours::black;
  style.meterColorLow = juce::Colours::lime;
  style.clipColor = juce::Colours::magenta;
  style.meterWidth = 20.0f;
  style.fillStyle = shmui::MeterFillStyle::Segmented;
  style.segmentLength = 2.0f;
  style.segmentGap = 1.0f;
  style.clipThreshold = clipThresholdDb;
  style.showScale = false;
  style.showTicks = false;
  style.showPeakHold = false;
  style.showPeakRmsNeedle = true;
  style.showClipIndicator = true;
  style.cornerRadius = 0.0f;
  meter.setStyle(style);

  std::atomic<bool> start{false};
  std::array<std::thread, 4> writers;
  for (int writer = 0; writer < static_cast<int>(writers.size()); ++writer) {
    writers[static_cast<size_t>(writer)] = std::thread([&, writer] {
      while (!start.load(std::memory_order_acquire))
        std::this_thread::yield();
      for (int sample = 0; sample < 4000; ++sample) {
        const float amount = static_cast<float>(sample) / 3999.0f;
        const float peak = 0.95f + (belowThresholdPeak - 0.95f) * amount;
        meter.setLevelPair(0, peak, peak * 0.5f);
        if ((sample + writer) % 8 == 0)
          std::this_thread::yield();
      }
    });
  }
  start.store(true, std::memory_order_release);
  for (auto& writer : writers)
    writer.join();

  meter.setLevelPair(0, belowThresholdPeak, belowThresholdPeak * 0.5f);
  shmui::LevelMeterTestAccess::injectFallbackPeak(meter, 0, belowThresholdPeak);
  auto image = renderMeter(meter, meterWidth, meterHeight);
  require(!meter.hasClipped(), "a rounded contention fallback below threshold cannot forge a clip");
  require(shmui::LevelMeterTestAccess::needleDb(meter, 0) > belowThresholdDb,
          "the conservative fallback still reaches the peak needle");

  const float crossingPeak = std::pow(10.0f, (clipThresholdDb + 0.1f) / 20.0f);
  meter.setLevelPair(0, crossingPeak, crossingPeak * 0.5f);
  meter.setLevelPair(0, belowThresholdPeak, belowThresholdPeak * 0.5f);
  image = renderMeter(meter, meterWidth, meterHeight);
  require(meter.hasClipped(),
          "an exact producer-side threshold crossing survives lower latest data");
  juce::ignoreUnused(image);
}

void requireFirstPaintUsesNewestNumbers() {
  shmui::LevelMeter meter;
  meter.setBounds(0, 0, 20, 100);
  meter.setDBRange(-60.0f, 0.0f);
  meter.setBallistics(shmui::MeterBallistics::PeakRms);
  auto style = meter.getStyle();
  style.backgroundColor = juce::Colours::black;
  style.meterColorLow = juce::Colours::lime;
  style.meterColorMid = juce::Colours::lime;
  style.meterColorHigh = juce::Colours::lime;
  style.meterWidth = 20.0f;
  style.showScale = false;
  style.showTicks = false;
  style.showPeakHold = false;
  style.showClipIndicator = false;
  style.cornerRadius = 0.0f;
  meter.setStyle(style);

  juce::Image image(juce::Image::ARGB, 20, 100, true);
  juce::Graphics graphics(image);
  meter.paintEntireComponent(graphics, true);
  meter.setLevelPair(0, 1.0f, 0.25f);
  // No dispatch pumping: the first available paint must not need a timer tick.
  meter.paintEntireComponent(graphics, true);
  auto pixel = image.getPixelAt(10, 50);
  require(pixel.getGreen() > 200 && pixel.getRed() < 20,
          "the first paint shows the newly published peak without a timer wait");

  meter.setLevelPair(0, 0.0f, 0.0f);
  meter.paintEntireComponent(graphics, true);
  pixel = image.getPixelAt(10, 50);
  require(pixel.getGreen() < 100, "a later paint does not replay the previous interval's raw peak");
}

void requireClipCallbackCanDestroyMeterAfterPaint() {
  auto meter = std::make_unique<shmui::LevelMeter>();
  meter->setBounds(0, 0, 20, 100);
  meter->setLevel(0, 2.0f);
  bool painting = true;
  meter->onClip = [&](int) {
    require(!painting, "clip callbacks must not destroy a component inside "
                       "JUCE's paint stack");
    if (!painting)
      meter.reset();
  };
  juce::Image image(juce::Image::ARGB, 20, 100, true);
  juce::Graphics graphics(image);
  meter->paintEntireComponent(graphics, true);
  painting = false;
  require(meter != nullptr && meter->hasClipped(),
          "the clip indicator latches in the current paint");
  require(pumpUntil([&] { return meter == nullptr; }, 500),
          "the deferred clip callback can safely destroy the meter");
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
  style.greenToYellowTransitionSegments = 12;
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
  require(greenCount >= 12, "fixture has enough green grits for the twelve-grit transition");

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
    if (isGreen && currentGreenIndex >= greenCount - 12)
      ++transitioningGreens;
  }
  require(transitioningGreens == 12, "exactly twelve pre-yellow green grits transition to yellow");

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
  requireFirstPaintUsesNewestNumbers();
  requireClipCallbackCanDestroyMeterAfterPaint();
  requirePackedPairPublication();
  requireSegmentedRenderingAndNeedle();
  requireSanitizedSegmentStyle();
  requireCacheInvalidationAndPixels();
  requireIntervalPeakSurvivesThrottlingAndSettles();
  requireContendedFallbackCannotForgeClip();

  return failures == 0 ? 0 : 1;
}
