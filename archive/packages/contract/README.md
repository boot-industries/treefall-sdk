# @orpheus/contract

Contract schemas and TypeScript types for Orpheus SDK command/event communication.

## Overview

This package defines the API boundary between Orpheus drivers (Service, WASM, Native) and client applications. It provides:

- **JSON schemas** for command and event validation
- **TypeScript types** generated from schemas
- **Version manifest** tracking schema evolution
- **Validation utilities** for runtime contract enforcement

## Installation

```bash
pnpm add @orpheus/contract
```

## Usage

### TypeScript Types

```typescript
import type { LoadSessionCommand, SessionChangedEvent } from '@orpheus/contract';

const command: LoadSessionCommand = {
  type: 'LoadSession',
  path: './session.json'
};
```

### Schema Validation

```typescript
import { validateCommand } from '@orpheus/contract';

const result = validateCommand(command);
if (!result.valid) {
  console.error('Invalid command:', result.errors);
}
```

### Accessing Schemas Directly

```typescript
import loadSessionSchema from '@orpheus/contract/schemas/v0.1.0-alpha/commands/LoadSession.json';
```

## Contract Versioning

The contract follows semantic versioning:

- **Major version** changes break compatibility (require migration)
- **Minor version** changes add features (backward compatible)
- **Patch version** changes fix bugs (fully compatible)

See [`MANIFEST.json`](./MANIFEST.json) for the complete version history.

## Development

See [`docs/CONTRACT_DEVELOPMENT.md`](../../docs/CONTRACT_DEVELOPMENT.md) for:
- Adding new commands/events
- Schema validation rules
- Contract evolution guidelines
- ORP062 compliance checklist

## License

MIT
