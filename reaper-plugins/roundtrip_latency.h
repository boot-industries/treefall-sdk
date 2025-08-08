#pragma once

// Simple roundtrip latency probe API used by the audio engine.
// CalibrateRoundTripLatency() performs a ping/loopback test and stores the measured latency.
// GetRoundTripLatency() returns the last measured roundtrip latency in seconds.

double CalibrateRoundTripLatency();
double GetRoundTripLatency();

