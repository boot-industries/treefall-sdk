<!-- SPDX-License-Identifier: MIT -->

# Orpheus SDK support matrix

**SDK version:** generated from `project(orpheus VERSION ...)` in the root
`CMakeLists.txt`. Run `python3 tools/version_contract.py --check` to verify the
repository claims.

## Release tiers

| Surface | macOS | Windows | Linux |
| --- | --- | --- | --- |
| Host-neutral core, session, transport, routing, analysis, writer/input | Supported; required CI | Supported; required Debug/Release CI | Supported; required Debug/Release CI |
| Installed CMake package and documented target manifest | Supported; clean-prefix fixture | Source/package validation only; WASAPI is not release-supported | Supported; clean-prefix fixture |
| Dummy audio driver | Supported | Supported | Supported |
| CoreAudio device backend | Supported on Apple platforms | Unavailable | Unavailable |
| WASAPI device backend | Unavailable | Source/fake-test capable only; not release-supported pending package/ABI and physical-hardware evidence | Unavailable |
| ASIO device backend | Unavailable | Optional source-only integration requiring a separately supplied ASIO SDK; excluded from install/export/manifest | Unavailable |
| ALSA device backend | Unavailable | Unavailable | Not implemented |
| JACK device backend | Unavailable | Unavailable | Not implemented |
| PipeWire device backend | Unavailable | Unavailable | Not implemented |

“Supported” means the surface is built, its SDK-owned tests run, and its installed
package is consumed through documented targets in required CI. A device backend is
not promoted to supported solely because its source compiles; it also requires the
backend-specific conformance and real-device acceptance record.

## Fixed remediation policy

- CoreAudio is the only shipped production device backend, and only on Apple.
- WASAPI remains source/fake-test capable but is not release-supported until
  Windows package/ABI and physical-hardware evidence exists.
- ASIO remains opt-in Windows source integration with a separately supplied SDK;
  its target is source-only and never enters install, export, or manifest
  surfaces.
- Linux exposes Dummy only; no production ALSA, JACK, or PipeWire provider is
  claimed.
- `ORPHEUS_ENABLE_AUDIO_CALLBACK_TIMING` defaults OFF. Timing diagnostics are
  compiled and executed only when explicitly enabled and a monitor is attached.
- WASAPI accepts only the requested output-channel count. A mix/closest format
  with another count returns `InvalidParameter` rather than silently changing
  the request.

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

## Linux backend decision

ALSA, JACK, and PipeWire are distinct providers, not interchangeable labels for a
generic Linux backend. Orpheus currently ships none of them and therefore claims
only host-neutral core/package support on Linux. The first provider must land
behind its own CMake target and capability identity, then pass the common device
conformance contract: truthful enumeration, selected-device open, negotiated
format/buffer reporting, callback lifecycle, xrun accounting, and a hardware-backed
acceptance artifact. Until all gates pass, the dummy driver is the only advertised
Linux audio driver.

## Hardware acceptance records

CoreAudio hardware acceptance is exercised on the maintained macOS workstation.
WASAPI has no passing real-device artifact in this repository yet. The manual
`wasapi-hardware-acceptance` workflow is the release gate; ordinary hosted
Windows CI is compile/package conformance only and cannot promote the backend.

## Known unavailable capabilities

- No Linux production device backend is shipped. ALSA, JACK, and PipeWire remain
  separate future capabilities; Orpheus does not advertise a generic Linux driver.
- WASAPI shared-mode enumeration, requested-format negotiation, callback playback,
  and negotiated rate/buffer reporting are implemented and required to compile in
  Windows CI. Promotion from release candidate to Supported is blocked until the
  self-hosted real-device acceptance workflow publishes a passing artifact.
- ASIO requires a separately supplied vendor SDK and is not part of release
  artifacts or required CI.
- Plugin hosting, network audio, WASM/mobile, and device-specific control drivers
  are outside the current released core.
