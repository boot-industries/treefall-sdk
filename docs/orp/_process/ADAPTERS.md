<!-- SPDX-License-Identifier: MIT -->
# Orpheus Adapters

Orpheus ships with a compact, host-neutral core. Adapters provide thin shims
around the public ABI so the same primitives can participate in different
plugin or application environments. Adapters are optional and can be enabled
individually at configure time.

## Adapter Matrix

| Adapter            | Target host            | CMake option                        | Default | Status       |
| ------------------ | ---------------------- | ----------------------------------- | ------- | ------------ |
| `orpheus_minhost`  | Standalone CLI utility | `-DORPHEUS_ENABLE_ADAPTER_MINHOST`  | `ON`    | Stable       |
| `reaper_orpheus`   | REAPER extension       | `-DORPHEUS_ENABLE_ADAPTER_REAPER`   | `OFF`   | Experimental |

## Build Examples

Configure the project with adapters at their defaults (core + minhost):

```sh
cmake -S . -B build
```

Opt into the REAPER adapter:

```sh
cmake -S . -B build -DORPHEUS_ENABLE_ADAPTER_REAPER=ON
```

Disable all adapters:

```sh
cmake -S . -B build -DORPHEUS_ENABLE_ADAPTERS=OFF
```

Each adapter links exclusively against the published Orpheus SDK ABI and does
not require any private headers from the core implementation.

## Adapter Status

### REAPER Adapter (Experimental)

The `reaper_orpheus` adapter is currently **experimental** and disabled by default. It provides a modern, ABI-based integration with REAPER that does not depend on legacy WDL/SWELL/WALTER frameworks.

**Key features:**
- ABI-based integration (no direct SDK coupling)
- Panel UI for session visualization
- Session import from JSON
- Click track rendering

**Important notes:**
- Experimental status: API may change between releases
- Limited testing across REAPER versions
- Not recommended for production use

**Legacy code:** Previous REAPER plugin implementations using WDL/SWELL/WALTER have been quarantined to `backup/non_orpheus_20250926/` and are no longer maintained. The current adapter uses a clean ABI approach.
