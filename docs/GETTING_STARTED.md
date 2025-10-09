# Getting Started with Orpheus SDK

This guide walks you through preparing your environment, building the core, and running the UI preview referenced in ORP061 §Phase 0.

## Prerequisites

- CMake 3.22+
- A C++20-capable compiler (clang or MSVC)
- Node.js 18 LTS and PNPM 8+

## Install Dependencies

```bash
pnpm install
```

If native build prerequisites are missing, consult [TROUBLESHOOTING](TROUBLESHOOTING.md).

## Build the C++ Core

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Run the Shmui UI Package

```bash
pnpm --filter @orpheus/shmui run build
pnpm --filter @orpheus/shmui run test
```

For live development, see the package README within `packages/shmui/`.

## Next Steps

- Explore the [Driver Architecture](DRIVER_ARCHITECTURE.md) document to understand how the engine surfaces across environments.
- Review the [Contract Guide](CONTRACT_DEVELOPMENT.md) to integrate with engine command schemas.
