# Contract Migration Guide

## Migrating from v0.1.0-alpha to v1.0.0-beta

**Version 1.0.0-beta** expands the Orpheus SDK contract with new commands and events for Phase 2 functionality. This guide helps you upgrade your driver or client code.

---

## What's Changed

### New Commands (5 total, 3 new)

**Existing (no changes):**
- `LoadSession` - Load a session file
- `RenderClick` - Render click track

**New in v1.0.0-beta:**
- `SaveSession` - Save current session to file or string
- `TriggerClipGridScene` - Trigger a clip grid scene by ID
- `SetTransport` - Set transport position and playing state

### New Events (6 total, 4 new)

**Existing (no changes):**
- `SessionChanged` - Session state changed
- `Heartbeat` - Periodic heartbeat from driver

**New in v1.0.0-beta:**
- `TransportTick` - Transport position update (≤30 Hz)
- `RenderProgress` - Render progress percentage (≤10 Hz)
- `RenderDone` - Render completed
- `Error` - Structured error with kind/code/message

### Event Frequency Constraints

New events have maximum emission frequencies:
- `TransportTick`: 30 Hz (every ~33ms)
- `RenderProgress`: 10 Hz (every 100ms)
- `Heartbeat`: 0.1 Hz (every 10 seconds)

---

## Migration Steps

### 1. Update Import

**Before (v0.1.0-alpha):**
```typescript
import { Command, Event } from '@orpheus/contract';
```

**After (v1.0.0-beta):**
```typescript
import { Command, Event } from '@orpheus/contract/v1.0.0-beta';
```

**Note:** The default export (`@orpheus/contract`) still points to v0.1.0-alpha for backwards compatibility. Explicitly import from `/v1.0.0-beta` to use the new contract.

### 2. Handle New Commands

If you're implementing a driver, add handlers for new commands:

```typescript
async function handleCommand(command: Command): Promise<void> {
  switch (command.type) {
    case 'LoadSession':
      // Existing handler
      break;
    case 'RenderClick':
      // Existing handler
      break;

    // NEW COMMANDS
    case 'SaveSession':
      await saveSession(command.path);
      break;
    case 'TriggerClipGridScene':
      await triggerScene(command.sceneId);
      break;
    case 'SetTransport':
      await setTransport(command.position, command.playing);
      break;
  }
}
```

### 3. Emit New Events

If you're implementing a driver, emit new events as appropriate:

```typescript
import { EVENT_FREQUENCY_LIMITS } from '@orpheus/contract/v1.0.0-beta';

// Transport tick (≤30 Hz)
let lastTickTime = 0;
const tickInterval = 1000 / EVENT_FREQUENCY_LIMITS.TransportTick;

function onTransportUpdate(position: number, beat: number, bar: number) {
  const now = Date.now();
  if (now - lastTickTime >= tickInterval) {
    emitEvent({
      type: 'TransportTick',
      timestamp: now,
      position,
      beat,
      bar,
      tempo: getCurrentTempo(),
    });
    lastTickTime = now;
  }
}

// Render progress (≤10 Hz)
let lastProgressTime = 0;
const progressInterval = 1000 / EVENT_FREQUENCY_LIMITS.RenderProgress;

function onRenderProgress(percentage: number, renderId: string) {
  const now = Date.now();
  if (now - lastProgressTime >= progressInterval) {
    emitEvent({
      type: 'RenderProgress',
      timestamp: now,
      percentage,
      renderId,
    });
    lastProgressTime = now;
  }
}

// Render done (once per render)
function onRenderComplete(renderId: string, outputPath: string) {
  emitEvent({
    type: 'RenderDone',
    timestamp: Date.now(),
    renderId,
    outputPath,
    sampleRate: 48000,
    channels: 2,
    duration: 120.5,
  });
}

// Structured errors
function onError(kind: 'Validation' | 'Render' | 'Transport' | 'System', code: string, message: string) {
  emitEvent({
    type: 'Error',
    timestamp: Date.now(),
    kind,
    code,
    message,
    details: { /* optional context */ },
  });
}
```

### 4. Update Client Event Handlers

If you're implementing a client, handle new events:

```typescript
client.on('event', (event: Event) => {
  switch (event.type) {
    case 'SessionChanged':
      // Existing handler
      break;
    case 'Heartbeat':
      // Existing handler
      break;

    // NEW EVENTS
    case 'TransportTick':
      updateTransportUI(event.position, event.beat, event.bar);
      break;
    case 'RenderProgress':
      updateProgressBar(event.percentage);
      break;
    case 'RenderDone':
      showRenderComplete(event.outputPath, event.duration);
      break;
    case 'Error':
      handleError(event.kind, event.code, event.message);
      break;
  }
});
```

---

## Validation

### JSON Schema Validation

All schemas are available in `packages/contract/schemas/v1.0.0-beta/`:

```
commands/
  LoadSession.json
  RenderClick.json
  SaveSession.json              # NEW
  TriggerClipGridScene.json     # NEW
  SetTransport.json             # NEW

events/
  SessionChanged.json
  Heartbeat.json
  TransportTick.json            # NEW
  RenderProgress.json           # NEW
  RenderDone.json               # NEW
  Error.json                    # NEW
```

Use these schemas with AJV or similar validators to ensure type safety at runtime.

### TypeScript Type Safety

All commands and events have TypeScript definitions in `src/v1.0.0-beta.ts`. The TypeScript compiler will catch missing handlers or incorrect field types.

---

## Backwards Compatibility

**v0.1.0-alpha is NOT deprecated.** Existing drivers and clients can continue using v0.1.0-alpha indefinitely.

**Migration Timeline:**
- **Now (Phase 2):** New drivers should implement v1.0.0-beta
- **Phase 3+:** v0.1.0-alpha drivers will continue to work but won't support new features

**Default Export Behavior:**
- `import {} from '@orpheus/contract'` → v0.1.0-alpha (backwards compat)
- `import {} from '@orpheus/contract/v1.0.0-beta'` → v1.0.0-beta (new features)

---

## Testing

### Unit Tests

Test new command handlers:

```typescript
import { Command } from '@orpheus/contract/v1.0.0-beta';

test('SaveSession command', async () => {
  const command: Command = {
    type: 'SaveSession',
    path: '/tmp/session.json',
    id: 'cmd-123',
  };

  await handleCommand(command);

  expect(fs.existsSync('/tmp/session.json')).toBe(true);
});
```

Test new event emission:

```typescript
import { Event } from '@orpheus/contract/v1.0.0-beta';

test('TransportTick event', () => {
  const event: Event = {
    type: 'TransportTick',
    timestamp: Date.now(),
    position: 48000,
    beat: 2,
    bar: 1,
    tempo: 120,
  };

  expect(event.type).toBe('TransportTick');
  expect(event.position).toBeGreaterThan(0);
});
```

### Integration Tests

Test complete command/event flows:

```typescript
test('Render workflow', async () => {
  const client = new OrpheusClient();
  await client.connect();

  const events: Event[] = [];
  client.on('event', (event) => events.push(event));

  await client.execute({
    type: 'RenderClick',
    outputPath: '/tmp/click.wav',
    bars: 4,
    bpm: 120,
  });

  // Wait for completion
  await waitFor(() => events.some(e => e.type === 'RenderDone'));

  // Verify progress events
  const progressEvents = events.filter(e => e.type === 'RenderProgress');
  expect(progressEvents.length).toBeGreaterThan(0);
  expect(progressEvents[progressEvents.length - 1].percentage).toBe(100);

  // Verify done event
  const doneEvent = events.find(e => e.type === 'RenderDone');
  expect(doneEvent.outputPath).toBe('/tmp/click.wav');
});
```

---

## Reference

- **Specification:** See ORP062 for complete contract definition
- **Implementation Plan:** See ORP068 for Phase 2 timeline
- **Type Definitions:** See `packages/contract/src/v1.0.0-beta.ts`
- **JSON Schemas:** See `packages/contract/schemas/v1.0.0-beta/`

---

**Questions?** See the main SDK documentation at `/Users/chrislyons/dev/orpheus-sdk/README.md`
