<!-- SPDX-License-Identifier: MIT -->

# Orpheus SDK support matrix

**SDK version:** generated from `project(orpheus VERSION ...)` in the root
`CMakeLists.txt`. Run `python3 tools/version_contract.py --check` to verify the
repository claims.

## Release tiers

| Surface | macOS | Windows | Linux |
| --- | --- | --- | --- |
| Host-neutral core, session, transport, routing, analysis, writer/input | Supported; required CI | Supported; required Debug/Release CI | Supported; required Debug/Release CI |
| Installed CMake package and documented target manifest | Supported; clean-prefix fixture | Supported; clean-prefix fixture and DLL ABI-link gate | Supported; clean-prefix fixture |
| Dummy audio driver | Supported | Supported | Supported |
| CoreAudio device backend | Supported on Apple platforms | Unavailable | Unavailable |
| WASAPI device backend | Unavailable | Experimental; disabled by default and not release-supported | Unavailable |
| ASIO device backend | Unavailable | Optional source integration requiring a separately supplied ASIO SDK; not release-supported | Unavailable |
| ALSA device backend | Unavailable | Unavailable | Not implemented |
| JACK device backend | Unavailable | Unavailable | Not implemented |
| PipeWire device backend | Unavailable | Unavailable | Not implemented |

“Supported” means the surface is built, its SDK-owned tests run, and its installed
package is consumed through documented targets in required CI. A device backend is
not promoted to supported solely because its source compiles; it also requires the
backend-specific conformance and real-device acceptance record.

## Required toolchain combinations

| Platform | Required CI toolchain | C++ library | Architectures |
| --- | --- | --- | --- |
| macOS | Apple Clang supplied by `macos-latest`, CMake 3.27 | libc++ | arm64 CI; x86_64 not currently a release gate |
| Windows | Visual Studio 2022 MSVC, CMake 3.27, vcpkg x64-windows | MSVC STL | x64 |
| Linux | GCC supplied by `ubuntu-latest`, CMake 3.27 | libstdc++ | x86_64 |

CMake 3.22 is the package minimum. The required compiler must provide C++20.
Toolchains outside this table may work but are not release-qualified until added
to required CI.

## Documented installed targets

The machine-readable manifest is installed at
`share/orpheus/installed-targets.json`. Stable targets for supported host-neutral
scenarios are:

- `Orpheus::core`
- `Orpheus::diagnostics`
- `Orpheus::audio_utils`
- `Orpheus::audio_io`
- `Orpheus::routing`
- `Orpheus::transport`

Platform and optional targets appear only when their build feature and dependencies
are enabled. Consumers must not infer availability from the operating system; they
must test the CMake target or query the driver capability API.

## Known unavailable capabilities

- No Linux production device backend is shipped. ALSA, JACK, and PipeWire remain
  separate future capabilities; Orpheus does not advertise a generic Linux driver.
- WASAPI source is experimental and disabled by default. Shared/exclusive-mode and
  real-device acceptance remain prerequisites for release support.
- ASIO requires a separately supplied vendor SDK and is not part of release
  artifacts or required CI.
- Plugin hosting, network audio, WASM/mobile, and device-specific control drivers
  are outside the current released core.
