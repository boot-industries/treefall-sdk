# Contract Development Guide

**Audience:** SDK developers extending contract schemas
**Phase:** All phases
**Last Updated:** 2025-10-12

## Overview

The contract boundary ensures type-safe, version-aware communication between Orpheus clients and drivers. This guide provides step-by-step instructions for adding new commands/events and maintaining schema compatibility.

**Key Principles:**
- Type safety via JSON Schema + TypeScript
- Version negotiation via handshake protocol
- Backward compatibility for minor/patch updates
- Breaking changes only on major version bumps

## Quick Reference

| Task | Command | Location |
|------|---------|----------|
| Add new command | `pnpm add:command <CommandName>` | `packages/contract/schemas/` |
| Add new event | `pnpm add:event <EventName>` | `packages/contract/schemas/` |
| Validate schemas | `pnpm run validate:schemas` | `packages/contract/` |
| Generate manifest | `pnpm run generate:manifest` | `packages/contract/` |
| Update types | `pnpm run build` | `packages/contract/` |

## Contract Package Structure

```
packages/contract/
├── schemas/
│   └── v0.1.0-alpha/
│       ├── commands/
│       │   ├── LoadSession.json
│       │   └── RenderClick.json
│       ├── events/
│       │   ├── SessionChanged.json
│       │   └── Heartbeat.json
│       └── errors/
│           └── ErrorResponse.json
├── src/
│   ├── schemas.ts        # Exported schema registry
│   ├── types.ts          # Generated TypeScript types
│   └── manifest.ts       # Version manifest
├── scripts/
│   ├── validate-schemas.ts
│   └── generate-manifest.ts
├── MANIFEST.json         # Schema version tracking
└── package.json
```

## Adding a New Command

### Step 1: Create Command Schema

Create a new JSON Schema file in `packages/contract/schemas/<version>/commands/`:

**Example: `GetSessionInfo.json`**
```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "$id": "https://orpheus-sdk.dev/schemas/v0.1.0-alpha/commands/GetSessionInfo.json",
  "title": "GetSessionInfo",
  "type": "object",
  "required": ["type"],
  "properties": {
    "type": {
      "type": "string",
      "const": "GetSessionInfo"
    }
  },
  "additionalProperties": false
}
```

**With Parameters:**
```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "$id": "https://orpheus-sdk.dev/schemas/v0.1.0-alpha/commands/ExportSession.json",
  "title": "ExportSession",
  "type": "object",
  "required": ["type", "outputPath", "format"],
  "properties": {
    "type": {
      "type": "string",
      "const": "ExportSession"
    },
    "outputPath": {
      "type": "string",
      "description": "Absolute path to output file"
    },
    "format": {
      "type": "string",
      "enum": ["json", "xml", "otio"],
      "description": "Export format"
    },
    "includeMetadata": {
      "type": "boolean",
      "default": true,
      "description": "Include session metadata in export"
    }
  },
  "additionalProperties": false
}
```

### Step 2: Define Response Schema

If your command returns data, define the response structure:

**Example: `GetSessionInfoResponse.json`**
```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "$id": "https://orpheus-sdk.dev/schemas/v0.1.0-alpha/responses/GetSessionInfoResponse.json",
  "title": "GetSessionInfoResponse",
  "type": "object",
  "required": ["success"],
  "properties": {
    "success": {
      "type": "boolean"
    },
    "result": {
      "type": "object",
      "properties": {
        "sessionName": { "type": "string" },
        "trackCount": { "type": "number" },
        "tempo": { "type": "number" },
        "duration": { "type": "number" }
      }
    },
    "error": {
      "$ref": "../errors/ErrorResponse.json"
    }
  }
}
```

### Step 3: Update Schema Registry

Add your command to `packages/contract/src/schemas.ts`:

```typescript
// packages/contract/src/schemas.ts
import GetSessionInfoSchema from '../schemas/v0.1.0-alpha/commands/GetSessionInfo.json';

export const CommandSchemas = {
  LoadSession: LoadSessionSchema,
  RenderClick: RenderClickSchema,
  GetSessionInfo: GetSessionInfoSchema, // Add here
};

export type CommandType = keyof typeof CommandSchemas;
```

### Step 4: Generate TypeScript Types

Run the build to generate TypeScript types:

```bash
cd packages/contract
pnpm run build
```

This generates types in `src/types.ts`:
```typescript
export interface GetSessionInfoCommand {
  type: 'GetSessionInfo';
}

export type Command =
  | LoadSessionCommand
  | RenderClickCommand
  | GetSessionInfoCommand; // Auto-added
```

### Step 5: Implement Command Handler

**Service Driver** (`packages/engine-service/src/handlers/commands.ts`):
```typescript
async function handleGetSessionInfo(): Promise<CommandResponse> {
  try {
    if (!session) {
      return {
        success: false,
        error: {
          code: 'session.not_loaded',
          message: 'No session is currently loaded',
        },
      };
    }

    const info = session.getSessionInfo();

    return {
      success: true,
      result: {
        sessionName: info.name,
        trackCount: info.tracks.length,
        tempo: info.tempo,
        duration: info.duration,
      },
    };
  } catch (error) {
    return {
      success: false,
      error: {
        code: 'session.info',
        message: error.message,
        details: { stack: error.stack },
      },
    };
  }
}
```

**Native Driver** (`packages/engine-native/src/session_wrapper.cpp`):
```cpp
Napi::Value SessionWrapper::GetSessionInfo(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::Object result = Napi::Object::New(env);

  if (!session_) {
    result.Set("success", false);
    Napi::Object error = Napi::Object::New(env);
    error.Set("code", "session.not_loaded");
    error.Set("message", "No session is currently loaded");
    result.Set("error", error);
    return result;
  }

  result.Set("success", true);
  Napi::Object resultData = Napi::Object::New(env);
  resultData.Set("sessionName", session_->name());
  resultData.Set("trackCount", static_cast<uint32_t>(session_->tracks().size()));
  resultData.Set("tempo", session_->tempo());
  result.Set("result", resultData);

  return result;
}
```

### Step 6: Update Driver Capabilities

Add the command to driver capability declarations:

**Service Driver** (`packages/engine-service/src/index.ts`):
```typescript
const capabilities: DriverCapabilities = {
  commands: [
    'LoadSession',
    'RenderClick',
    'GetSessionInfo', // Add here
  ],
  events: ['SessionChanged', 'Heartbeat'],
  version: '0.1.0-alpha.0',
  supportsRealTimeEvents: true,
};
```

**Native Driver** (`packages/engine-native/src/drivers/native-driver.ts`):
```typescript
async handshake(): Promise<HandshakeResult> {
  const capabilities: DriverCapabilities = {
    commands: [
      'LoadSession',
      'RenderClick',
      'GetSessionInfo', // Add here
    ],
    events: ['SessionChanged', 'Heartbeat'],
    version: '0.1.0-alpha.0',
    supportsRealTimeEvents: true,
  };
  return { success: true, capabilities };
}
```

### Step 7: Write Tests

Create test file `packages/contract/tests/GetSessionInfo.test.ts`:

```typescript
import { describe, it, expect } from 'vitest';
import { CommandSchemas } from '../src/schemas';
import Ajv from 'ajv';

describe('GetSessionInfo Command', () => {
  const ajv = new Ajv();
  const validate = ajv.compile(CommandSchemas.GetSessionInfo);

  it('validates minimal command', () => {
    const command = { type: 'GetSessionInfo' };
    expect(validate(command)).toBe(true);
  });

  it('rejects invalid type', () => {
    const command = { type: 'InvalidCommand' };
    expect(validate(command)).toBe(false);
  });

  it('rejects additional properties', () => {
    const command = { type: 'GetSessionInfo', extra: 'field' };
    expect(validate(command)).toBe(false);
  });
});
```

Integration test `packages/client/tests/get-session-info.test.ts`:

```typescript
import { describe, it, expect, beforeAll, afterAll } from 'vitest';
import { OrpheusClient, DriverType } from '../src';

describe('GetSessionInfo Integration', () => {
  let client: OrpheusClient;

  beforeAll(async () => {
    client = new OrpheusClient({
      driverPreference: [DriverType.Service],
      drivers: {
        service: { url: 'http://127.0.0.1:8080' },
      },
    });
    await client.connect();
  });

  afterAll(async () => {
    await client.disconnect();
  });

  it('returns session info after loading', async () => {
    // Load session first
    await client.execute({
      type: 'LoadSession',
      path: './fixtures/test-session.json',
    });

    // Get info
    const response = await client.execute({
      type: 'GetSessionInfo',
    });

    expect(response.success).toBe(true);
    expect(response.result).toMatchObject({
      sessionName: expect.any(String),
      trackCount: expect.any(Number),
      tempo: expect.any(Number),
    });
  });

  it('returns error when no session loaded', async () => {
    const response = await client.execute({
      type: 'GetSessionInfo',
    });

    expect(response.success).toBe(false);
    expect(response.error?.code).toBe('session.not_loaded');
  });
});
```

### Step 8: Update Documentation

Add command to driver integration guide `docs/DRIVER_INTEGRATION_GUIDE.md`:

```markdown
### Session Information

```typescript
const response = await client.execute({
  type: 'GetSessionInfo',
});

if (response.success) {
  console.log('Session:', response.result.sessionName);
  console.log('Tracks:', response.result.trackCount);
  console.log('Tempo:', response.result.tempo);
}
```
```

## Adding a New Event

### Step 1: Create Event Schema

Create JSON Schema in `packages/contract/schemas/<version>/events/`:

**Example: `RenderProgress.json`**
```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "$id": "https://orpheus-sdk.dev/schemas/v0.1.0-alpha/events/RenderProgress.json",
  "title": "RenderProgress",
  "type": "object",
  "required": ["type", "timestamp", "progress"],
  "properties": {
    "type": {
      "type": "string",
      "const": "RenderProgress"
    },
    "timestamp": {
      "type": "number",
      "description": "Unix timestamp in milliseconds"
    },
    "progress": {
      "type": "number",
      "minimum": 0,
      "maximum": 100,
      "description": "Render completion percentage"
    },
    "sequenceId": {
      "type": "number",
      "description": "Event sequence number for ordering"
    }
  },
  "additionalProperties": false
}
```

### Step 2: Update Event Registry

Add to `packages/contract/src/schemas.ts`:

```typescript
export const EventSchemas = {
  SessionChanged: SessionChangedSchema,
  Heartbeat: HeartbeatSchema,
  RenderProgress: RenderProgressSchema, // Add here
};

export type EventType = keyof typeof EventSchemas;
```

### Step 3: Emit Event from Drivers

**Service Driver** (`packages/engine-service/src/handlers/render.ts`):
```typescript
async function renderWithProgress(spec: RenderClickSpec): Promise<void> {
  let progress = 0;

  const progressCallback = (percent: number) => {
    progress = percent;

    // Emit RenderProgress event
    const event: RenderProgressEvent = {
      type: 'RenderProgress',
      timestamp: Date.now(),
      progress: Math.min(100, Math.max(0, percent)),
      sequenceId: getNextSequenceId(),
    };

    broadcastEvent(event); // Send to all WebSocket clients
  };

  await orpheus.renderClick(spec, progressCallback);
}
```

**Native Driver** (`packages/engine-native/src/session_wrapper.cpp`):
```cpp
void SessionWrapper::EmitRenderProgress(double progress) {
  if (callbacks_.empty()) return;

  Napi::Env env = callbacks_[0].callback.Env();
  Napi::Object event = Napi::Object::New(env);

  event.Set("type", "RenderProgress");
  event.Set("timestamp", static_cast<double>(
    std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count()));
  event.Set("progress", progress);
  event.Set("sequenceId", ++sequence_id_);

  EmitEvent(env, event);
}
```

### Step 4: Subscribe in Client

```typescript
const unsubscribe = client.subscribe(
  (event) => {
    if (event.type === 'RenderProgress') {
      console.log(`Render progress: ${event.progress}%`);
    }
  },
  { eventTypes: ['RenderProgress'] }
);
```

### Step 5: Write Event Tests

```typescript
describe('RenderProgress Event', () => {
  it('validates correct event', () => {
    const event = {
      type: 'RenderProgress',
      timestamp: Date.now(),
      progress: 50,
      sequenceId: 1,
    };

    expect(validate(event)).toBe(true);
  });

  it('rejects progress out of range', () => {
    const event = {
      type: 'RenderProgress',
      timestamp: Date.now(),
      progress: 150, // Invalid: > 100
    };

    expect(validate(event)).toBe(false);
  });
});
```

## Schema Versioning Workflow

### Semantic Versioning

Contract versions follow **SemVer**:

- **MAJOR** (`v1.0.0` → `v2.0.0`): Breaking changes (rename field, remove command)
- **MINOR** (`v1.0.0` → `v1.1.0`): Backward-compatible additions (new optional field, new command)
- **PATCH** (`v1.0.0` → `v1.0.1`): Bug fixes (typo in description, no schema changes)

### Version Lifecycle

```
v0.9.0 (alpha) → v1.0.0-beta → v1.0.0 (stable) → v1.1.0 (minor) → v2.0.0 (major)
```

### Creating a New Version

**Step 1: Create new version directory**
```bash
cd packages/contract
cp -r schemas/v0.1.0-alpha schemas/v1.0.0
```

**Step 2: Update schema $id fields**
```json
{
  "$id": "https://orpheus-sdk.dev/schemas/v1.0.0/commands/LoadSession.json"
}
```

**Step 3: Update manifest**
```bash
pnpm run generate:manifest
```

**Step 4: Update package version**
```json
{
  "name": "@orpheus/contract",
  "version": "1.0.0"
}
```

**Step 5: Create migration guide**
Create `docs/migrations/v0-to-v1.md` documenting breaking changes and migration path.

### Manifest System

The manifest tracks all contract versions with checksums:

**`packages/contract/MANIFEST.json`:**
```json
{
  "currentVersion": "1.0.0",
  "currentPath": "schemas/v1.0.0",
  "checksum": "sha256:abc123...",
  "availableVersions": [
    {
      "version": "0.9.0",
      "path": "schemas/v0.9.0",
      "checksum": "sha256:def456...",
      "status": "deprecated"
    },
    {
      "version": "1.0.0",
      "path": "schemas/v1.0.0",
      "checksum": "sha256:abc123...",
      "status": "stable"
    }
  ]
}
```

**Generate manifest:**
```bash
cd packages/contract
pnpm run generate:manifest
```

**Validate checksums:**
```bash
pnpm run validate:manifest
```

## Testing Requirements

### Schema Validation Tests

**Required for every command/event:**

1. Valid minimal input
2. Valid input with all optional fields
3. Invalid type field
4. Invalid required fields
5. Extra properties rejection

**Example test suite:**
```typescript
describe('LoadSession Schema', () => {
  const validate = ajv.compile(CommandSchemas.LoadSession);

  it('validates minimal command', () => {
    expect(validate({ type: 'LoadSession', path: './session.json' })).toBe(true);
  });

  it('validates with optional fields', () => {
    expect(validate({
      type: 'LoadSession',
      path: './session.json',
      autoPlay: true,
    })).toBe(true);
  });

  it('rejects missing required fields', () => {
    expect(validate({ type: 'LoadSession' })).toBe(false);
  });

  it('rejects additional properties', () => {
    expect(validate({
      type: 'LoadSession',
      path: './session.json',
      unknownField: 'value',
    })).toBe(false);
  });
});
```

### Integration Tests

**Required for every command:**

1. Success case (expected result)
2. Error case (invalid input)
3. Edge cases (empty values, boundary conditions)

**Example:**
```typescript
describe('LoadSession Integration', () => {
  it('loads valid session', async () => {
    const response = await client.execute({
      type: 'LoadSession',
      path: './fixtures/valid-session.json',
    });
    expect(response.success).toBe(true);
  });

  it('returns error for nonexistent file', async () => {
    const response = await client.execute({
      type: 'LoadSession',
      path: './nonexistent.json',
    });
    expect(response.success).toBe(false);
    expect(response.error?.code).toMatch(/file\.not_found|session\.load/);
  });
});
```

### Cross-Driver Testing

**Test each command against all drivers:**

```typescript
describe.each([
  ['Service', DriverType.Service],
  ['Native', DriverType.Native],
])('%s Driver: LoadSession', (name, driverType) => {
  let client: OrpheusClient;

  beforeAll(async () => {
    client = new OrpheusClient({ driverPreference: [driverType] });
    await client.connect();
  });

  it('loads session successfully', async () => {
    const response = await client.execute({
      type: 'LoadSession',
      path: './fixtures/test-session.json',
    });
    expect(response.success).toBe(true);
  });
});
```

## Best Practices

### Schema Design

1. **Use strict types:** Prefer `const` over `enum` for single values
2. **Document fields:** Always include `description` for clarity
3. **Validate early:** Schemas should catch errors before native code execution
4. **Keep backwards compatibility:** Add optional fields, don't remove required ones

### Error Handling

Use consistent error codes:

```typescript
// Good
error: {
  code: 'session.load',
  message: 'Failed to load session',
  details: { path: './session.json', reason: 'File not found' }
}

// Bad
error: {
  message: 'Error loading file'
}
```

### Performance

- **Validate once:** Validate at driver boundary, not in every handler
- **Minimize event frequency:** Cap real-time events (TransportTick ≤30Hz, RenderProgress ≤10Hz)
- **Use sequence IDs:** Allow clients to detect dropped events

### Documentation

- Update `docs/DRIVER_INTEGRATION_GUIDE.md` with examples
- Add JSDoc comments to generated types
- Include migration notes for breaking changes

## Validation Checklist

Before committing contract changes:

- [ ] Schema passes `pnpm run validate:schemas`
- [ ] Manifest updated via `pnpm run generate:manifest`
- [ ] TypeScript types generated via `pnpm run build`
- [ ] All schema tests pass (`pnpm test`)
- [ ] Integration tests pass for all drivers
- [ ] Documentation updated
- [ ] Version bumped according to SemVer
- [ ] Breaking changes documented in migration guide

## Further Reading

- [ORP062 - Engine Contracts Technical Addendum](./integration/ORP062%20Technical%20Addendum_%20Engine%20Contracts%2C%20Drivers%2C%20and%20Integration%20Guardrails.md)
- [ORP063 - Contract Architecture Optimization](./integration/ORP063%20Technical%20Optimization_%20Harmonizing%20Migration%20Strategy%20with%20Contract%20Architecture.md)
- [Driver Integration Guide](./DRIVER_INTEGRATION_GUIDE.md)
- [Performance Budgets](./PERFORMANCE.md)

## Support

For questions or issues:
- GitHub Issues: https://github.com/anthropics/orpheus-sdk/issues
- Examples: `packages/contract/tests/`
