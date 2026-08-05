/*
  ==============================================================================

    AudioAnalyzer.cpp
    Created: Shmui-to-JUCE Audio Visualization Port

    Implementation of the core audio analysis engine.

  ==============================================================================
*/

#include "AudioAnalyzer.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace shmui {

//==============================================================================

AudioAnalyzer::AudioAnalyzer(AnalysisMode mode) {
  if (mode == AnalysisMode::Spectrum) {
    fftOrder = kSpectrumFFTOrder;
    fftSize = kSpectrumFFTSize;
  } else {
    fftOrder = kWaveformFFTOrder;
    fftSize = kWaveformFFTSize;
  }

  fft = std::make_unique<juce::dsp::FFT>(fftOrder);
  fftData.resize(fftSize * 2, 0.0f);
  fifo.resize(fftSize, 0.0f);

  for (auto& slot : publicationSlots) {
    jassert(slot.state.is_lock_free());
    slot.data.fill(0.0f);
  }
}

//==============================================================================
// Audio Thread Methods

void AudioAnalyzer::pushSamples(const float* samples, int numSamples) {
  if (samples == nullptr || numSamples <= 0)
    return;

  const float rms = calculateRMS(samples, numSamples);
  const float currentRMS = sanitizeNormalized(smoothedRMS.load(std::memory_order_relaxed));
  const float newRMS = sanitizeNormalized(smoothValue(currentRMS, rms, kVolumeSmoothingFactor));
  smoothedRMS.store(newRMS, std::memory_order_relaxed);

  float peak = 0.0f;
  for (int i = 0; i < numSamples; ++i) {
    const float sample = std::isfinite(samples[i]) ? samples[i] : 0.0f;
    peak = std::max(peak, std::abs(sample));
  }
  peakLevel.store(sanitizeNormalized(peak), std::memory_order_relaxed);

  for (int i = 0; i < numSamples; ++i) {
    const float sample = std::isfinite(samples[i]) ? samples[i] : 0.0f;
    fifo[fifoIndex++] = sample;

    if (fifoIndex >= fftSize) {
      performFFT();
      fifoIndex = 0;
    }
  }
}

void AudioAnalyzer::processBlock(const juce::AudioBuffer<float>& buffer) {
  const int numChannels = buffer.getNumChannels();
  const int numSamples = buffer.getNumSamples();

  if (numChannels <= 0 || numSamples <= 0)
    return;

  for (int offset = 0; offset < numSamples; offset += kMaxBufferSize) {
    const int samplesToProcess = std::min(kMaxBufferSize, numSamples - offset);

    if (numChannels == 1) {
      pushSamples(buffer.getReadPointer(0, offset), samplesToProcess);
      continue;
    }

    std::fill(monoMixBuffer.begin(), monoMixBuffer.begin() + samplesToProcess, 0.0f);

    for (int channel = 0; channel < numChannels; ++channel) {
      const float* channelData = buffer.getReadPointer(channel, offset);
      if (channelData == nullptr)
        continue;

      for (int sample = 0; sample < samplesToProcess; ++sample) {
        const float value = channelData[sample];
        if (std::isfinite(value))
          monoMixBuffer[static_cast<std::size_t>(sample)] += value;
      }
    }

    const float scale = 1.0f / static_cast<float>(numChannels);
    for (int sample = 0; sample < samplesToProcess; ++sample)
      monoMixBuffer[static_cast<std::size_t>(sample)] *= scale;

    pushSamples(monoMixBuffer.data(), samplesToProcess);
  }
}

//==============================================================================
// UI Thread Methods

void AudioAnalyzer::getFrequencyData(std::vector<float>& outData) const {
  const int numBins = std::min(fftSize / 2, static_cast<int>(kPublicationFrameSize));
  outData.assign(static_cast<std::size_t>(std::max(0, numBins)), 0.0f);

  std::size_t slotIndex = 0;
  if (!claimReadableSlot(slotIndex))
    return;

  const auto& frame = publicationSlots[slotIndex].data;
  std::copy_n(frame.begin(), outData.size(), outData.begin());
  releaseReadableSlot(slotIndex);

  const float configuredSensitivity = sensitivity.load(std::memory_order_relaxed);
  const float safeSensitivity =
      std::isfinite(configuredSensitivity) ? std::max(0.0f, configuredSensitivity) : 1.0f;
  for (auto& value : outData)
    value = sanitizeNormalized(value * safeSensitivity);
}

void AudioAnalyzer::getMirroredFrequencyData(std::vector<float>& outData) const {
  std::vector<float> freqData;
  getFrequencyData(freqData);

  const int totalBins = static_cast<int>(freqData.size());
  const int startFreq = static_cast<int>(static_cast<float>(totalBins) * kFrequencyRangeStart);
  const int endFreq = static_cast<int>(static_cast<float>(totalBins) * kFrequencyRangeEnd);
  const int rangeSize = std::max(0, endFreq - startFreq);
  const int halfLength = rangeSize / 2;

  outData.clear();
  outData.reserve(static_cast<std::size_t>(rangeSize));

  for (int i = halfLength - 1; i >= 0; --i) {
    const int index = startFreq + i;
    if (index >= 0 && index < totalBins)
      outData.push_back(sanitizeNormalized(freqData[index]));
  }

  for (int i = 0; i < halfLength; ++i) {
    const int index = startFreq + i;
    if (index >= 0 && index < totalBins)
      outData.push_back(sanitizeNormalized(freqData[index]));
  }
}

float AudioAnalyzer::getRMSLevel() const {
  return sanitizeNormalized(smoothedRMS.load(std::memory_order_relaxed));
}

float AudioAnalyzer::getPeakLevel() const {
  return sanitizeNormalized(peakLevel.load(std::memory_order_relaxed));
}

void AudioAnalyzer::getFrequencyBands(std::vector<float>& outBands, int numBands, int loPass,
                                      int hiPass) const {
  if (numBands <= 0) {
    outBands.clear();
    return;
  }

  const int bandCount = std::min(numBands, static_cast<int>(kPublicationFrameSize));
  outBands.assign(static_cast<std::size_t>(bandCount), 0.0f);

  const int availableBins = std::min(fftSize / 2, static_cast<int>(kPublicationFrameSize));
  if (availableBins <= 0)
    return;

  const int firstBin = juce::jlimit(0, availableBins, loPass);
  const int lastBin = juce::jlimit(0, availableBins, hiPass);
  if (lastBin <= firstBin)
    return;

  std::size_t slotIndex = 0;
  if (!claimReadableSlot(slotIndex))
    return;

  const auto& frame = publicationSlots[slotIndex].data;
  const int sliceLength = lastBin - firstBin;
  const int chunkSize = sliceLength / bandCount + ((sliceLength % bandCount) != 0 ? 1 : 0);

  const float configuredSensitivity = sensitivity.load(std::memory_order_relaxed);
  const float safeSensitivity =
      std::isfinite(configuredSensitivity) ? std::max(0.0f, configuredSensitivity) : 1.0f;

  for (int band = 0; band < bandCount; ++band) {
    const int startOffset = std::min(sliceLength, band * chunkSize);
    const int endOffset = std::min(sliceLength, (band + 1) * chunkSize);
    double sum = 0.0;
    int count = 0;

    for (int bin = firstBin + startOffset; bin < firstBin + endOffset; ++bin) {
      const float magnitude = frame[static_cast<std::size_t>(bin)];
      const float dbValue =
          (std::isfinite(magnitude) && magnitude > 0.0f) ? 20.0f * std::log10(magnitude) : kMinDb;
      sum += static_cast<double>(normalizeDb(dbValue));
      ++count;
    }

    const float average = count > 0 ? static_cast<float>(sum / static_cast<double>(count)) : 0.0f;
    outBands[static_cast<std::size_t>(band)] = sanitizeNormalized(average * safeSensitivity);
  }

  releaseReadableSlot(slotIndex);
}

//==============================================================================
// Configuration

void AudioAnalyzer::setSmoothingTimeConstant(float smoothing) {
  if (!std::isfinite(smoothing))
    return;

  smoothingTimeConstant.store(juce::jlimit(0.0f, 1.0f, smoothing), std::memory_order_relaxed);
}

void AudioAnalyzer::setSensitivity(float newSensitivity) {
  if (!std::isfinite(newSensitivity))
    return;

  sensitivity.store(std::max(0.0f, newSensitivity), std::memory_order_relaxed);
}

//==============================================================================
// Static Utility Functions

float AudioAnalyzer::calculateRMS(const float* samples, int numSamples) {
  if (samples == nullptr || numSamples <= 0)
    return 0.0f;

  double sum = 0.0;
  for (int i = 0; i < numSamples; ++i) {
    const float sample = std::isfinite(samples[i]) ? samples[i] : 0.0f;
    sum += static_cast<double>(sample) * static_cast<double>(sample);
  }

  if (!std::isfinite(sum))
    return 1.0f;

  const double mean = sum / static_cast<double>(numSamples);
  if (!std::isfinite(mean) || mean <= 0.0)
    return 0.0f;

  return sanitizeNormalized(static_cast<float>(std::sqrt(mean)));
}

float AudioAnalyzer::normalizeDb(float value) {
  if (std::isnan(value))
    return 0.0f;

  if (value == -std::numeric_limits<float>::infinity())
    return 0.0f;

  const float clamped = juce::jlimit(kMinDb, kMaxDb, value);
  const float normalized = 1.0f - (clamped * -1.0f) / 100.0f;
  return sanitizeNormalized(std::sqrt(std::max(0.0f, normalized)));
}

float AudioAnalyzer::smoothValue(float current, float target, float factor) {
  const float safeCurrent = sanitizeNormalized(current);
  const float safeTarget = sanitizeNormalized(target);
  const float safeFactor = std::isfinite(factor) ? juce::jlimit(0.0f, 1.0f, factor) : 0.0f;
  return sanitizeNormalized(safeCurrent + (safeTarget - safeCurrent) * safeFactor);
}

//==============================================================================
// Private Methods

void AudioAnalyzer::performFFT() {
  // Copy FIFO data to FFT buffer
  std::copy(fifo.begin(), fifo.end(), fftData.begin());

  // Zero padding for the imaginary part
  std::fill(fftData.begin() + fftSize, fftData.end(), 0.0f);

  // Apply window function (Hann window)
  for (int i = 0; i < fftSize; ++i) {
    const float window = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i /
                                                 static_cast<float>(fftSize - 1)));
    fftData[i] *= window;
  }

  // Perform FFT
  fft->performFrequencyOnlyForwardTransform(fftData.data());

  // Update smoothed data
  updateSmoothedData();
}

void AudioAnalyzer::updateSmoothedData() {
  const float configuredSmooth = smoothingTimeConstant.load(std::memory_order_relaxed);
  const float smooth = std::isfinite(configuredSmooth) ? juce::jlimit(0.0f, 1.0f, configuredSmooth)
                                                       : kDefaultSmoothing;
  const int numBins = std::min(fftSize / 2, static_cast<int>(kPublicationFrameSize));

  for (int i = 0; i < numBins; ++i) {
    const float magnitude = fftData[static_cast<std::size_t>(i)];
    const float normalizedMagnitude =
        std::isfinite(magnitude) ? magnitude / static_cast<float>(fftSize) : 0.0f;
    const float scaledValue = sanitizeNormalized(normalizedMagnitude * 2.0f);

    smoothingAccumulator[static_cast<std::size_t>(i)] =
        smoothValue(smoothingAccumulator[static_cast<std::size_t>(i)], scaledValue, 1.0f - smooth);
  }

  for (std::size_t i = static_cast<std::size_t>(numBins); i < smoothingAccumulator.size(); ++i) {
    smoothingAccumulator[i] = 0.0f;
  }

  std::size_t slotIndex = 0;
  if (!claimWritableSlot(slotIndex))
    return;

  auto& slot = publicationSlots[slotIndex];
  std::copy_n(smoothingAccumulator.begin(), static_cast<std::size_t>(numBins), slot.data.begin());
  std::fill(slot.data.begin() + numBins, slot.data.end(), 0.0f);
  slot.state.store(PublicationState::Ready, std::memory_order_release);
}

bool AudioAnalyzer::claimWritableSlot(std::size_t& index) noexcept {
  for (std::size_t attempt = 0; attempt < kPublicationSlotCount; ++attempt) {
    const std::size_t candidate = (nextWriteSlot + attempt) % kPublicationSlotCount;
    auto& state = publicationSlots[candidate].state;
    PublicationState expected = PublicationState::Free;
    if (state.compare_exchange_strong(expected, PublicationState::Writing,
                                      std::memory_order_acquire, std::memory_order_relaxed)) {
      index = candidate;
      nextWriteSlot = (candidate + 1) % kPublicationSlotCount;
      return true;
    }
  }

  return false;
}

bool AudioAnalyzer::claimReadableSlot(std::size_t& index) const noexcept {
  for (std::size_t attempt = 0; attempt < kPublicationSlotCount; ++attempt) {
    const std::size_t candidate = (nextReadSlot + attempt) % kPublicationSlotCount;
    auto& state = publicationSlots[candidate].state;
    PublicationState expected = PublicationState::Ready;
    if (state.compare_exchange_strong(expected, PublicationState::Reading,
                                      std::memory_order_acq_rel, std::memory_order_relaxed)) {
      index = candidate;
      nextReadSlot = (candidate + 1) % kPublicationSlotCount;
      return true;
    }
  }

  return false;
}

void AudioAnalyzer::releaseReadableSlot(std::size_t index) const noexcept {
  publicationSlots[index].state.store(PublicationState::Free, std::memory_order_release);
}

float AudioAnalyzer::sanitizeNormalized(float value) noexcept {
  if (std::isnan(value))
    return 0.0f;

  if (value == std::numeric_limits<float>::infinity())
    return 1.0f;

  if (value == -std::numeric_limits<float>::infinity())
    return 0.0f;

  return juce::jlimit(0.0f, 1.0f, value);
}

} // namespace shmui
