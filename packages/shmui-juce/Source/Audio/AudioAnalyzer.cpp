/*
  ==============================================================================

    AudioAnalyzer.cpp
    Created: Shmui-to-JUCE Audio Visualization Port

    Implementation of the core audio analysis engine.

  ==============================================================================
*/

#include "AudioAnalyzer.h"

namespace shmui
{

//==============================================================================

AudioAnalyzer::AudioAnalyzer(AnalysisMode mode)
{
    // Set FFT size based on mode
    if (mode == AnalysisMode::Spectrum)
    {
        fftOrder = kSpectrumFFTOrder;
        fftSize = kSpectrumFFTSize;
    }
    else
    {
        fftOrder = kWaveformFFTOrder;
        fftSize = kWaveformFFTSize;
    }

    // Create FFT processor
    fft = std::make_unique<juce::dsp::FFT>(fftOrder);

    // Allocate buffers
    fftData.resize(fftSize * 2, 0.0f);
    fifo.resize(fftSize, 0.0f);
    smoothedFrequencyData.resize(fftSize / 2, 0.0f);
}

//==============================================================================
// Audio Thread Methods

void AudioAnalyzer::pushSamples(const float* samples, int numSamples)
{
    // Calculate RMS for level metering
    const float rms = calculateRMS(samples, numSamples);

    // Smooth the RMS value
    const float currentRMS = smoothedRMS.load(std::memory_order_relaxed);
    const float newRMS = smoothValue(currentRMS, rms, kVolumeSmoothingFactor);
    smoothedRMS.store(newRMS, std::memory_order_relaxed);

    // Track peak level
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        peak = std::max(peak, std::abs(samples[i]));
    }
    peakLevel.store(peak, std::memory_order_relaxed);

    // Fill FIFO for FFT
    for (int i = 0; i < numSamples; ++i)
    {
        fifo[fifoIndex++] = samples[i];

        if (fifoIndex >= fftSize)
        {
            performFFT();
            fifoIndex = 0;
        }
    }
}

void AudioAnalyzer::processBlock(const juce::AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if (numChannels == 0 || numSamples == 0)
        return;

    // Average all channels for mono analysis
    if (numChannels == 1)
    {
        pushSamples(buffer.getReadPointer(0), numSamples);
    }
    else
    {
        // Use pre-allocated buffer (sized to kMaxBufferSize in constructor)
        // If buffer exceeds max size, only process what fits (should not happen in practice)
        const int samplesToProcess = std::min(numSamples, kMaxBufferSize);

        // Clear and mix down to mono
        std::fill(monoMixBuffer.begin(), monoMixBuffer.begin() + samplesToProcess, 0.0f);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* channelData = buffer.getReadPointer(ch);
            for (int i = 0; i < samplesToProcess; ++i)
            {
                monoMixBuffer[i] += channelData[i];
            }
        }

        // Normalize
        const float scale = 1.0f / static_cast<float>(numChannels);
        for (int i = 0; i < samplesToProcess; ++i)
        {
            monoMixBuffer[i] *= scale;
        }

        pushSamples(monoMixBuffer.data(), samplesToProcess);
    }
}

//==============================================================================
// UI Thread Methods

void AudioAnalyzer::getFrequencyData(std::vector<float>& outData) const
{
    const juce::SpinLock::ScopedLockType lock(dataLock);

    outData = smoothedFrequencyData;

    // Apply sensitivity
    const float sens = sensitivity.load(std::memory_order_relaxed);
    if (sens != 1.0f)
    {
        for (auto& value : outData)
        {
            value = juce::jlimit(0.0f, 1.0f, value * sens);
        }
    }
}

void AudioAnalyzer::getMirroredFrequencyData(std::vector<float>& outData) const
{
    // Get the frequency data first
    std::vector<float> freqData;
    getFrequencyData(freqData);

    // Extract relevant frequency range (5% to 40% of bins for voice focus)
    const int totalBins = static_cast<int>(freqData.size());
    const int startFreq = static_cast<int>(totalBins * kFrequencyRangeStart);
    const int endFreq = static_cast<int>(totalBins * kFrequencyRangeEnd);

    const int rangeSize = endFreq - startFreq;
    const int halfLength = rangeSize / 2;

    outData.clear();
    outData.reserve(rangeSize);

    // Left side (reversed) - as in waveform.tsx lines 655-658
    for (int i = halfLength - 1; i >= 0; --i)
    {
        const int idx = startFreq + i;
        if (idx < totalBins)
        {
            outData.push_back(freqData[idx]);
        }
    }

    // Right side (normal) - as in waveform.tsx lines 660-663
    for (int i = 0; i < halfLength; ++i)
    {
        const int idx = startFreq + i;
        if (idx < totalBins)
        {
            outData.push_back(freqData[idx]);
        }
    }
}

float AudioAnalyzer::getRMSLevel() const
{
    return smoothedRMS.load(std::memory_order_relaxed);
}

float AudioAnalyzer::getPeakLevel() const
{
    return peakLevel.load(std::memory_order_relaxed);
}

void AudioAnalyzer::getFrequencyBands(std::vector<float>& outBands,
                                      int numBands,
                                      int loPass,
                                      int hiPass) const
{
    // Implementation from bar-visualizer.tsx splitIntoBands function
    outBands.resize(numBands);

    const juce::SpinLock::ScopedLockType lock(dataLock);

    const int sliceLength = hiPass - loPass;
    const int chunkSize = (sliceLength + numBands - 1) / numBands;

    for (int i = 0; i < numBands; ++i)
    {
        float sum = 0.0f;
        int count = 0;

        const int start = loPass + i * chunkSize;
        const int end = std::min(loPass + (i + 1) * chunkSize, hiPass);

        for (int j = start; j < end; ++j)
        {
            if (j < static_cast<int>(smoothedFrequencyData.size()))
            {
                // Use normalizeDb for perceptual scaling
                // Note: smoothedFrequencyData already contains magnitude values
                // We convert to dB-like range for normalization
                const float magnitude = smoothedFrequencyData[j];
                const float dbValue = magnitude > 0.0f ?
                    20.0f * std::log10(magnitude) : kMinDb;
                sum += normalizeDb(dbValue);
                count++;
            }
        }

        outBands[i] = count > 0 ? sum / static_cast<float>(count) : 0.0f;
    }

    // Apply sensitivity
    const float sens = sensitivity.load(std::memory_order_relaxed);
    if (sens != 1.0f)
    {
        for (auto& value : outBands)
        {
            value = juce::jlimit(0.0f, 1.0f, value * sens);
        }
    }
}

//==============================================================================
// Configuration

void AudioAnalyzer::setSmoothingTimeConstant(float smoothing)
{
    smoothingTimeConstant.store(juce::jlimit(0.0f, 1.0f, smoothing),
                                std::memory_order_relaxed);
}

void AudioAnalyzer::setSensitivity(float newSensitivity)
{
    sensitivity.store(std::max(0.0f, newSensitivity), std::memory_order_relaxed);
}

//==============================================================================
// Static Utility Functions

float AudioAnalyzer::calculateRMS(const float* samples, int numSamples)
{
    if (numSamples <= 0)
        return 0.0f;

    float sum = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        sum += samples[i] * samples[i];
    }

    return std::sqrt(sum / static_cast<float>(numSamples));
}

float AudioAnalyzer::normalizeDb(float value)
{
    // From bar-visualizer.tsx normalizeDb function
    // Perceptual scaling: convert dB to 0-1 with sqrt for perceptual response

    if (value == -std::numeric_limits<float>::infinity())
        return 0.0f;

    const float clamped = juce::jlimit(kMinDb, kMaxDb, value);
    const float normalized = 1.0f - (clamped * -1.0f) / 100.0f;

    return std::sqrt(normalized);  // Perceptual scaling
}

float AudioAnalyzer::smoothValue(float current, float target, float factor)
{
    return current + (target - current) * factor;
}

//==============================================================================
// Private Methods

void AudioAnalyzer::performFFT()
{
    // Copy FIFO data to FFT buffer
    std::copy(fifo.begin(), fifo.end(), fftData.begin());

    // Zero padding for the imaginary part
    std::fill(fftData.begin() + fftSize, fftData.end(), 0.0f);

    // Apply window function (Hann window)
    for (int i = 0; i < fftSize; ++i)
    {
        const float window = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi *
                                                      i / static_cast<float>(fftSize - 1)));
        fftData[i] *= window;
    }

    // Perform FFT
    fft->performFrequencyOnlyForwardTransform(fftData.data());

    // Update smoothed data
    updateSmoothedData();
}

void AudioAnalyzer::updateSmoothedData()
{
    const juce::SpinLock::ScopedLockType lock(dataLock);

    const float smooth = smoothingTimeConstant.load(std::memory_order_relaxed);
    const int numBins = fftSize / 2;

    for (int i = 0; i < numBins; ++i)
    {
        // Get magnitude from FFT output
        // fft->performFrequencyOnlyForwardTransform gives us real magnitudes
        const float magnitude = fftData[i];

        // Normalize to 0-1 range (divide by FFT size for proper scaling)
        const float normalizedMagnitude = magnitude / static_cast<float>(fftSize);

        // Scale to 0-255 range like Web Audio's getByteFrequencyData
        // then normalize back to 0-1
        const float scaledValue = juce::jlimit(0.0f, 1.0f, normalizedMagnitude * 2.0f);

        // Apply smoothing
        smoothedFrequencyData[i] = smoothedFrequencyData[i] * smooth +
                                   scaledValue * (1.0f - smooth);
    }
}

} // namespace shmui
