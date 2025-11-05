# @orpheus/engine-service

**Orpheus Service Driver (orpheusd)** - HTTP/WebSocket service for Orpheus SDK integration.

## Overview

The Service Driver provides a network-based interface to the Orpheus SDK, enabling remote control and monitoring through HTTP REST endpoints and WebSocket event streaming.

## Features

- **HTTP Endpoints:**
  - `GET /health` - Service health check
  - `GET /version` - Service and SDK version information
  - `GET /contract` - Available command/event schemas
  - `POST /command` - Execute Orpheus commands

- **WebSocket Streaming:**
  - `ws://host:port/ws` - Event stream for session state changes, transport updates, and diagnostics

- **Security Defaults:**
  - Binds to `127.0.0.1` (localhost) by default
  - `--host 0.0.0.0` requires explicit flag and logs security warning
  - Token-based authentication support (configurable)
  - Graceful shutdown on `SIGTERM`

## Installation

```bash
pnpm install
pnpm build
```

## Usage

### Start Service (Development)

```bash
pnpm dev
```

### Start Service (Production)

```bash
pnpm start
# or
orpheusd
```

### Command-Line Options

```bash
orpheusd [options]

Options:
  -p, --port <port>        Port to listen on (default: 8080)
  -h, --host <host>        Host to bind to (default: 127.0.0.1)
  --log-level <level>      Log level: trace, debug, info, warn, error (default: info)
  --auth-token <token>     Require authentication token
  --help                   Display help information
  --version                Display version information
```

### Security Warning

When binding to `0.0.0.0` (all interfaces), the service logs a security warning:

```
⚠️  WARNING: Binding to 0.0.0.0 exposes this service to your network.
   Ensure firewall rules and authentication are properly configured.
```

## API Examples

### Health Check

```bash
curl http://127.0.0.1:8080/health
# {"status":"ok","uptime":123.456}
```

### Load Session

```bash
curl -X POST http://127.0.0.1:8080/command \
  -H "Content-Type: application/json" \
  -d '{
    "type": "LoadSession",
    "payload": {
      "sessionId": "my-session",
      "bpm": 120,
      "sampleRate": 48000
    }
  }'
```

### WebSocket Event Stream

```bash
# Using wscat
wscat -c ws://127.0.0.1:8080/ws

# Server will emit events like:
# {"type":"SessionChanged","payload":{"sessionId":"my-session","bpm":120}}
# {"type":"Heartbeat","payload":{"timestamp":1234567890}}
```

## Development

### Project Structure

```
packages/engine-service/
├── src/
│   ├── bin/
│   │   └── orpheusd.ts       # CLI entry point
│   ├── server.ts             # Fastify server setup
│   ├── routes/               # HTTP route handlers
│   │   ├── health.ts
│   │   ├── version.ts
│   │   ├── contract.ts
│   │   └── command.ts
│   ├── websocket.ts          # WebSocket handler
│   ├── types.ts              # TypeScript types
│   └── index.ts              # Public API exports
├── package.json
├── tsconfig.json
└── README.md
```

### Testing

```bash
pnpm test          # Run tests once
pnpm test:watch    # Watch mode
```

### Linting and Formatting

```bash
pnpm lint          # Check for issues
pnpm lint:fix      # Auto-fix issues
pnpm format:check  # Check formatting
pnpm format:write  # Apply formatting
```

## Architecture

The Service Driver is an **optional adapter** that bridges the Orpheus core (C++) with web-based UIs. It does not replace the core engine but provides a convenient remote interface.

**Key Design Principles:**
- **Offline-first:** Core audio operations work without network dependencies
- **Host-neutral:** Service is one of multiple driver options (alongside Native, WASM)
- **Localhost-only by default:** Security-conscious defaults
- **Graceful degradation:** Service failures don't crash audio threads

## Integration

The Service Driver integrates with:
- `@orpheus/contract` - Command/event schemas
- `@orpheus/client` - Client-side driver broker (connects UI to service)
- Orpheus C++ core - Linked as native library (Phase 1, TASK-018)

## Roadmap

- [x] P1.DRIV.001 - Service foundation (HTTP/WebSocket endpoints)
- [ ] P1.DRIV.002 - Command handler (link to C++ core)
- [ ] P1.DRIV.003 - Event emission (stream session changes)
- [ ] P1.DRIV.004 - Authentication (token-based auth)

## License

MIT - See LICENSE file in repository root.
