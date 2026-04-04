# Build Requirements

## C++ Build

### Prerequisites
- CMake ≥3.20
- C++20-capable compiler:
  - Linux: GCC ≥11 or Clang ≥13
  - macOS: Xcode Command Line Tools (Clang)
  - Windows: MSVC 2019+ or MinGW-w64

### Build Steps

```bash
# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build

# Test
ctest --test-dir build --output-on-failure
```

## Node/UI Build

### Prerequisites
- Node.js ≥18.0
- PNPM ≥8.0

### Build Steps

```bash
# Install dependencies
pnpm install

# Build all packages
pnpm run build
```

## Troubleshooting

See [TROUBLESHOOTING.md](TROUBLESHOOTING.md) for common build issues.
