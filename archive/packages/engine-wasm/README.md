# @orpheus/engine-wasm

WebAssembly driver for Orpheus SDK - browser-side audio engine with zero-copy performance.

## Features

- **High Performance**: WASM-compiled C++20 core running at near-native speed
- **Security First**: SRI (Subresource Integrity) verification for all WASM artifacts
- **Version Pinning**: Reproducible builds with locked Emscripten version
- **Worker Ready**: Designed to run in Web Workers for non-blocking audio processing
- **Type Safe**: Full TypeScript definitions for all WASM bindings

## Installation

```bash
pnpm add @orpheus/engine-wasm
```

## Usage

### Basic Usage

```typescript
import { loadWASM, isWASMSupported } from '@orpheus/engine-wasm';
import integrity from '@orpheus/engine-wasm/build/integrity.json';

if (!isWASMSupported()) {
  console.error('WebAssembly not supported');
  // Fall back to Service driver
}

// Load with SRI verification
const module = await loadWASM('/assets/wasm/', integrity);

// Initialize
if (module.initialize()) {
  console.log(`Orpheus WASM v${module.getVersion()} ready`);
}

// Shutdown when done
module.shutdown();
```

### With Web Worker

See `src/worker.ts` for the Web Worker wrapper (P2.DRIV.003).

## Building

### Prerequisites

- **Emscripten SDK** version `3.1.45` (locked in `.emscripten-version`)
- **CMake** ≥ 3.20
- **Node.js** ≥ 18

### Install Emscripten

```bash
# Clone emsdk
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk

# Install and activate required version
./emsdk install 3.1.45
./emsdk activate 3.1.45
source ./emsdk_env.sh
```

### Build WASM Module

```bash
# Build WASM + TypeScript
pnpm build

# Or separately:
pnpm build:wasm  # C++ → WASM
pnpm build:ts    # TypeScript → dist/
```

Build outputs:
- `build/orpheus.wasm` - Compiled WASM module
- `build/orpheus.js` - Emscripten glue code
- `build/integrity.json` - SRI hashes for security
- `dist/` - Compiled TypeScript

## Security

### SRI Verification

All WASM artifacts are integrity-checked using SHA-384 hashes:

```typescript
import integrity from '@orpheus/engine-wasm/build/integrity.json';

// loadWASM() automatically verifies:
// - orpheus.wasm matches integrity.files['orpheus.wasm']
// - Content-Type is 'application/wasm'
// - Same-origin policy enforced
```

### MIME Type Requirements

Your server MUST serve WASM with correct MIME type:

```
Content-Type: application/wasm
```

For static hosting (e.g., nginx):
```nginx
types {
  application/wasm wasm;
}
```

### Version Locking

Emscripten version is locked to ensure reproducible builds:

```bash
cat .emscripten-version  # 3.1.45
```

The build script enforces this version and will fail if mismatched.

## Architecture

```
┌─────────────────────────────────────┐
│   Browser / Web Worker              │
├─────────────────────────────────────┤
│  TypeScript Wrapper (loader.ts)    │
│  ↓                                  │
│  Emscripten Glue (orpheus.js)      │
│  ↓                                  │
│  WASM Module (orpheus.wasm)        │
│  ↓                                  │
│  Orpheus C++20 Core                │
└─────────────────────────────────────┘
```

## Current Status (Phase 2 - P2.DRIV.001)

✅ **Completed**:
- Package structure (`packages/engine-wasm/`)
- CMakeLists.txt with Emscripten configuration
- `.emscripten-version` file (3.1.45)
- Build script with version enforcement
- SRI integrity generation
- TypeScript loader with security checks
- Minimal WASM bindings (`getVersion`, `initialize`, `shutdown`)

🚧 **Next Steps** (P2.DRIV.002-005):
- Web Worker wrapper (P2.DRIV.003)
- Command interface with contract integration (P2.DRIV.004)
- Client broker integration (P2.DRIV.005)

## License

MIT - See LICENSE file in repository root
