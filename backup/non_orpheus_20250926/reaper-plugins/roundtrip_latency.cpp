#include "roundtrip_latency.h"
#include <numeric>
#include <vector>
#ifdef LATENCY_PROBE_TEST
#include <cstdio>
#endif

static double g_roundtrip_latency = 0.0;

// Utility: cross-correlation to find offset of ping within capture buffer
static size_t find_ping_offset(const std::vector<float>& ping, const std::vector<float>& capture) {
  const size_t ping_size = ping.size();
  const size_t capture_size = capture.size();
  if (capture_size < ping_size)
    return 0;
  size_t best_offset = 0;
  double best_val = -1.0;
  const size_t max_offset = capture_size - ping_size;
  for (size_t o = 0; o <= max_offset; ++o) {
    double v = std::inner_product(ping.begin(), ping.end(), capture.begin() + o, 0.0);
    if (v > best_val) {
      best_val = v;
      best_offset = o;
    }
  }
  return best_offset;
}

// Simulate a loopback capture. Real implementation should interact with audio I/O.
static std::vector<float> simulate_loopback(const std::vector<float>& ping, int srate) {
  int delay_samples = srate / 20; // simulate 50ms roundtrip
  const size_t ping_size = ping.size();
  std::vector<float> buf(delay_samples + ping_size);
  for (size_t i = 0; i < ping_size; ++i)
    buf[delay_samples + i] = ping[i];
  return buf;
}

double CalibrateRoundTripLatency() {
  const int srate = 48000;
  std::vector<float> ping(64, 0.0f);
  ping[0] = 1.0f; // simple impulse

  // In a real implementation, ping would be sent to the output and capture would
  // come from the input. Here we simulate the loopback for demonstration.
  std::vector<float> capture = simulate_loopback(ping, srate);

  size_t offset = find_ping_offset(ping, capture);
  g_roundtrip_latency = static_cast<double>(offset) / static_cast<double>(srate);
  return g_roundtrip_latency;
}

double GetRoundTripLatency() {
  return g_roundtrip_latency;
}

#ifdef LATENCY_PROBE_TEST
int main() {
  double v = CalibrateRoundTripLatency();
  std::printf("%f\n", v);
  return 0;
}
#endif
