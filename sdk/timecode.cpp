#include "timecode.h"
#include <cstring>

namespace reaper {
namespace timecode {

static inline uint8_t to_bcd(int v) {
    return ((v / 10) << 4) | (v % 10);
}

static inline int from_bcd(uint8_t v) {
    return (v >> 4) * 10 + (v & 0x0F);
}

void EncodeLTC(const Frame &src, uint8_t out[10]) {
    std::memset(out, 0, 10);
    out[0] = to_bcd(src.frames);
    out[1] = to_bcd(src.seconds);
    out[2] = to_bcd(src.minutes);
    out[3] = to_bcd(src.hours);
}

bool DecodeLTC(const uint8_t in[10], Frame *out) {
    if (!out) return false;
    out->frames  = from_bcd(in[0]);
    out->seconds = from_bcd(in[1]);
    out->minutes = from_bcd(in[2]);
    out->hours   = from_bcd(in[3]);
    return true;
}

void EncodeMTC(const Frame &src, uint8_t out[8]) {
    const int fpsCode = src.rate == FrameRate::FPS24 ? 0 :
                        src.rate == FrameRate::FPS25 ? 1 :
                        src.rate == FrameRate::FPS30_DROP ? 2 : 3;
    out[0] = (0x0 << 4) | (src.frames & 0x0F);
    out[1] = (0x1 << 4) | ((src.frames >> 4) & 0x01);
    out[2] = (0x2 << 4) | (src.seconds & 0x0F);
    out[3] = (0x3 << 4) | ((src.seconds >> 4) & 0x03);
    out[4] = (0x4 << 4) | (src.minutes & 0x0F);
    out[5] = (0x5 << 4) | ((src.minutes >> 4) & 0x03);
    out[6] = (0x6 << 4) | (src.hours & 0x0F);
    out[7] = (0x7 << 4) | ((src.hours >> 4) & 0x01) | (fpsCode << 1);
}

bool DecodeMTC(const uint8_t in[8], Frame *out) {
    if (!out) return false;
    out->frames  = (in[1] & 0x01) << 4 | (in[0] & 0x0F);
    out->seconds = (in[3] & 0x03) << 4 | (in[2] & 0x0F);
    out->minutes = (in[5] & 0x03) << 4 | (in[4] & 0x0F);
    out->hours   = (in[7] & 0x01) << 4 | (in[6] & 0x0F);
    const int fpsCode = (in[7] >> 1) & 0x3;
    out->rate = fpsCode == 0 ? FrameRate::FPS24 :
                fpsCode == 1 ? FrameRate::FPS25 :
                fpsCode == 2 ? FrameRate::FPS30_DROP : FrameRate::FPS30;
    return true;
}

double ToSeconds(const Frame &tc) {
    const double fps = static_cast<int>(tc.rate);
    return tc.hours * 3600.0 + tc.minutes * 60.0 + tc.seconds + tc.frames / fps;
}

Frame FromSeconds(double seconds, FrameRate rate) {
    Frame out;
    out.rate = rate;
    const double fps = static_cast<int>(rate);
    out.hours = static_cast<int>(seconds / 3600.0);
    seconds -= out.hours * 3600.0;
    out.minutes = static_cast<int>(seconds / 60.0);
    seconds -= out.minutes * 60.0;
    out.seconds = static_cast<int>(seconds);
    seconds -= out.seconds;
    out.frames = static_cast<int>(seconds * fps);
    return out;
}

} // namespace timecode
} // namespace reaper
