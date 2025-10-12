# @orpheus/react

React integration for Orpheus SDK - Context provider and hooks for seamless React integration.

## Overview

`@orpheus/react` provides React bindings for the Orpheus SDK, including a Context provider and hooks for command execution and event subscription. Built on top of `@orpheus/client`, it offers a React-friendly API for audio session management.

## Installation

```bash
pnpm add @orpheus/react @orpheus/client react
```

**Optional peer dependencies** (choose based on your driver preference):
```bash
# For native driver (N-API bindings)
pnpm add @orpheus/engine-native

# For service driver (HTTP/WebSocket)
pnpm add @orpheus/engine-service
```

## Quick Start

```tsx
import { OrpheusProvider, useOrpheusCommand } from '@orpheus/react';
import { DriverType } from '@orpheus/client';

// 1. Wrap your app with OrpheusProvider
function App() {
  return (
    <OrpheusProvider
      config={{
        driverPreference: [DriverType.Native, DriverType.Service],
        autoConnect: true,
      }}
    >
      <SessionLoader />
    </OrpheusProvider>
  );
}

// 2. Use hooks in your components
function SessionLoader() {
  const { execute, loading, error } = useOrpheusCommand();

  const handleLoad = () => {
    execute({
      type: 'LoadSession',
      path: './session.json',
    });
  };

  return (
    <button onClick={handleLoad} disabled={loading}>
      {loading ? 'Loading...' : 'Load Session'}
    </button>
  );
}
```

## API Reference

### OrpheusProvider

Context provider that makes OrpheusClient available throughout your React tree.

```tsx
<OrpheusProvider
  config={{
    driverPreference: [DriverType.Native],
    autoConnect: true,
  }}
  onStatusChange={(status) => console.log('Status:', status)}
  onError={(error) => console.error('Error:', error)}
>
  <App />
</OrpheusProvider>
```

**Props:**

| Prop | Type | Description |
|------|------|-------------|
| `config` | `ClientConfig` | OrpheusClient configuration |
| `client` | `OrpheusClient` | Pre-configured client (if provided, config is ignored) |
| `children` | `React.ReactNode` | Child components |
| `onStatusChange` | `(status: ConnectionStatus) => void` | Connection status callback |
| `onError` | `(error: Error) => void` | Error callback |

### useOrpheus

Access the OrpheusClient instance from context.

```tsx
function MyComponent() {
  const { client } = useOrpheus();

  useEffect(() => {
    console.log('Connection status:', client.status);
  }, [client]);

  return <div>Status: {client.status}</div>;
}
```

**Returns:**

```typescript
{
  client: OrpheusClient;
}
```

### useOrpheusCommand

Execute commands with React state management.

```tsx
function LoadButton() {
  const { execute, loading, error, data, reset } = useOrpheusCommand();

  const handleLoad = async () => {
    try {
      const response = await execute({
        type: 'LoadSession',
        path: './session.json',
      });

      if (response.success) {
        console.log('Loaded:', response.result);
      }
    } catch (err) {
      console.error('Failed:', err);
    }
  };

  return (
    <div>
      <button onClick={handleLoad} disabled={loading}>
        Load Session
      </button>
      {loading && <p>Loading...</p>}
      {error && <p>Error: {error.message}</p>}
      {data && <p>Success!</p>}
      <button onClick={reset}>Reset</button>
    </div>
  );
}
```

**Returns:**

```typescript
{
  execute: (command: Command) => Promise<CommandResponse>;
  loading: boolean;
  error: Error | null;
  data: CommandResponse | null;
  reset: () => void;
}
```

### useOrpheusEvents

Subscribe to SDK events with React state management.

```tsx
function EventMonitor() {
  const { latestEvent, events, clear } = useOrpheusEvents({
    eventTypes: ['SessionChanged'],
    onEvent: (event) => {
      console.log('Received:', event);
    },
  });

  return (
    <div>
      <h2>Latest: {latestEvent?.type}</h2>
      <ul>
        {events.map((event, i) => (
          <li key={i}>
            {event.type} - {new Date(event.timestamp).toLocaleTimeString()}
          </li>
        ))}
      </ul>
      <button onClick={clear}>Clear History</button>
    </div>
  );
}
```

**Options:**

```typescript
{
  eventTypes?: string[];              // Filter by event type
  onEvent?: (event: Event) => void;   // Event callback
}
```

**Returns:**

```typescript
{
  latestEvent: Event | null;
  events: Event[];
  clear: () => void;
}
```

## Examples

### Basic Session Management

```tsx
import { OrpheusProvider, useOrpheusCommand, useOrpheusEvents } from '@orpheus/react';

function SessionManager() {
  const { execute, loading } = useOrpheusCommand();
  const { latestEvent } = useOrpheusEvents({
    eventTypes: ['SessionChanged'],
  });

  const loadSession = () => {
    execute({
      type: 'LoadSession',
      path: './my-session.json',
    });
  };

  return (
    <div>
      <button onClick={loadSession} disabled={loading}>
        {loading ? 'Loading...' : 'Load Session'}
      </button>

      {latestEvent && latestEvent.type === 'SessionChanged' && (
        <div>
          Session: {latestEvent.payload.sessionPath}
          <br />
          Tracks: {latestEvent.payload.trackCount}
        </div>
      )}
    </div>
  );
}

function App() {
  return (
    <OrpheusProvider config={{ autoConnect: true }}>
      <SessionManager />
    </OrpheusProvider>
  );
}
```

### Advanced: Manual Client Management

```tsx
import { OrpheusClient } from '@orpheus/client';
import { OrpheusProvider, useOrpheus } from '@orpheus/react';

function App() {
  // Create client with custom configuration
  const [client] = useState(() => new OrpheusClient({
    driverPreference: [DriverType.Service],
    drivers: {
      service: {
        url: 'http://localhost:8080',
        authToken: process.env.REACT_APP_ORPHEUS_TOKEN,
      },
    },
  }));

  return (
    <OrpheusProvider client={client}>
      <MyApp />
    </OrpheusProvider>
  );
}
```

### Real-time Event Monitoring

```tsx
import { useOrpheusEvents } from '@orpheus/react';

function RealtimeMonitor() {
  const { events, clear } = useOrpheusEvents();

  return (
    <div>
      <h2>Event Stream</h2>
      <button onClick={clear}>Clear</button>

      <ul>
        {events.map((event, i) => (
          <li key={i}>
            <strong>{event.type}</strong>
            <pre>{JSON.stringify(event.payload, null, 2)}</pre>
          </li>
        ))}
      </ul>
    </div>
  );
}
```

### Error Handling

```tsx
function ErrorHandlingExample() {
  const { execute, error, reset } = useOrpheusCommand();

  const handleLoad = async () => {
    try {
      const response = await execute({
        type: 'LoadSession',
        path: './session.json',
      });

      if (!response.success) {
        console.error('Command failed:', response.error);
      }
    } catch (err) {
      // Network or other errors
      console.error('Execution failed:', err);
    }
  };

  return (
    <div>
      <button onClick={handleLoad}>Load Session</button>

      {error && (
        <div className="error">
          <p>Error: {error.message}</p>
          <button onClick={reset}>Retry</button>
        </div>
      )}
    </div>
  );
}
```

### TypeScript Integration

```tsx
import type { LoadSessionCommand, SessionChangedEvent } from '@orpheus/react';

function TypeSafeComponent() {
  const { execute } = useOrpheusCommand();
  const { latestEvent } = useOrpheusEvents({
    eventTypes: ['SessionChanged'],
  });

  const handleLoad = () => {
    const command: LoadSessionCommand = {
      type: 'LoadSession',
      path: './session.json',
    };

    execute(command);
  };

  // Type guard for events
  const sessionEvent = latestEvent?.type === 'SessionChanged'
    ? (latestEvent as SessionChangedEvent)
    : null;

  return (
    <div>
      <button onClick={handleLoad}>Load</button>
      {sessionEvent && <p>Tracks: {sessionEvent.payload.trackCount}</p>}
    </div>
  );
}
```

## Best Practices

### 1. Place OrpheusProvider High in Tree

```tsx
// ✅ Good - at app root
function App() {
  return (
    <OrpheusProvider config={{ autoConnect: true }}>
      <Router>
        <Routes />
      </Router>
    </OrpheusProvider>
  );
}

// ❌ Bad - inside routes, creates multiple clients
function Route() {
  return (
    <OrpheusProvider config={{ autoConnect: true }}>
      <Page />
    </OrpheusProvider>
  );
}
```

### 2. Handle Connection Status

```tsx
function App() {
  const [status, setStatus] = useState<ConnectionStatus>();

  return (
    <OrpheusProvider
      config={{ autoConnect: true }}
      onStatusChange={setStatus}
    >
      {status === ConnectionStatus.Connecting && <LoadingScreen />}
      {status === ConnectionStatus.Connected && <MainApp />}
      {status === ConnectionStatus.Error && <ErrorScreen />}
    </OrpheusProvider>
  );
}
```

### 3. Clean Up Event Listeners

```tsx
// ✅ Good - useOrpheusEvents handles cleanup
function Component() {
  useOrpheusEvents({
    onEvent: (event) => console.log(event),
  });
}

// ✅ Also good - manual cleanup
function Component() {
  const { client } = useOrpheus();

  useEffect(() => {
    const unsubscribe = client.subscribe((event) => {
      console.log(event);
    });

    return unsubscribe; // Cleanup on unmount
  }, [client]);
}
```

### 4. Memoize Event Callbacks

```tsx
function Component() {
  const handleEvent = useCallback((event: Event) => {
    console.log('Event:', event);
  }, []);

  useOrpheusEvents({ onEvent: handleEvent });
}
```

## Architecture

```
React Application
    ↓
OrpheusProvider (React Context)
    ↓
@orpheus/client (Driver Broker)
    ├─→ ServiceDriver → orpheusd
    └─→ NativeDriver → C++ SDK
```

## Status

**Phase 1 (P1.UI.001) - Complete** ✅

- [x] OrpheusProvider component
- [x] OrpheusContext
- [x] useOrpheus hook
- [x] useOrpheusCommand hook
- [x] useOrpheusEvents hook
- [x] TypeScript type definitions
- [x] Comprehensive documentation

## Related Packages

- **@orpheus/client**: Unified driver broker
- **@orpheus/contract**: Command/event schemas
- **@orpheus/engine-service**: HTTP/WebSocket driver
- **@orpheus/engine-native**: N-API native driver

## License

MIT
