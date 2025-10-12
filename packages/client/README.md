# @orpheus/client

Unified client broker for Orpheus SDK with automatic driver selection and handshake protocol.

## Overview

The Orpheus Client provides a high-level, unified interface for interacting with the Orpheus SDK. It automatically selects and connects to the best available driver (Native or Service) based on your configuration and environment.

## Features

- **Automatic Driver Selection**: Tries drivers in preference order until one succeeds
- **Unified Interface**: Single API for all commands and events, regardless of driver
- **Connection Management**: Automatic connection, disconnection, and failover
- **Event Subscription**: Subscribe to SDK events with optional filtering
- **TypeScript Support**: Full type definitions included

## Installation

```bash
pnpm add @orpheus/client
```

**Optional peer dependencies:**
```bash
# For native driver (N-API bindings)
pnpm add @orpheus/engine-native

# For service driver (HTTP/WebSocket)
pnpm add @orpheus/engine-service
```

## Quick Start

```typescript
import { OrpheusClient } from '@orpheus/client';

// Create client with automatic driver selection
const client = new OrpheusClient({
  autoConnect: true,
});

// Wait for connection
client.on((event) => {
  if (event.type === 'connected') {
    console.log(`Connected to ${event.driver} driver`);
  }
});

// Execute commands
const response = await client.execute({
  type: 'LoadSession',
  payload: {
    sessionPath: './session.json',
  },
  requestId: 'load-1',
});

// Subscribe to events
client.subscribe((event) => {
  console.log('SDK event:', event);
});
```

## Driver Selection

The client supports two driver types:

### Native Driver (`DriverType.Native`)
- Direct N-API bindings to Orpheus C++ SDK
- Highest performance, lowest latency
- Requires `@orpheus/engine-native` to be installed and built
- Best for: Electron apps, Node.js services with performance requirements

### Service Driver (`DriverType.Service`)
- HTTP/WebSocket connection to `orpheusd` service
- Cross-process, language-agnostic
- Requires `orpheusd` to be running
- Best for: Web apps, microservices, language bindings

## Configuration

### Basic Configuration

```typescript
const client = new OrpheusClient({
  // Driver preference order (default: [Native, Service])
  driverPreference: [DriverType.Native, DriverType.Service],

  // Auto-connect on instantiation
  autoConnect: false,

  // Driver-specific configs
  drivers: {
    service: {
      url: 'http://127.0.0.1:8080',
      authToken: process.env.ORPHEUS_TOKEN,
      timeout: 30000,
    },
    native: {
      addonPath: './custom/path/orpheus_native.node',
    },
  },
});
```

### Service Driver Only

```typescript
const client = new OrpheusClient({
  driverPreference: [DriverType.Service],
  drivers: {
    service: {
      url: 'http://127.0.0.1:8080',
      authToken: 'your-secret-token',
    },
  },
});
```

### Native Driver Only

```typescript
const client = new OrpheusClient({
  driverPreference: [DriverType.Native],
});
```

## API Reference

### OrpheusClient

#### Constructor

```typescript
new OrpheusClient(config?: ClientConfig)
```

#### Properties

```typescript
// Current driver instance
client.driver: IDriver | null

// Connection status
client.status: ConnectionStatus

// Driver capabilities
client.capabilities: DriverCapabilities | null
```

#### Methods

**connect(driverType?: DriverType): Promise<void>**

Connect to a driver. If `driverType` is specified, only that driver will be tried. Otherwise, attempts drivers in preference order.

```typescript
// Auto-select driver
await client.connect();

// Force specific driver
await client.connect(DriverType.Service);
```

**disconnect(): Promise<void>**

Disconnect from the current driver.

```typescript
await client.disconnect();
```

**execute(command: OrpheusCommand): Promise<OrpheusResponse>**

Execute a command.

```typescript
const response = await client.execute({
  type: 'LoadSession',
  payload: { sessionPath: './session.json' },
  requestId: 'load-1',
});

if (response.success) {
  console.log('Session loaded:', response.result);
} else {
  console.error('Error:', response.error);
}
```

**on(callback: (event: ClientEvent) => void): () => void**

Subscribe to client events (connection status changes).

```typescript
const unsubscribe = client.on((event) => {
  switch (event.type) {
    case 'connected':
      console.log(`Connected to ${event.driver}`);
      break;
    case 'disconnected':
      console.log('Disconnected');
      break;
    case 'error':
      console.error('Error:', event.error);
      break;
  }
});

// Unsubscribe later
unsubscribe();
```

**subscribe(callback: (event: OrpheusEvent) => void, options?: SubscriptionOptions): () => void**

Subscribe to Orpheus SDK events.

```typescript
// Subscribe to all events
const unsubscribe = client.subscribe((event) => {
  console.log('Event:', event.type, event.payload);
});

// Subscribe to specific event types
client.subscribe(
  (event) => {
    console.log('Session changed:', event.payload);
  },
  { eventTypes: ['SessionChanged'] }
);
```

## Event Types

### Client Events

```typescript
type ClientEvent =
  | { type: 'connected'; driver: DriverType }
  | { type: 'disconnected' }
  | { type: 'error'; error: Error }
  | { type: 'driver-changed'; from: DriverType | null; to: DriverType };
```

### Orpheus Events

See `@orpheus/contract` for event schemas:
- `SessionChanged` - Session state changed
- `Heartbeat` - Periodic heartbeat with status
- `RenderProgress` - Render operation progress

## Examples

### Basic Usage

```typescript
import { OrpheusClient } from '@orpheus/client';

async function main() {
  const client = new OrpheusClient({ autoConnect: true });

  // Wait for connection
  await new Promise((resolve) => {
    client.on((event) => {
      if (event.type === 'connected') resolve(undefined);
    });
  });

  // Load session
  await client.execute({
    type: 'LoadSession',
    payload: { sessionPath: './session.json' },
    requestId: 'load-1',
  });

  // Subscribe to session changes
  client.subscribe((event) => {
    if (event.type === 'SessionChanged') {
      console.log('Session:', event.payload);
    }
  });
}

main().catch(console.error);
```

### With Error Handling

```typescript
const client = new OrpheusClient();

try {
  await client.connect();
} catch (error) {
  console.error('Failed to connect:', error);
  process.exit(1);
}

try {
  const response = await client.execute({
    type: 'LoadSession',
    payload: { sessionPath: './session.json' },
    requestId: 'load-1',
  });

  if (!response.success) {
    console.error('Command failed:', response.error);
  }
} catch (error) {
  console.error('Execution error:', error);
}
```

### React Integration

```typescript
import { OrpheusClient, DriverType } from '@orpheus/client';
import { useEffect, useState } from 'react';

function useOrpheusClient() {
  const [client] = useState(() => new OrpheusClient());
  const [status, setStatus] = useState(client.status);

  useEffect(() => {
    const unsubscribe = client.on((event) => {
      if (event.type === 'connected' || event.type === 'disconnected') {
        setStatus(client.status);
      }
    });

    client.connect();

    return () => {
      unsubscribe();
      client.disconnect();
    };
  }, [client]);

  return { client, status };
}
```

## Architecture

```
Application
    ↓
@orpheus/client (this package)
    ↓
Driver Selection
    ├─→ Native Driver → @orpheus/engine-native → Orpheus C++ SDK
    └─→ Service Driver → HTTP/WS → orpheusd → Orpheus C++ SDK
```

## Status

**Phase 1 (P1.DRIV.008) - Complete** ✅

- [x] Client broker package structure
- [x] Driver interface and registry
- [x] Service driver implementation
- [x] Native driver implementation
- [x] Automatic driver selection
- [x] Connection management
- [x] Command execution
- [x] Event subscription
- [x] TypeScript type definitions

**Future Enhancements (Phase 2+):**
- [ ] Driver health monitoring
- [ ] Automatic reconnection
- [ ] Command queueing during disconnection
- [ ] Performance metrics
- [ ] Request caching

## Related Packages

- **@orpheus/contract**: Command/event schemas
- **@orpheus/engine-service**: HTTP/WebSocket service driver
- **@orpheus/engine-native**: N-API native driver

## License

MIT
