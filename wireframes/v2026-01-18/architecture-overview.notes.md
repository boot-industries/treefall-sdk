# Architecture Overview - Notes

**Last Updated:** 2026-01-18
**Related Diagram:** [architecture-overview.mermaid.md](./architecture-overview.mermaid.md)

## Overview

The Orpheus SDK follows a layered architecture designed for professional audio applications requiring broadcast-safe, deterministic operation. This document describes the architectural decisions and changes introduced in ORP121.

## Architectural Layers

### Applications Layer

Consumer applications built on the Orpheus SDK:

| Application | Technology | Status | Purpose |
|-------------|------------|--------|---------|
| Clip Composer | JUCE C++ | v0.2.x | Professional soundboard for broadcast/theater |
| Wave Finder | Planned | v1.0 roadmap | Harmonic frequency calculator |
| FX Engine | Planned | v1.0 roadmap | LLM-powered audio effects |
| JUCE Demo Host | JUCE C++ | Active | Integration demonstration |

### Adapters Layer

Thin integration adapters that bridge the SDK to specific hosts:

- **Minhost CLI** - Command-line interface for offline rendering and session manipulation
- **REAPER Extension** - DAW integration (currently quarantined pending SDK stabilization)
- **Custom Adapters** - Partner-specific integrations following the adapter pattern

**Adapter Design Principle:** Adapters should be ≤300 LOC, delegating all audio processing to the Core SDK.

### Driver Layer (TypeScript)

JavaScript/TypeScript bindings for web and Node.js applications:

| Driver | Package | Use Case |
|--------|---------|----------|
| Native Driver | `@orpheus/engine-native` | In-process N-API access (lowest latency) |
| Service Driver | `@orpheus/engine-service` | HTTP + WebSocket remote access |
| WASM Driver | `@orpheus/engine-wasm` | Browser-based access via Emscripten |
| Client Broker | `@orpheus/client` | Unified interface with automatic driver selection |

### Core SDK (C++20)

The heart of the system, providing deterministic audio processing.

## ORP121 Routing Enhancements

The ORP121 Audio Backend Refactoring introduced significant improvements to the routing subsystem:

### New Components

#### TruePeakMeter (Q-04)

ITU-R BS.1770-4 compliant true-peak metering:
- **4x oversampling** with 48-tap polyphase FIR filter
- Detects inter-sample peaks that standard peak meters miss
- ~0.1 dB accuracy for inter-sample peak detection
- Essential for broadcast compliance (EBU R128)

**Location:** `src/core/routing/true_peak_meter.h`

#### HeadroomMode (Q-05)

Automatic gain reduction when summing multiple channels:

| Mode | Formula | Use Case |
|------|---------|----------|
| `None` | No reduction | Manual gain staging |
| `PerGroup` | -3 dB per additional channel | Simple mixing |
| `Global` | Based on total active channels | Master bus limiting |
| `Logarithmic` | -10*log10(n) dB | Broadcast standard |

**Location:** `include/orpheus/routing_matrix.h`

#### SPSC Callback Queue (C-03)

Lock-free Single-Producer Single-Consumer ring buffer:
- Replaces mutex-based callback queue
- Eliminates priority inversion risk
- 256-slot capacity
- Graceful overflow handling (drop, don't block)

**Location:** `src/core/transport/transport_controller.cpp`

#### Soft-Knee Limiter (C-02)

Continuous limiter replacing discontinuous tanh implementation:
- **Threshold:** -2 dBFS (0.794 linear)
- **Knee Width:** 0.3
- **Ceiling:** 0.9999
- C1 continuous curve (no audible clicks)

**Location:** `src/core/routing/routing_matrix.cpp`

### Updated Components

#### GainSmoother (C-01)

Extended range from 0-1 to 0-3.98 (+12 dB):
- Supports professional boost requirements
- Maintains smooth transitions
- No clipping at unity gain

#### RoutingMatrix

Now includes:
- Constant-power pan law (cos/sin coefficients)
- Per-channel TruePeakMeter instances
- Per-group TruePeakMeter instances
- Master TruePeakMeter
- Configurable HeadroomMode

#### RoutingConfig (Q-03)

Added `sample_rate` field:
- Default: 48000 Hz
- Supports 44.1 kHz, 96 kHz, etc.
- All sample-rate-dependent calculations now parameterized

## Threading Model

The SDK uses three threads with strict separation:

### Message Thread (UI)
- Handles user input
- Updates visual components
- Processes SDK callbacks via SPSC queue

### Audio Thread (Real-Time)
- Processes audio in `processAudio()` callback
- **No allocations** allowed
- **No locks** allowed (lock-free structures only)
- Posts events to message thread via SPSC queue

### File I/O Thread (Background)
- Pre-loads audio files
- Calculates waveform data
- No interaction with audio thread

## When to Modify This Diagram

Update this diagram when:
- Adding new core SDK modules
- Changing the layer relationships
- Adding new driver types
- Modifying the routing pipeline

## Related Diagrams

- [component-map](./component-map.notes.md) - Detailed class relationships
- [data-flow](./data-flow.notes.md) - How data moves through the system
- [repo-structure](./repo-structure.notes.md) - Directory organization

## References

1. ORP121 Audio Backend Refactoring Master Plan
2. ORP122 Phase 4 Quality Improvements Implementation Report
3. ITU-R BS.1770-4 (True-peak metering specification)
4. EBU R128 (Loudness normalization standard)
