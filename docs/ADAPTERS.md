<!-- SPDX-License-Identifier: MIT -->
# Orpheus Adapters

Orpheus ships with a compact, host-neutral core. Adapters provide thin shims
around the public ABI so the same primitives can participate in different
plugin or application environments. Adapters are optional and can be enabled
individually at configure time.

## Adapter Matrix

| Adapter            | Target host            | CMake option                        | Default |
| ------------------ | ---------------------- | ----------------------------------- | ------- |
| `orpheus_minhost`  | Standalone CLI utility | `-DORPHEUS_ENABLE_ADAPTER_MINHOST`  | `ON`    |
| `reaper_orpheus`   | REAPER extension       | `-DORPHEUS_ENABLE_ADAPTER_REAPER`   | `OFF`   |

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
