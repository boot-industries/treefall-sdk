// SPDX-License-Identifier: MIT
// ORP121 A-04/A-05: Multi-channel format abstraction and mix matrices
#include <orpheus/channel_format.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace orpheus {

// ============================================================================
// ITU-R BS.775-3 Downmix Coefficients
// These are the standard coefficients for professional broadcast downmix
// ============================================================================

/// Center channel coefficient: 1/sqrt(2) = -3 dB
static constexpr float K_CENTER = 0.7071067811865476f;

/// Surround channel coefficient: 1/sqrt(2) = -3 dB
static constexpr float K_SURROUND = 0.7071067811865476f;

/// LFE coefficient: typically 0 for dialogue-normalized content
/// Some standards use 0.707 or 0.25 depending on content type
static constexpr float K_LFE = 0.0f;

// ============================================================================
// ChannelFormat Factory Methods
// ============================================================================

ChannelFormat ChannelFormat::Mono() {
  ChannelFormat fmt{};
  fmt.layout = ChannelLayout::Mono;
  fmt.num_channels = 1;
  fmt.channel_map.fill(Speaker::None);
  fmt.channel_map[0] = Speaker::C; // Mono is conceptually center
  fmt.name = "Mono";
  return fmt;
}

ChannelFormat ChannelFormat::Stereo() {
  ChannelFormat fmt{};
  fmt.layout = ChannelLayout::Stereo;
  fmt.num_channels = 2;
  fmt.channel_map.fill(Speaker::None);
  fmt.channel_map[0] = Speaker::L;
  fmt.channel_map[1] = Speaker::R;
  fmt.name = "Stereo";
  return fmt;
}

ChannelFormat ChannelFormat::LCR() {
  ChannelFormat fmt{};
  fmt.layout = ChannelLayout::LCR;
  fmt.num_channels = 3;
  fmt.channel_map.fill(Speaker::None);
  fmt.channel_map[0] = Speaker::L;
  fmt.channel_map[1] = Speaker::C;
  fmt.channel_map[2] = Speaker::R;
  fmt.name = "LCR";
  return fmt;
}

ChannelFormat ChannelFormat::Quad() {
  ChannelFormat fmt{};
  fmt.layout = ChannelLayout::Quad;
  fmt.num_channels = 4;
  fmt.channel_map[0] = Speaker::L;
  fmt.channel_map[1] = Speaker::R;
  fmt.channel_map[2] = Speaker::Ls;
  fmt.channel_map[3] = Speaker::Rs;
  fmt.name = "Quad";
  return fmt;
}

ChannelFormat ChannelFormat::Surround50() {
  ChannelFormat fmt{};
  fmt.layout = ChannelLayout::Surround_5_0;
  fmt.num_channels = 5;
  fmt.channel_map[0] = Speaker::L;
  fmt.channel_map[1] = Speaker::R;
  fmt.channel_map[2] = Speaker::C;
  fmt.channel_map[3] = Speaker::Ls;
  fmt.channel_map[4] = Speaker::Rs;
  fmt.name = "5.0 Surround";
  return fmt;
}

ChannelFormat ChannelFormat::Surround51() {
  ChannelFormat fmt{};
  fmt.layout = ChannelLayout::Surround_5_1;
  fmt.num_channels = 6;
  fmt.channel_map.fill(Speaker::None);
  // ITU/SMPTE standard order: L, R, C, LFE, Ls, Rs
  fmt.channel_map[0] = Speaker::L;
  fmt.channel_map[1] = Speaker::R;
  fmt.channel_map[2] = Speaker::C;
  fmt.channel_map[3] = Speaker::LFE;
  fmt.channel_map[4] = Speaker::Ls;
  fmt.channel_map[5] = Speaker::Rs;
  fmt.name = "5.1 Surround";
  return fmt;
}

ChannelFormat ChannelFormat::Surround71() {
  ChannelFormat fmt{};
  fmt.layout = ChannelLayout::Surround_7_1;
  fmt.num_channels = 8;
  fmt.channel_map.fill(Speaker::None);
  // ITU/SMPTE standard order: L, R, C, LFE, Ls, Rs, Lb, Rb
  fmt.channel_map[0] = Speaker::L;
  fmt.channel_map[1] = Speaker::R;
  fmt.channel_map[2] = Speaker::C;
  fmt.channel_map[3] = Speaker::LFE;
  fmt.channel_map[4] = Speaker::Ls;
  fmt.channel_map[5] = Speaker::Rs;
  fmt.channel_map[6] = Speaker::Lb;
  fmt.channel_map[7] = Speaker::Rb;
  fmt.name = "7.1 Surround";
  return fmt;
}

ChannelFormat ChannelFormat::SMPTE51Stereo() {
  ChannelFormat fmt{};
  fmt.layout = ChannelLayout::SMPTE_51_ST;
  fmt.num_channels = 8;
  fmt.channel_map.fill(Speaker::None);
  fmt.channel_map[0] = Speaker::L;
  fmt.channel_map[1] = Speaker::R;
  fmt.channel_map[2] = Speaker::C;
  fmt.channel_map[3] = Speaker::LFE;
  fmt.channel_map[4] = Speaker::Ls;
  fmt.channel_map[5] = Speaker::Rs;
  fmt.channel_map[6] = Speaker::Lo;
  fmt.channel_map[7] = Speaker::Ro;
  fmt.name = "SMPTE 5.1 + Lo/Ro";
  return fmt;
}

ChannelFormat ChannelFormat::SMPTE51MatrixStereo() {
  ChannelFormat fmt{};
  fmt.layout = ChannelLayout::SMPTE_51_LTRT;
  fmt.num_channels = 8;
  fmt.channel_map.fill(Speaker::None);
  fmt.channel_map[0] = Speaker::L;
  fmt.channel_map[1] = Speaker::R;
  fmt.channel_map[2] = Speaker::C;
  fmt.channel_map[3] = Speaker::LFE;
  fmt.channel_map[4] = Speaker::Ls;
  fmt.channel_map[5] = Speaker::Rs;
  fmt.channel_map[6] = Speaker::Lt;
  fmt.channel_map[7] = Speaker::Rt;
  fmt.name = "SMPTE 5.1 + Lt/Rt";
  return fmt;
}

ChannelFormat ChannelFormat::Atmos714() {
  ChannelFormat fmt{};
  fmt.layout = ChannelLayout::Atmos_7_1_4;
  fmt.num_channels = 12;
  fmt.channel_map.fill(Speaker::None);
  // Dolby Atmos 7.1.4 channel order
  fmt.channel_map[0] = Speaker::L;
  fmt.channel_map[1] = Speaker::R;
  fmt.channel_map[2] = Speaker::C;
  fmt.channel_map[3] = Speaker::LFE;
  fmt.channel_map[4] = Speaker::Ls;
  fmt.channel_map[5] = Speaker::Rs;
  fmt.channel_map[6] = Speaker::Lb;
  fmt.channel_map[7] = Speaker::Rb;
  fmt.channel_map[8] = Speaker::Ltf;
  fmt.channel_map[9] = Speaker::Rtf;
  fmt.channel_map[10] = Speaker::Ltb;
  fmt.channel_map[11] = Speaker::Rtb;
  fmt.name = "Dolby Atmos 7.1.4";
  return fmt;
}

ChannelFormat ChannelFormat::Ambisonics(uint8_t order) {
  ChannelFormat fmt{};
  uint8_t num_ch = static_cast<uint8_t>((order + 1) * (order + 1));

  if (order == 1) {
    fmt.layout = ChannelLayout::Ambisonics_FOA;
  } else if (order == 2) {
    fmt.layout = ChannelLayout::Ambisonics_HOA2;
  } else if (order == 3) {
    fmt.layout = ChannelLayout::Ambisonics_HOA3;
  } else {
    fmt.layout = ChannelLayout::Custom;
  }

  fmt.num_channels = num_ch;
  fmt.channel_map.fill(Speaker::None);
  // ACN channel ordering (0, 1, 2, 3, ..., N-1)
  // Speaker enum values W, Y, Z, X map to ACN 0-3
  if (num_ch >= 4) {
    fmt.channel_map[0] = Speaker::W;
    fmt.channel_map[1] = Speaker::Y;
    fmt.channel_map[2] = Speaker::Z;
    fmt.channel_map[3] = Speaker::X;
  }
  fmt.name = "Ambisonics Order " + std::to_string(order);
  return fmt;
}

ChannelFormat ChannelFormat::Custom(uint8_t numChannels) {
  ChannelFormat fmt{};
  fmt.layout = ChannelLayout::Custom;
  fmt.num_channels = numChannels;
  fmt.channel_map.fill(Speaker::None);
  fmt.name = "Custom " + std::to_string(numChannels) + "ch";
  return fmt;
}

// ============================================================================
// MixMatrix Implementation
// ============================================================================

void MixMatrix::apply(const float* const* input, float** output, size_t num_frames) const {
  // Clear output buffers first
  for (uint8_t out_ch = 0; out_ch < output_channels; ++out_ch) {
    std::memset(output[out_ch], 0, num_frames * sizeof(float));
  }

  // Apply matrix: output[out] = sum(input[in] * coeff[out][in])
  for (size_t frame = 0; frame < num_frames; ++frame) {
    for (uint8_t out_ch = 0; out_ch < output_channels; ++out_ch) {
      float sum = 0.0f;
      for (uint8_t in_ch = 0; in_ch < input_channels; ++in_ch) {
        sum += input[in_ch][frame] * coefficients[out_ch][in_ch];
      }
      output[out_ch][frame] = sum;
    }
  }
}

void MixMatrix::applyInterleaved(const float* input, float* output, size_t num_frames) const {
  for (size_t frame = 0; frame < num_frames; ++frame) {
    for (uint8_t out_ch = 0; out_ch < output_channels; ++out_ch) {
      float sum = 0.0f;
      for (uint8_t in_ch = 0; in_ch < input_channels; ++in_ch) {
        sum += input[frame * input_channels + in_ch] * coefficients[out_ch][in_ch];
      }
      output[frame * output_channels + out_ch] = sum;
    }
  }
}

MixMatrix MixMatrix::Identity(uint8_t channels) {
  MixMatrix m{};
  m.input_channels = channels;
  m.output_channels = channels;
  // Initialize all to zero
  for (auto& row : m.coefficients) {
    row.fill(0.0f);
  }
  // Set diagonal to 1.0
  for (uint8_t i = 0; i < channels; ++i) {
    m.coefficients[i][i] = 1.0f;
  }
  return m;
}

MixMatrix MixMatrix::Downmix_51_to_Stereo() {
  // ITU-R BS.775-3 standard downmix
  // L_out = L + 0.707*C + 0.707*Ls
  // R_out = R + 0.707*C + 0.707*Rs
  // LFE omitted (K_LFE = 0)

  MixMatrix m{};
  m.input_channels = 6;  // L, R, C, LFE, Ls, Rs
  m.output_channels = 2; // L, R

  for (auto& row : m.coefficients) {
    row.fill(0.0f);
  }

  // Left output: L + 0.707*C + 0.707*Ls
  m.coefficients[0][0] = 1.0f;       // L
  m.coefficients[0][2] = K_CENTER;   // C
  m.coefficients[0][3] = K_LFE;      // LFE
  m.coefficients[0][4] = K_SURROUND; // Ls

  // Right output: R + 0.707*C + 0.707*Rs
  m.coefficients[1][1] = 1.0f;       // R
  m.coefficients[1][2] = K_CENTER;   // C
  m.coefficients[1][3] = K_LFE;      // LFE
  m.coefficients[1][5] = K_SURROUND; // Rs

  return m;
}

MixMatrix MixMatrix::Downmix_71_to_Stereo() {
  // Extended ITU-R BS.775-3 for 7.1
  // L_out = L + 0.707*C + 0.707*Ls + 0.5*Lb
  // R_out = R + 0.707*C + 0.707*Rs + 0.5*Rb

  MixMatrix m{};
  m.input_channels = 8; // L, R, C, LFE, Ls, Rs, Lb, Rb
  m.output_channels = 2;

  for (auto& row : m.coefficients) {
    row.fill(0.0f);
  }

  // Left output
  m.coefficients[0][0] = 1.0f;       // L
  m.coefficients[0][2] = K_CENTER;   // C
  m.coefficients[0][4] = K_SURROUND; // Ls
  m.coefficients[0][6] = 0.5f;       // Lb (attenuated backs)

  // Right output
  m.coefficients[1][1] = 1.0f;       // R
  m.coefficients[1][2] = K_CENTER;   // C
  m.coefficients[1][5] = K_SURROUND; // Rs
  m.coefficients[1][7] = 0.5f;       // Rb (attenuated backs)

  return m;
}

MixMatrix MixMatrix::Downmix_71_to_51() {
  // 7.1 to 5.1: Fold back channels into surrounds
  // Ls_out = Ls + 0.707*Lb
  // Rs_out = Rs + 0.707*Rb

  MixMatrix m{};
  m.input_channels = 8;
  m.output_channels = 6;

  for (auto& row : m.coefficients) {
    row.fill(0.0f);
  }

  // Pass through L, R, C, LFE
  m.coefficients[0][0] = 1.0f; // L
  m.coefficients[1][1] = 1.0f; // R
  m.coefficients[2][2] = 1.0f; // C
  m.coefficients[3][3] = 1.0f; // LFE

  // Fold backs into surrounds
  m.coefficients[4][4] = 1.0f;       // Ls
  m.coefficients[4][6] = K_SURROUND; // + Lb
  m.coefficients[5][5] = 1.0f;       // Rs
  m.coefficients[5][7] = K_SURROUND; // + Rb

  return m;
}

MixMatrix MixMatrix::Upmix_Mono_to_Stereo() {
  // Mono to stereo: Equal to both channels
  MixMatrix m{};
  m.input_channels = 1;
  m.output_channels = 2;

  for (auto& row : m.coefficients) {
    row.fill(0.0f);
  }

  m.coefficients[0][0] = 1.0f; // L = Mono
  m.coefficients[1][0] = 1.0f; // R = Mono

  return m;
}

MixMatrix MixMatrix::Upmix_Stereo_to_51() {
  // Stereo to 5.1: Phantom center, silent rears
  // This is a basic "compatibility" upmix, not a creative upmixer

  MixMatrix m{};
  m.input_channels = 2;
  m.output_channels = 6;

  for (auto& row : m.coefficients) {
    row.fill(0.0f);
  }

  // Direct stereo to L/R
  m.coefficients[0][0] = 1.0f; // L
  m.coefficients[1][1] = 1.0f; // R

  // Phantom center: average of L+R at reduced level
  // This is optional and may cause phase issues - often better to leave at 0
  // m.coefficients[2][0] = K_CENTER;
  // m.coefficients[2][1] = K_CENTER;

  // LFE, Ls, Rs = 0 (silent)

  return m;
}

// ============================================================================
// Helper Functions
// ============================================================================

MixMatrix getDownmixMatrix(const ChannelFormat& from, const ChannelFormat& to) {
  // Same format - identity
  if (from.num_channels == to.num_channels) {
    return MixMatrix::Identity(from.num_channels);
  }

  // Standard downmix paths
  if (from.layout == ChannelLayout::Surround_7_1 && to.layout == ChannelLayout::Stereo) {
    return MixMatrix::Downmix_71_to_Stereo();
  }
  if (from.layout == ChannelLayout::Surround_7_1 && to.layout == ChannelLayout::Surround_5_1) {
    return MixMatrix::Downmix_71_to_51();
  }
  if (from.layout == ChannelLayout::Surround_5_1 && to.layout == ChannelLayout::Stereo) {
    return MixMatrix::Downmix_51_to_Stereo();
  }

  // Fallback: identity with channel count of minimum
  return MixMatrix::Identity(std::min(from.num_channels, to.num_channels));
}

MixMatrix getUpmixMatrix(const ChannelFormat& from, const ChannelFormat& to) {
  // Same format - identity
  if (from.num_channels == to.num_channels) {
    return MixMatrix::Identity(from.num_channels);
  }

  // Standard upmix paths
  if (from.layout == ChannelLayout::Mono && to.layout == ChannelLayout::Stereo) {
    return MixMatrix::Upmix_Mono_to_Stereo();
  }
  if (from.layout == ChannelLayout::Stereo && to.layout == ChannelLayout::Surround_5_1) {
    return MixMatrix::Upmix_Stereo_to_51();
  }

  // Fallback: identity
  return MixMatrix::Identity(from.num_channels);
}

} // namespace orpheus
