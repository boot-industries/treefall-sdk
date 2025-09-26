#include "orpheus/abi.h"

#include "session/session_graph.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <new>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

using orpheus::core::Clip;
using orpheus::core::SessionGraph;
using orpheus::core::Track;

namespace {

constexpr orpheus_abi_version kCurrentAbi{1, 0};

class IoException : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

SessionGraph *ToSession(orpheus_session_handle handle) {
  return reinterpret_cast<SessionGraph *>(handle);
}

Track *ToTrack(orpheus_track_handle handle) {
  return reinterpret_cast<Track *>(handle);
}

Clip *ToClip(orpheus_clip_handle handle) {
  return reinterpret_cast<Clip *>(handle);
}

template <typename Fn>
orpheus_status GuardAbiCall(Fn &&fn) {
  try {
    return fn();
  } catch (const std::invalid_argument &) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  } catch (const IoException &) {
    return ORPHEUS_STATUS_IO_ERROR;
  } catch (const std::filesystem::filesystem_error &) {
    return ORPHEUS_STATUS_IO_ERROR;
  } catch (const std::ios_base::failure &) {
    return ORPHEUS_STATUS_IO_ERROR;
  } catch (const std::bad_alloc &) {
    return ORPHEUS_STATUS_OUT_OF_MEMORY;
  } catch (...) {
    return ORPHEUS_STATUS_INTERNAL_ERROR;
  }
}

orpheus_status SessionCreate(orpheus_session_handle *out_session) {
  if (out_session == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    auto *session = new SessionGraph();
    *out_session = reinterpret_cast<orpheus_session_handle>(session);
    return ORPHEUS_STATUS_OK;
  });
}

void SessionDestroy(orpheus_session_handle session) {
  auto *ptr = ToSession(session);
  delete ptr;
}

orpheus_status SessionAddTrack(orpheus_session_handle session,
                               const orpheus_track_desc *desc,
                               orpheus_track_handle *out_track) {
  if (session == nullptr || out_track == nullptr || desc == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    auto *session_ptr = ToSession(session);
    const std::string name = desc->name != nullptr ? desc->name : "";
    Track *track = session_ptr->add_track(name);
    *out_track = reinterpret_cast<orpheus_track_handle>(track);
    return ORPHEUS_STATUS_OK;
  });
}

orpheus_status SessionRemoveTrack(orpheus_session_handle session,
                                  orpheus_track_handle track) {
  if (session == nullptr || track == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    auto *session_ptr = ToSession(session);
    auto *track_ptr = ToTrack(track);
    if (!session_ptr->remove_track(track_ptr)) {
      return ORPHEUS_STATUS_NOT_FOUND;
    }
    return ORPHEUS_STATUS_OK;
  });
}

orpheus_status SessionSetTempo(orpheus_session_handle session, double bpm) {
  if (session == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    ToSession(session)->set_tempo(bpm);
    return ORPHEUS_STATUS_OK;
  });
}

orpheus_status SessionGetTransportState(orpheus_session_handle session,
                                        orpheus_transport_state *out_state) {
  if (session == nullptr || out_state == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  auto *session_ptr = ToSession(session);
  const auto state = session_ptr->transport_state();
  out_state->tempo_bpm = state.tempo_bpm;
  out_state->position_beats = state.position_beats;
  out_state->is_playing = state.is_playing ? 1 : 0;
  return ORPHEUS_STATUS_OK;
}

orpheus_status ClipgridAddClip(orpheus_session_handle session,
                               orpheus_track_handle track,
                               const orpheus_clip_desc *desc,
                               orpheus_clip_handle *out_clip) {
  if (session == nullptr || track == nullptr || desc == nullptr ||
      out_clip == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    auto *session_ptr = ToSession(session);
    auto *track_ptr = ToTrack(track);
    const std::string name = desc->name != nullptr ? desc->name : "";
    Clip *clip = session_ptr->add_clip(*track_ptr, name, desc->start_beats,
                                       desc->length_beats);
    *out_clip = reinterpret_cast<orpheus_clip_handle>(clip);
    return ORPHEUS_STATUS_OK;
  });
}

orpheus_status ClipgridRemoveClip(orpheus_session_handle session,
                                  orpheus_clip_handle clip) {
  if (session == nullptr || clip == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    auto *session_ptr = ToSession(session);
    auto *clip_ptr = ToClip(clip);
    if (!session_ptr->remove_clip(clip_ptr)) {
      return ORPHEUS_STATUS_NOT_FOUND;
    }
    return ORPHEUS_STATUS_OK;
  });
}

orpheus_status ClipgridSetClipStart(orpheus_session_handle session,
                                    orpheus_clip_handle clip,
                                    double start_beats) {
  if (session == nullptr || clip == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    auto *session_ptr = ToSession(session);
    auto *clip_ptr = ToClip(clip);
    session_ptr->set_clip_start(*clip_ptr, start_beats);
    return ORPHEUS_STATUS_OK;
  });
}

orpheus_status ClipgridSetClipLength(orpheus_session_handle session,
                                     orpheus_clip_handle clip,
                                     double length_beats) {
  if (session == nullptr || clip == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    auto *session_ptr = ToSession(session);
    auto *clip_ptr = ToClip(clip);
    session_ptr->set_clip_length(*clip_ptr, length_beats);
    return ORPHEUS_STATUS_OK;
  });
}

orpheus_status ClipgridCommit(orpheus_session_handle session) {
  if (session == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    ToSession(session)->commit_clip_grid();
    return ORPHEUS_STATUS_OK;
  });
}

constexpr int kBeatsPerBar = 4;
constexpr int kBitsPerSample = 16;

struct RenderClickParams {
  double tempo_bpm;
  std::uint32_t bars;
  std::uint32_t sample_rate;
  std::uint32_t channels;
  double gain;
  double frequency_hz;
  double duration_seconds;
};

RenderClickParams NormalizeRenderSpec(const orpheus_render_click_spec &spec) {
  RenderClickParams params{};
  params.tempo_bpm = spec.tempo_bpm > 0.0 ? spec.tempo_bpm : 120.0;
  params.bars = spec.bars > 0u ? spec.bars : 4u;
  params.sample_rate = spec.sample_rate > 0u ? spec.sample_rate : 44100u;
  params.channels = spec.channels > 0u ? spec.channels : 2u;
  params.gain = (spec.gain > 0.0 && spec.gain <= 1.0) ? spec.gain : 0.25;
  params.frequency_hz =
      spec.click_frequency_hz > 0.0 ? spec.click_frequency_hz : 1000.0;
  params.duration_seconds = spec.click_duration_seconds > 0.0
                                ? spec.click_duration_seconds
                                : 0.05;
  return params;
}

std::vector<int16_t> GenerateClickSamples(const RenderClickParams &params) {
  const std::uint64_t total_beats =
      static_cast<std::uint64_t>(params.bars) * kBeatsPerBar;
  const double samples_per_beat_f =
      static_cast<double>(params.sample_rate) * 60.0 / params.tempo_bpm;
  std::uint64_t samples_per_beat =
      static_cast<std::uint64_t>(std::llround(samples_per_beat_f));
  samples_per_beat = std::max<std::uint64_t>(1, samples_per_beat);

  const double click_samples_f =
      params.duration_seconds * static_cast<double>(params.sample_rate);
  std::uint64_t click_samples =
      static_cast<std::uint64_t>(std::llround(click_samples_f));
  click_samples = std::max<std::uint64_t>(1, click_samples);

  const std::uint64_t total_samples = samples_per_beat * total_beats;
  std::vector<int16_t> buffer(total_samples * params.channels, 0);

  const double phase_increment =
      2.0 * std::numbers::pi * params.frequency_hz /
      static_cast<double>(params.sample_rate);

  for (std::uint64_t beat = 0; beat < total_beats; ++beat) {
    const std::uint64_t offset = beat * samples_per_beat;
    const double accent = (beat % kBeatsPerBar == 0) ? 1.0 : 0.75;
    for (std::uint64_t i = 0; i < click_samples && (offset + i) < total_samples;
         ++i) {
      const double envelope = 0.5 *
                               (1.0 - std::cos(std::numbers::pi *
                                               static_cast<double>(i) /
                                               static_cast<double>(click_samples)));
      const double sample_value =
          std::sin(phase_increment * static_cast<double>(i)) * envelope *
          params.gain * accent;
      const double clamped = std::clamp(sample_value, -1.0, 1.0);
      const int16_t pcm = static_cast<int16_t>(std::lrint(clamped * 32767.0));
      for (std::uint32_t channel = 0; channel < params.channels; ++channel) {
        buffer[(offset + i) * params.channels + channel] = pcm;
      }
    }
  }

  return buffer;
}

struct WavHeader {
  char riff[4] = {'R', 'I', 'F', 'F'};
  std::uint32_t chunkSize = 0;
  char wave[4] = {'W', 'A', 'V', 'E'};
  char fmt[4] = {'f', 'm', 't', ' '};
  std::uint32_t fmtChunkSize = 16;
  std::uint16_t audioFormat = 1;
  std::uint16_t numChannels = 0;
  std::uint32_t sampleRate = 0;
  std::uint32_t byteRate = 0;
  std::uint16_t blockAlign = 0;
  std::uint16_t bitsPerSample = kBitsPerSample;
  char data[4] = {'d', 'a', 't', 'a'};
  std::uint32_t dataSize = 0;
};

void WriteWaveFile(const std::string &path, const RenderClickParams &params,
                   const std::vector<int16_t> &samples) {
  namespace fs = std::filesystem;
  const fs::path output_path(path);
  if (!output_path.parent_path().empty()) {
    fs::create_directories(output_path.parent_path());
  }

  WavHeader header;
  header.numChannels = static_cast<std::uint16_t>(params.channels);
  header.sampleRate = params.sample_rate;
  header.blockAlign =
      header.numChannels * static_cast<std::uint16_t>(sizeof(int16_t));
  header.byteRate = header.sampleRate * header.blockAlign;
  header.dataSize =
      static_cast<std::uint32_t>(samples.size() * sizeof(int16_t));
  header.chunkSize = 36 + header.dataSize;

  std::ofstream stream(path, std::ios::binary);
  if (!stream) {
    throw IoException("Unable to open render target: " + path);
  }

  stream.write(reinterpret_cast<const char *>(&header), sizeof(header));
  stream.write(reinterpret_cast<const char *>(samples.data()),
               static_cast<std::streamsize>(samples.size() * sizeof(int16_t)));
  if (!stream) {
    throw IoException("Failed to write render output: " + path);
  }
}

orpheus_status RenderClick(const orpheus_render_click_spec *spec,
                           const char *out_path) {
  if (spec == nullptr || out_path == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    const RenderClickParams params = NormalizeRenderSpec(*spec);
    const std::vector<int16_t> samples = GenerateClickSamples(params);
    WriteWaveFile(out_path, params, samples);
    return ORPHEUS_STATUS_OK;
  });
}

orpheus_status RenderTracks(orpheus_session_handle /*session*/,
                            const char * /*out_path*/) {
  return ORPHEUS_STATUS_NOT_IMPLEMENTED;
}

const orpheus_session_v1 kSessionV1{
    &SessionCreate,        &SessionDestroy,     &SessionAddTrack,
    &SessionRemoveTrack,   &SessionSetTempo,    &SessionGetTransportState};

const orpheus_clipgrid_v1 kClipgridV1{&ClipgridAddClip,    &ClipgridRemoveClip,
                                      &ClipgridSetClipStart,
                                      &ClipgridSetClipLength,
                                      &ClipgridCommit};

const orpheus_render_v1 kRenderV1{&RenderClick, &RenderTracks};

orpheus_abi_version NegotiateAbi(orpheus_abi_version requested) {
  if (requested.major != kCurrentAbi.major) {
    return kCurrentAbi;
  }
  orpheus_abi_version negotiated = kCurrentAbi;
  negotiated.minor = std::min(requested.minor, kCurrentAbi.minor);
  return negotiated;
}

const orpheus_abi_negotiator kNegotiator{&NegotiateAbi};

}  // namespace

extern "C" {

const orpheus_session_v1 *orpheus_session_abi_v1() { return &kSessionV1; }

const orpheus_clipgrid_v1 *orpheus_clipgrid_abi_v1() { return &kClipgridV1; }

const orpheus_render_v1 *orpheus_render_abi_v1() { return &kRenderV1; }

const orpheus_abi_negotiator *orpheus_negotiate_abi() { return &kNegotiator; }

}  // extern "C"
