# Driver Integration Guide

**Audience:** Application developers integrating Orpheus SDK
**Phase:** Phase 1 (P1.DOC.001)
**Last Updated:** 2025-10-12

## Overview

This guide provides step-by-step instructions for integrating Orpheus drivers into your application. The Orpheus SDK offers three driver types:

- **Native Driver** (`@orpheus/engine-native`) - Direct N-API bindings for Node.js/Electron
- **Service Driver** (`@orpheus/engine-service`) - HTTP/WebSocket service for cross-process communication
- **WebAssembly Driver** (`@orpheus/engine-wasm`) - Browser-based driver (Phase 2+)

All drivers are accessed through the unified **`@orpheus/client`** package, which handles automatic driver selection, capability verification, health monitoring, and reconnection.

## Quick Start

### 1. Install Dependencies

```bash
# Install client broker (required)
pnpm add @orpheus/client @orpheus/contract

# Install drivers (at least one required)
pnpm add @orpheus/engine-native   # For Native driver
pnpm add @orpheus/engine-service  # For Service driver
```

### 2. Basic Integration

```typescript
import { OrpheusClient, DriverType } from '@orpheus/client';

// Create client with automatic driver selection
const client = new OrpheusClient({
  driverPreference: [DriverType.Native, DriverType.Service],
  autoConnect: true,
});

// Execute commands
const response = await client.execute({
  type: 'LoadSession',
  path: './session.json',
});

if (response.success) {
  console.log('Session loaded:', response.result);
}

// Subscribe to events
const unsubscribe = client.subscribe((event) => {
  console.log('Event:', event.type, event);
});

// Cleanup
await client.disconnect();
unsubscribe();
```

## Driver Selection

### Automatic Selection with Fallback

The client automatically tries drivers in preference order:

```typescript
const client = new OrpheusClient({
  // Try Native first, fallback to Service
  driverPreference: [DriverType.Native, DriverType.Service],

  // Specific driver configurations
  drivers: {
    native: {
      // Native driver options (usually auto-detected)
    },
    service: {
      url: 'http://localhost:8080',
      authToken: process.env.ORPHEUS_TOKEN,
      timeout: 30000,
    },
  },
});
```

### Manual Driver Selection

```typescript
// Connect to specific driver
await client.connect(DriverType.Service);
```

### Required Capabilities

Reject drivers that don't support required features:

```typescript
const client = new OrpheusClient({
  requiredCommands: ['LoadSession', 'RenderClick'],
  requiredEvents: ['SessionChanged'],
});

// Client will only connect to drivers supporting these capabilities
```

## Native Driver Integration

### Prerequisites

```bash
# Ensure CMake and build tools are installed
cmake --version  # >= 3.22

# Build Orpheus C++ SDK first
cd /path/to/orpheus-sdk
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release

# Build native driver
cd packages/engine-native
pnpm install
pnpm run build:native  # Compiles C++ addon
pnpm run build:ts      # Compiles TypeScript
```

### Usage

```typescript
import { OrpheusClient, DriverType } from '@orpheus/client';

const client = new OrpheusClient({
  driverPreference: [DriverType.Native],
  drivers: {
    native: {
      // Usually auto-detected, but can specify:
      addonPath: '/custom/path/to/orpheus_native.node',
    },
  },
});

await client.connect();

// Native driver provides:
// - Lowest latency (direct C++ calls)
// - Session loading via orpheus::core::SessionGraph
// - Click rendering via orpheus_render_abi_v1
// - Real-time events via N-API callbacks
```

### Platform Considerations

**macOS:**
```bash
# arm64 (Apple Silicon)
pnpm --filter @orpheus/engine-native build:native

# x86_64 (Intel)
CMAKE_OSX_ARCHITECTURES=x86_64 pnpm --filter @orpheus/engine-native build:native
```

**Linux:**
```bash
# Ensure dependencies installed
sudo apt-get install build-essential cmake

pnpm --filter @orpheus/engine-native build:native
```

**Windows:**
```bash
# Visual Studio 2019+ required
pnpm --filter @orpheus/engine-native build:native
```

## Service Driver Integration

### Starting the Service

```bash
# Start orpheusd service
cd packages/engine-service
pnpm start

# Or with custom configuration
ORPHEUS_PORT=8080 ORPHEUS_BIND=127.0.0.1 pnpm start

# With authentication
ORPHEUS_AUTH_TOKEN=your-secret-token pnpm start
```

### Usage

```typescript
import { OrpheusClient, DriverType } from '@orpheus/client';

const client = new OrpheusClient({
  driverPreference: [DriverType.Service],
  drivers: {
    service: {
      url: 'http://127.0.0.1:8080',
      authToken: process.env.ORPHEUS_TOKEN,
      timeout: 30000, // Request timeout in ms
    },
  },
});

await client.connect();

// Service driver provides:
// - Cross-process communication
// - HTTP/WebSocket transport
// - Real-time event streaming
// - Token-based authentication
```

### Authentication

The service driver supports Bearer token authentication:

```typescript
const client = new OrpheusClient({
  drivers: {
    service: {
      url: 'http://localhost:8080',
      authToken: 'your-secret-token-here',
    },
  },
});

// Token is sent as: Authorization: Bearer <token>
```

**Security Notes:**
- Always use environment variables for tokens
- Never commit tokens to version control
- Service binds to 127.0.0.1 by default (localhost only)
- Use HTTPS in production environments

### Service Endpoints

- `GET /health` - Health check (public)
- `GET /version` - Version information (public)
- `GET /contract` - Contract manifest (requires auth if enabled)
- `POST /command` - Execute command (requires auth if enabled)
- `WebSocket /ws` - Event stream (requires auth if enabled)

## Health Monitoring & Reconnection

### Enable Health Checks

```typescript
const client = new OrpheusClient({
  enableHealthChecks: true,
  healthCheckInterval: 30000, // Check every 30 seconds

  enableReconnection: true,
  maxReconnectionAttempts: 3,
  reconnectionDelay: 1000, // Wait 1s between attempts
});

// Monitor connection status
client.on((event) => {
  if (event.type === 'error') {
    console.error('Connection error:', event.error);
  } else if (event.type === 'connected') {
    console.log('Connected to driver:', event.driver);
  } else if (event.type === 'disconnected') {
    console.log('Disconnected from driver');
  }
});
```

### Health Check Behavior

**Service Driver:**
- Checks `/health` endpoint (5s timeout)
- Verifies WebSocket connection state
- Returns healthy only if both are operational

**Native Driver:**
- Verifies session instance validity
- Attempts `getSessionInfo()` call
- Returns healthy if driver operational

### Reconnection Flow

```
Health check fails
    ↓
Attempt 1: Wait 1s → Try connect()
    ↓
Attempt 2: Wait 1s → Try connect()
    ↓
Attempt 3: Wait 1s → Try connect()
    ↓
Max attempts reached → Emit error event
```

## Command Execution

### Basic Commands

```typescript
// Load session
const loadResponse = await client.execute({
  type: 'LoadSession',
  path: './session.json',
});

if (loadResponse.success) {
  const { sessionPath, sessionName, trackCount, tempo } = loadResponse.result;
  console.log(`Loaded: ${sessionName} (${trackCount} tracks @ ${tempo} BPM)`);
}

// Render click track
const renderResponse = await client.execute({
  type: 'RenderClick',
  outputPath: './click.wav',
  bars: 4,
  bpm: 120,
  sampleRate: 48000,
});
```

### Error Handling

```typescript
try {
  const response = await client.execute({
    type: 'LoadSession',
    path: './nonexistent.json',
  });

  if (!response.success) {
    console.error('Command failed:', response.error);
    // response.error: { code, message, details }
  }
} catch (error) {
  // Network or driver errors
  console.error('Execution failed:', error);
}
```

### Command Response Format

```typescript
interface CommandResponse {
  success: boolean;
  requestId?: string;
  result?: unknown;  // Command-specific result data
  error?: {
    code: string;      // Error code (e.g., "session.load")
    message: string;   // Human-readable message
    details?: unknown; // Additional error context
  };
}
```

## Event Subscription

### Subscribe to All Events

```typescript
const unsubscribe = client.subscribe((event) => {
  console.log('Received:', event.type);

  switch (event.type) {
    case 'SessionChanged':
      console.log('Session:', event.sessionPath, event.trackCount);
      break;

    case 'Heartbeat':
      console.log('Uptime:', event.uptime, 'seconds');
      break;
  }
});

// Later: cleanup
unsubscribe();
```

### Filter by Event Type

```typescript
const unsubscribe = client.subscribe(
  (event) => {
    console.log('Session changed:', event);
  },
  {
    eventTypes: ['SessionChanged'],
  }
);
```

### Event Types

**SessionChanged:**
```typescript
{
  type: 'SessionChanged',
  timestamp: 1697040000000,  // Unix ms
  sessionPath: './session.json',
  trackCount: 8,
  sequenceId: 1,
}
```

**Heartbeat:**
```typescript
{
  type: 'Heartbeat',
  timestamp: 1697040000000,
  uptime: 120.5,  // seconds
  sequenceId: 2,
}
```

## React Integration

### Setup

```bash
pnpm add @orpheus/react
```

### Provider

```tsx
import { OrpheusProvider, DriverType } from '@orpheus/react';

function App() {
  return (
    <OrpheusProvider
      config={{
        driverPreference: [DriverType.Native, DriverType.Service],
        autoConnect: true,
      }}
      onStatusChange={(status) => console.log('Status:', status)}
      onError={(error) => console.error('Error:', error)}
    >
      <YourApp />
    </OrpheusProvider>
  );
}
```

### Hooks

```tsx
import {
  useOrpheus,
  useOrpheusCommand,
  useOrpheusEvents,
} from '@orpheus/react';

function SessionLoader() {
  const { client } = useOrpheus();
  const { execute, loading, error, data } = useOrpheusCommand();
  const { latestEvent, events } = useOrpheusEvents({
    eventTypes: ['SessionChanged'],
  });

  const handleLoad = () => {
    execute({
      type: 'LoadSession',
      path: './session.json',
    });
  };

  return (
    <div>
      <button onClick={handleLoad} disabled={loading}>
        {loading ? 'Loading...' : 'Load Session'}
      </button>

      {error && <p>Error: {error.message}</p>}

      {latestEvent && (
        <p>Tracks: {latestEvent.trackCount}</p>
      )}
    </div>
  );
}
```

## Troubleshooting

### Native Driver: "Failed to load native bindings"

**Cause:** Native addon not built

**Solution:**
```bash
cd packages/engine-native
pnpm run build:native
```

### Service Driver: "Failed to connect to service"

**Cause:** Service not running or wrong URL

**Solution:**
```bash
# Start service
cd packages/engine-service
pnpm start

# Or check service URL in client config
```

### "Driver does not support required capabilities"

**Cause:** Driver missing required commands/events

**Solution:**
```typescript
// Check what's required vs available
const caps = client.capabilities;
console.log('Available commands:', caps?.commands);
console.log('Available events:', caps?.events);

// Adjust requirements or use different driver
```

### Health checks failing

**Cause:** Network issues, service crashed, or driver unhealthy

**Solution:**
```typescript
// Manual health check
const isHealthy = await client.driver?.healthCheck();
console.log('Driver healthy:', isHealthy);

// Check service logs
cd packages/engine-service
pm2 logs orpheusd
```

## Best Practices

### 1. Use Environment Variables

```typescript
const client = new OrpheusClient({
  drivers: {
    service: {
      url: process.env.ORPHEUS_SERVICE_URL || 'http://127.0.0.1:8080',
      authToken: process.env.ORPHEUS_TOKEN,
    },
  },
});
```

### 2. Handle Connection Lifecycle

```typescript
const client = new OrpheusClient({ /* config */ });

// Connect
await client.connect();

// Use client
try {
  await client.execute({ /* command */ });
} finally {
  // Always cleanup
  await client.disconnect();
}
```

### 3. Monitor Events

```typescript
client.on((event) => {
  switch (event.type) {
    case 'connected':
      console.log('✓ Connected to', event.driver);
      break;
    case 'disconnected':
      console.log('✗ Disconnected');
      break;
    case 'error':
      console.error('⚠ Error:', event.error.message);
      break;
  }
});
```

### 4. Graceful Degradation

```typescript
const client = new OrpheusClient({
  // Try Native first (best performance)
  // Fallback to Service if Native unavailable
  driverPreference: [DriverType.Native, DriverType.Service],

  // Only require core functionality
  requiredCommands: ['LoadSession'],
});
```

### 5. Test with Multiple Drivers

```typescript
// Test suite should verify both drivers work
describe('Orpheus Integration', () => {
  test('Native driver', async () => {
    const client = new OrpheusClient({
      driverPreference: [DriverType.Native],
    });
    await client.connect();
    // ... test commands
  });

  test('Service driver', async () => {
    const client = new OrpheusClient({
      driverPreference: [DriverType.Service],
    });
    await client.connect();
    // ... test commands
  });
});
```

## Performance Considerations

### Driver Performance Characteristics

| Driver | Latency | Throughput | Memory | Use Case |
|--------|---------|------------|--------|----------|
| Native | ~1-5ms | Very High | Moderate | Real-time audio, desktop apps |
| Service | ~10-50ms | High | Low | Orchestration, remote control |
| WASM | ~5-20ms | High | Low | Browser, web apps |

### Optimization Tips

**Native Driver:**
- Keep in same process as audio I/O
- Minimize command round-trips
- Use event callbacks for real-time updates

**Service Driver:**
- Batch commands when possible
- Use WebSocket events instead of polling
- Enable connection pooling for multiple clients

## Security Considerations

### Service Driver

1. **Bind to localhost** (default: `127.0.0.1`)
2. **Enable authentication** via `ORPHEUS_AUTH_TOKEN`
3. **Use HTTPS** in production
4. **Rate limit** command execution
5. **Validate** all inputs

### Native Driver

1. **Validate** session file paths (prevent directory traversal)
2. **Sandbox** file system access
3. **Limit** render output sizes
4. **Monitor** memory usage

## Further Reading

- [Driver Architecture Overview](./DRIVER_ARCHITECTURE.md)
- [Contract Development Guide](./CONTRACT_DEVELOPMENT.md)
- [Package Naming Conventions](./PACKAGE_NAMING.md)
- [Performance Budgets](./PERFORMANCE.md)

## Support

For issues or questions:
- GitHub Issues: https://github.com/anthropics/orpheus-sdk/issues
- Documentation: https://docs.orpheus-sdk.dev
- Examples: `packages/*/README.md`
