#pragma once

#include <cstdint>
#include <cstddef>

namespace reaper {
namespace timecode {

// Enumeration of supported frame rates.
enum class FrameRate {
    FPS24 = 24,
    FPS25 = 25,
    FPS30 = 30,
    FPS30_DROP = 29
};

// SMPTE timecode frame representation.
struct Frame {
    int hours{0};
    int minutes{0};
    int seconds{0};
    int frames{0};
    FrameRate rate{FrameRate::FPS30};
};

// Encode an LTC frame into 80 bits (10 bytes).
void EncodeLTC(const Frame &src, uint8_t out[10]);
// Decode an LTC frame from 80 bits (10 bytes). Returns false on failure.
bool DecodeLTC(const uint8_t in[10], Frame *out);

// Encode an MTC frame as eight quarter-frame messages.
void EncodeMTC(const Frame &src, uint8_t out[8]);
// Decode an MTC frame from eight quarter-frame messages.
bool DecodeMTC(const uint8_t in[8], Frame *out);

// PTPv2 timestamp representation.
struct PTPv2Time {
    uint64_t seconds{0};
    uint32_t nanoseconds{0};
};

// Convert a timecode frame to a PTPv2 timestamp.
PTPv2Time ToPTP(const Frame &src);
// Generate a timecode frame from a PTPv2 timestamp.
Frame FromPTP(const PTPv2Time &src, FrameRate rate);
// Convert a PTPv2 timestamp to an ST2110 RTP clock value.
uint32_t ToST2110RTP(const PTPv2Time &src, uint32_t sampleRate);

// Convert a timecode frame to seconds relative to start.
double ToSeconds(const Frame &tc);
// Generate a timecode frame from seconds.
Frame FromSeconds(double seconds, FrameRate rate);

// Utility helpers for transport synchronisation.
inline double SyncToIncoming(const Frame &tc) { return ToSeconds(tc); }
inline Frame SyncToOutgoing(double position, FrameRate rate) { return FromSeconds(position, rate); }

} // namespace timecode
} // namespace reaper
