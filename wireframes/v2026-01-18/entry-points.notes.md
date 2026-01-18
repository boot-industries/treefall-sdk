# Entry Points - Notes

**Last Updated:** 2026-01-18
**Related Diagram:** [entry-points.mermaid.md](./entry-points.mermaid.md)

## Overview

This document describes all the ways to interact with the Orpheus SDK, from GUI applications to command-line tools to language bindings.

## User Personas

### End User (Broadcast/Theater Professional)
- Uses Orpheus Clip Composer for live performance
- Expects ultra-low latency (<5ms)
- Needs 24/7 reliability

### Application Developer (TypeScript/JavaScript)
- Builds web or Electron applications
- Uses JavaScript drivers (@orpheus/client)
- May use React integration (@orpheus/react)

### Plugin Developer (C++/Native)
- Directly links C++ libraries
- Builds REAPER extensions, VST plugins
- Requires full API access

### DevOps/QA (Automation)
- Uses CLI tools for batch processing
- Runs automated test suites
- Integrates with CI/CD pipelines

## GUI Applications

### Orpheus Clip Composer

**Technology:** JUCE C++
**Status:** v0.2.x (ORP121 improvements)

**Features:**
- 48-button clip grid × 8 tabs (384 clips visible, 960 total)
- Loop playback with trim IN/OUT
- Fade IN/OUT with curve selection
- Waveform display with zoom
- Audio device settings
- **ORP121:** Improved routing with headroom management

**Entry Point:** Double-click application icon

### JUCE Demo Host

**Technology:** JUCE C++
**Purpose:** SDK integration demonstration

### Custom Applications

**Options:**
1. Direct C++ linking (lowest latency)
2. JavaScript drivers (cross-platform)
3. REAPER adapter (DAW integration)

## Command-Line Tools

### Minhost (orpheus_minhost)

Primary CLI tool for offline rendering and session manipulation.

```bash
# Load session and render click track
./orpheus_minhost --session session.json --render out.wav --bars 4 --bpm 120

# Transport simulation
./orpheus_minhost --session session.json --transport
```

### Session Inspector (inspect_session)

Session analysis and validation tool.

```bash
# Show session summary
./inspect_session --file session.json --stats

# Validate schema
./inspect_session --file session.json --validate
```

## JavaScript Drivers

### Native Driver (@orpheus/engine-native)

**Use Case:** Node.js and Electron applications
**Latency:** Lowest (in-process)

```typescript
import { NativeEngine } from '@orpheus/engine-native';

const engine = new NativeEngine();
await engine.loadSession(path);
await engine.renderClick(output);
```

### Service Driver (@orpheus/engine-service)

**Use Case:** Remote access, multi-process architecture
**Protocol:** HTTP + WebSocket

```typescript
import { ServiceEngine } from '@orpheus/engine-service';

const engine = new ServiceEngine();
await engine.start();
await engine.connect();
```

### WASM Driver (@orpheus/engine-wasm)

**Use Case:** Browser applications
**Isolation:** Web Worker

```typescript
import { WASMEngine } from '@orpheus/engine-wasm';

const engine = new WASMEngine();
await engine.initialize();
```

### Client Broker (@orpheus/client)

**Use Case:** Unified API with automatic driver selection

```typescript
import { OrpheusClient } from '@orpheus/client';

const client = new OrpheusClient();
await client.connect();
// Auto-selects: Native → WASM → Service
```

## Native C++ API

### Direct Linking

```cpp
#include <orpheus/transport_controller.h>
#include <orpheus/routing_matrix.h>

// ORP121 configuration
RoutingConfig config;
config.sample_rate = 48000;
config.headroom_mode = HeadroomMode::Logarithmic;
config.enable_true_peak = true;

RoutingMatrix routing(config);
TransportController transport(&session, &driver);
```

### CMake Integration

```cmake
target_link_libraries(myapp
  orpheus_core
  orpheus_transport
  orpheus_routing
  orpheus_audio_io
)
```

## Configuration

### Session JSON

Human-readable session format stored in `.json` files:
- Tracks, clips, tempo
- Trim points, fades, gain
- Routing assignments

### Environment Variables

| Variable | Purpose | Default |
|----------|---------|---------|
| `ORPHEUS_LOG_LEVEL` | Logging verbosity | WARN |
| `ORPHEUS_DRIVER_TYPE` | Force driver type | auto |
| `ORPHEUS_SERVICE_PORT` | Service driver port | 8080 |

### RoutingConfig (ORP121)

New configuration options:

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `sample_rate` | uint32_t | 48000 | Sample rate in Hz |
| `headroom_mode` | HeadroomMode | None | Automatic gain reduction |
| `enable_true_peak` | bool | false | ITU-R BS.1770-4 metering |

## Related Diagrams

- [architecture-overview](./architecture-overview.notes.md) - System design
- [data-flow](./data-flow.notes.md) - Data processing
