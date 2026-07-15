// SPDX-License-Identifier: MIT
// ORP121 A-04: Multi-channel format abstraction
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace orpheus {

/// Standard channel layouts (ST2110/SMPTE aligned)
/// Values match channel count for bed formats
enum class ChannelLayout : uint8_t {
  Unspecified = 0,
  Mono = 1,
  Stereo = 2,
  LCR = 3,          // Left, Center, Right (film/theater)
  Quad = 4,         // L, R, Ls, Rs (legacy quad)
  Surround_5_0 = 5, // L, R, C, Ls, Rs (film without LFE)
  Surround_5_1 = 6, // L, R, C, LFE, Ls, Rs (ITU-R BS.775)
  Surround_7_1 = 8, // L, R, C, LFE, Ls, Rs, Lb, Rb
  Atmos_5_1_2 = 8,  // 5.1 + 2 height (Ltf, Rtf)
  Atmos_5_1_4 = 10, // 5.1 + 4 height (Ltf, Rtf, Ltb, Rtb)
  Atmos_7_1_4 = 12, // 7.1 + 4 height
  SMPTE_51_ST = 108, // ST 2110-30 compound order: L, R, C, LFE, Ls, Rs, Lo, Ro
  SMPTE_51_LTRT = 109, // Matrix-stereo alternative: L, R, C, LFE, Ls, Rs, Lt, Rt

  // Scene-based formats (ambisonics)
  Ambisonics_FOA = 4,   // First-order: W, Y, Z, X (ACN/SN3D)
  Ambisonics_HOA2 = 9,  // Second-order (9 channels)
  Ambisonics_HOA3 = 16, // Third-order (16 channels)

  Custom = 255 // User-defined layout
};

/// Speaker positions for channel mapping (SMPTE/ITU standard order)
enum class Speaker : uint8_t {
  // Front layer
  L = 0,   // Left
  R = 1,   // Right
  C = 2,   // Center
  LFE = 3, // Low Frequency Effects
  Ls = 4,  // Left Surround
  Rs = 5,  // Right Surround

  // Rear layer (7.1+)
  Lb = 6, // Left Back
  Rb = 7, // Right Back

  // Discrete stereo downmix pair used by SMPTE2110.(51,ST)
  Lo = 15, // Left only
  Ro = 16, // Right only
  Lt = 17, // Left total (matrix-encoded)
  Rt = 18, // Right total (matrix-encoded)

  // Height layer (Atmos/Auro)
  Ltf = 8,  // Left Top Front
  Rtf = 9,  // Right Top Front
  Ltb = 10, // Left Top Back
  Rtb = 11, // Right Top Back

  // Extended height (22.2, etc.)
  Ctf = 12, // Center Top Front
  Ctb = 13, // Center Top Back
  Ts = 14,  // Top Surround (zenith)

  // Ambisonics (ACN channel ordering)
  // Note: These overlap with speaker positions intentionally
  // Ambisonics uses indices 0-N where N = (order+1)^2 - 1
  W = 0, // Omni (ACN 0)
  Y = 1, // Front-back dipole (ACN 1)
  Z = 2, // Up-down dipole (ACN 2)
  X = 3, // Left-right dipole (ACN 3)
  // HOA continues ACN sequence: V, T, R, S, U (order 2), etc.

  None = 255
};

/// Maximum supported channels per format
static constexpr size_t MAX_FORMAT_CHANNELS = 32;

/// Channel format descriptor
/// Describes the speaker layout and channel ordering for an audio format
struct ChannelFormat {
  ChannelLayout layout;
  uint8_t num_channels;
  std::array<Speaker, MAX_FORMAT_CHANNELS> channel_map; // Speaker per channel
  std::string name;

  ChannelFormat() : layout(ChannelLayout::Unspecified), num_channels(0), channel_map{}, name() {
    channel_map.fill(Speaker::None);
  }

  /// True only after a host or parser has established an explicit layout.
  [[nodiscard]] bool isSpecified() const {
    return layout != ChannelLayout::Unspecified && num_channels > 0;
  }

  /// Check if format is bed-based (discrete speaker feeds)
  [[nodiscard]] bool isBedFormat() const {
    return layout != ChannelLayout::Ambisonics_FOA && layout != ChannelLayout::Ambisonics_HOA2 &&
           layout != ChannelLayout::Ambisonics_HOA3;
  }

  /// Check if format is scene-based (ambisonics)
  [[nodiscard]] bool isAmbisonics() const {
    return !isBedFormat();
  }

  /// Get speaker position for a channel index
  [[nodiscard]] Speaker getSpeaker(size_t channel_index) const {
    if (channel_index >= num_channels)
      return Speaker::None;
    return channel_map[channel_index];
  }

  // Factory methods for standard formats
  static ChannelFormat Mono();
  static ChannelFormat Stereo();
  static ChannelFormat LCR();
  static ChannelFormat Quad();
  static ChannelFormat Surround50();
  static ChannelFormat Surround51();
  static ChannelFormat Surround71();
  static ChannelFormat SMPTE51Stereo();
  static ChannelFormat SMPTE51MatrixStereo();
  static ChannelFormat Atmos714();
  static ChannelFormat Ambisonics(uint8_t order);
  static ChannelFormat Custom(uint8_t numChannels);
};

/// Downmix/upmix coefficient matrix
/// Implements format conversion following ITU-R BS.775-3 and Dolby specifications
///
/// Usage: output[out_ch] = sum(input[in_ch] * coefficients[out_ch][in_ch])
struct MixMatrix {
  uint8_t input_channels;
  uint8_t output_channels;
  std::array<std::array<float, MAX_FORMAT_CHANNELS>, MAX_FORMAT_CHANNELS> coefficients; // [out][in]

  /// Apply mix matrix to audio buffer
  /// @param input Array of input channel pointers
  /// @param output Array of output channel pointers
  /// @param num_frames Number of samples per channel
  void apply(const float* const* input, float** output, size_t num_frames) const;

  /// Apply mix matrix to interleaved buffer
  /// @param input Interleaved input samples
  /// @param output Interleaved output samples (must be sized for output_channels)
  /// @param num_frames Number of frames
  void applyInterleaved(const float* input, float* output, size_t num_frames) const;

  // Standard ITU-R BS.775-3 downmix matrices
  static MixMatrix Downmix_51_to_Stereo();
  static MixMatrix Downmix_71_to_Stereo();
  static MixMatrix Downmix_71_to_51();

  // Upmix matrices (phantom/derived)
  static MixMatrix Upmix_Mono_to_Stereo();
  static MixMatrix Upmix_Stereo_to_51(); // Phantom center, silent rears

  // Identity (pass-through)
  static MixMatrix Identity(uint8_t channels);
};

/// Get appropriate downmix matrix for format conversion
/// @param from Source format
/// @param to Target format
/// @return Mix matrix, or identity if no conversion needed
MixMatrix getDownmixMatrix(const ChannelFormat& from, const ChannelFormat& to);

/// Get appropriate upmix matrix for format conversion
/// @param from Source format
/// @param to Target format
/// @return Mix matrix, or identity if no conversion needed
MixMatrix getUpmixMatrix(const ChannelFormat& from, const ChannelFormat& to);

} // namespace orpheus
