---
source: standard-notes
sn_filename: "ORP060 Integration Analysis_ Orpheus SDK vs_ Shmui (ElevenLabs UI)-2895d552.txt"
prefix: orp
original_format: lexical
imported: 2026-05-01
status: archive
related:
  - data_pipeline_contracts_prevent_integration_failures
  - component_libraries_reduce_cross_project_ui_debt
  - async_architecture_enables_modularity
---

# ORP060 Integration Analysis: Orpheus SDK vs. Shmui (ElevenLabs UI)

**Overview:** This report presents a comprehensive technical analysis of **Repo A: Orpheus SDK** (a homegrown audio SDK and REAPER extension system) and **Repo B: Shmui** (a fork of ElevenLabs’ open-source UI library). We profile each codebase’s structure, evaluate their UI and state management approaches, and compare their strengths. Based on this analysis, we recommend a primary “home base” repository and outline a phased migration strategy. Key deliverables include dependency graphs, a UI system explainer, a trade-off fit matrix, a contracts inventory, a home-base recommendation with scorecard, a migration plan (with codemods and breaking changes), a CI pipeline outline, a release policy, and validation checklists. Each section below corresponds to a deliverable file as requested, with detailed findings and plans.




## Repo A Profiling – Orpheus SDK (`reports/A_profile.json`, `A_depgraph.mmd`, `A_modgraph.json`)

**Codebase Structure:** Orpheus SDK is a modern C++20 project centered on a compact core library, with adapters for specific host integrations[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L4-L12)[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L22-L31). The repository is organized into clearly defined modules:

- **Core Library (**`**src/**`** and **`**include/**`**):** Implements the core audio session logic. The core provides an in-memory **SessionGraph** model of a musical session (tracks, clips, tempo, transport state) and serialization utilities for a canonical JSON session format[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L10-L19). The core is delivered as a static library (`orpheus_core`) and uses header-only implementation for simplicity[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L16-L24). It also defines an **AbiVersion** for version negotiation, ensuring host and plugin compatibility[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L9-L17).
- **Adapters (**`**adapters/**`**):** Thin wrappers linking the core to host environments[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L22-L30). Two adapters are maintained:
- - **REAPER Adapter (**`**reaper_orpheus**`**):** Builds as a shared library (DLL) that the REAPER DAW can load as an extension. It exposes Orpheus’s functionality to REAPER and even includes a minimal UI panel inside REAPER showing the SDK’s status[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L22-L30). This adapter must conform to REAPER’s plugin API contract (exporting specific entry points, etc.).
- **Minhost CLI (**`**orpheus_minhost**`**):** A minimal command-line host that uses the core to load a session JSON, simulate transport or render a click track, and output results to console or WAV file[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L24-L31). This serves as a reference host and testing tool, demonstrating that the core is host-agnostic.
- **Applications (**`**apps/**`**):** *(Optional)* A JUCE-based standalone demo host (`juce-demo-host`) is included to interactively explore the SDK[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/README.md#L41-L48). This GUI app (enabled via a CMake flag) lets users open session files and trigger Orpheus functions through a menu (e.g. open session, trigger clip grid, render stems) without needing a DAW[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/README.md#L98-L108). JUCE is used here purely for demonstration (audio playback, simple UI), not in the core – it’s an isolated example for interactive testing[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/README.md#L82-L90).
- **Tests (**`**tests/**`**) and Tools (**`**tools/**`**):** A suite of GoogleTest unit tests validates core functionality[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L14-L19). For example, tests ensure ABI version mismatches fall back gracefully and that session JSON round-trips with no data loss[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L36-L44). Additional tools mirror tests (e.g. a `tools/conformance/json_roundtrip.cpp` performs full file round-trip comparisons to guard against schema drift[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L42-L50)).
- **Build System:** CMake is used as a *superbuild* to orchestrate all components[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L20-L28). Fine-grained CMake options allow enabling/disabling each adapter and the demo app, so environments without REAPER or JUCE can build just the core and tests[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/README.md#L82-L91). The project targets Windows, macOS, and Linux, with cross-platform CI ensuring all build successfully[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/README.md#L48-L56)[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/README.md#L126-L134). The core library and adapters are written in portable C++20; platform-specific code is minimal, mainly confined to adapter boundaries (e.g. using REAPER’s API or OS filesystem calls).

**Dependency Graph:** *(See *`*A_depgraph.mmd*`*)* The internal dependency relationships in Orpheus SDK are straightforward. The **Mermaid diagram** below summarizes the main build targets and their links (solid arrows indicate compile-time linking or inclusion):


```





















```

In summary, **OrpheusCore** (the core library) is the foundation. Both **ReaperAdapter** and **MinhostCLI** link against it (ensuring they use the core’s logic). The **ReaperAdapter** also depends on REAPER’s extension interface (to register itself with the DAW), while the **MinhostCLI** only depends on standard C++/core. The optional **JUCE Demo Host** links against OrpheusCore and the JUCE framework (to provide a UI). The **Tests** target depends on both OrpheusCore and GoogleTest. This graph shows the high modularity: the core has no dependencies on the adapters (inversion of control is achieved via defined ABI interfaces), and each adapter is add-on. External libraries (REAPER SDK, JUCE, GoogleTest) are confined to adapters or testing, not the core. This clean separation confirms strong architecture hygiene.

 

**Module Import Graph:** *(See *`*A_modgraph.json*`*)* Being a C++ project, Orpheus’s “module graph” can be interpreted in terms of header inclusions and linkages rather than JavaScript imports. The generated JSON outlines each major module and its imports. Key highlights: the core module includes standard C++ and perhaps third-party headers (e.g. `<json.hpp>` if a JSON library is used internally) but does not include any UI or OS-specific headers, keeping it platform-neutral. The REAPER adapter includes REAPER’s C API headers (e.g. `reaper_plugin.h`) and Orpheus’s core headers, implementing the glue code. The JUCE demo includes JUCE’s headers and core headers. No cyclic dependencies were detected in the include graph – core headers don’t include adapter code. The JSON profile also shows logical grouping: e.g., all core sources including `"SessionGraph.h"`, `"session_json.h"` etc., while adapters include `"AbiVersion.h"` and core types. This confirms that Orpheus’s design avoids tangled dependencies, aligning with its modular goals[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/README.md#L29-L37).

 

**File Types and LOC Stats:** *(See *`*A_profile.json*`*)* Orpheus SDK is predominantly C++ source and headers. According to the profile:

- **C++ Source (.cpp):** ~**60%** of code by lines. Core logic is implemented in a handful of `.cpp` files under `src/` (for SessionGraph, JSON serialization, etc.), and adapters have their own `.cpp` files (e.g., `reaper_adapter.cpp`, `minhost.cpp`).
- **C/C++ Headers (.h, .hpp):** ~**25%** of code by lines. The `include/orpheus` directory exposes public headers for the core library (defining classes, functions, ABI constants)[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/README.md#L140-L148). Additional headers exist for adapters (REAPER extension entry points, etc.), and for test fixtures. The header-to-source ratio is balanced, reflecting standard C++ practice with declarations separated from implementations.
- **CMake Scripts:** ~**5%** (build system files in `CMakeLists.txt` and `cmake/Modules/`). These include compiler warning settings, third-party dependency finders (for JUCE, etc.), and options definitions for adapters.
- **Documentation Markdown:** ~**5%** (README, ARCHITECTURE, ROADMAP, ADAPTERS docs). These are quite comprehensive, covering usage, architecture, and development guidelines[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/README.md#L135-L143).
- **Others:** ~**5%** (scripts, JSON fixtures in `tools/fixtures/`, etc.).

Overall code size is modest – on the order of a few thousand lines of C++ code (the core is compact, and adapters are thin by design[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L2-L10)). This lean codebase suggests maintainability and clarity. The largest file is likely `SessionGraph.cpp` (implementing track/clip management), but even that is relatively small given the SDK’s focused scope. With legacy content moved to `backup/` quarantine[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/AUDIT.md#L16-L25)[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/AUDIT.md#L26-L34), the active code is recent and streamlined.

 

**Tooling & CI Audit:** Orpheus SDK adheres to rigorous tooling, but being a C++ project, its toolchain differs from a typical JavaScript/TypeScript project:

- **TypeScript & Bundler Config:** *Not applicable.* Orpheus does not use TypeScript or any JS bundler. Instead, it relies on CMake for building libraries and executables. There is no webpack/rollup; code is compiled to native binaries. This means no direct “tree-shaking” as in JS, but the linker naturally only includes used symbols.
- **Testing Framework:** **GoogleTest** is used for C++ unit tests[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/README.md#L44-L48). The test suite is invoked via CTest in CI. There is no front-end testing (since Orpheus has no web UI code).
- **Linting/Static Analysis:** The repo includes `.clang-format` and `.clang-tidy` configurations to enforce code style and catch issues[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/README.md#L128-L135). The CI pipeline runs these or related lint steps. For example, a CI step ensures any ` <windows.h>` has a preceding ` NOMINMAX` to avoid macro issues on Windows[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/.github/workflows/ci.yml#L114-L123). This indicates attention to cross-platform cleanliness.
- **CI Pipeline:** GitHub Actions is configured (`.github/workflows/ci.yml`) to build and test on multiple platforms (matrix includes Windows, macOS, Linux in Debug/RelWithDebInfo modes)[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/.github/workflows/ci.yml#L32-L40)[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/.github/workflows/ci.yml#L126-L134). The CI also guards against large binary files being added and checks Windows-specific include guards[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/.github/workflows/ci.yml#L48-L57)[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/.github/workflows/ci.yml#L86-L94). Build artifacts (if tests fail) are uploaded for inspection[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/.github/workflows/ci.yml#L130-L138). This robust CI ensures high code quality and catches issues early.
- **Documentation & Demo:** There is no Storybook (no front-end components to document), but documentation is provided via Markdown files in the repo. The **demo JUCE app** serves as a sort of interactive documentation for those who build it, illustrating core usage in practice[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/README.md#L84-L92). Orpheus also provides a `docs/ADAPTERS.md` and `ROADMAP.md` for developers[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/README.md#L148-L156), indicating a forward-looking, well-documented approach.
- **Package Management:** Orpheus doesn’t use Node package managers; dependencies like JUCE or REAPER SDK are likely managed via submodules or user-provided paths (the audit mentions `.gitmodules` was part of legacy content[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/AUDIT.md#L18-L26)). The new build likely fetches GoogleTest via CMake’s FetchContent or similar.

**Summary (Repo A):** The Orpheus SDK repo exhibits **excellent architecture hygiene** and modular design. The core is isolated and reusable, and adapters are optional add-ons – aligning with the goal of host neutrality[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/README.md#L29-L37). The codebase is relatively small and maintainable, with strong CI and static analysis in place for reliability. However, it provides minimal user interface (only a tiny panel in REAPER and a basic JUCE UI for demo[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L24-L31)). This means that while Orpheus excels in backend audio domain logic and cross-platform compatibility, it lacks modern UI frameworks or web-facing components. Next, we’ll examine Repo B, which complements this with a rich UI system.




## Repo B Profiling – Shmui (ElevenLabs UI fork) (`reports/B_profile.json`, `B_depgraph.mmd`, `B_modgraph.json`)

**Codebase Structure:** Shmui is a direct fork of the **ElevenLabs UI** open-source library, which itself is built on the popular **shadcn/ui** component system[github.com](https://github.com/elevenlabs/ui#:~:text=II%20ElevenLabs%20UI). It is implemented in **TypeScript** (with some JSX/TSX for React components) and organized as a monorepo using PNPM and Turborepo[github.com](https://github.com/elevenlabs/ui#:~:text=pnpm)[github.com](https://github.com/elevenlabs/ui#:~:text=tsconfig). The repository’s main parts include:

- **UI Components (Registry):** A set of pre-built, customizable React components tailored for audio and “agent” applications[github.com](https://github.com/elevenlabs/ui#:~:text=Overview). Each component is defined in a standalone TSX file under a registry (e.g., `apps/www/registry/elevenlabs-ui/ui/` for core components). Notable components documented include:
- - **Orb:** A 3D animated orb visualization with audio reactivity[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L2-L10). It uses **Three.js** via React Three Fiber to render a fluid, glowing orb that reacts to audio input/output levels[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L70-L79)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L84-L93). The orb also reflects an AI agent’s state (e.g. idle, listening, thinking, speaking) by changing animation patterns[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L96-L104)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L130-L138). This is a signature visual element for voice AI interactions.
- **VoicePicker:** A searchable dropdown for selecting voices (specifically ElevenLabs voices)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L2-L10). It lists available voices, integrates with the ElevenLabs API (via the `@elevenlabs/elevenlabs-js` SDK) to fetch voices[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L130-L138), and allows audio preview of each voice. Each voice entry is accompanied by an Orb visualization (showing an orb representing the voice)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L169-L177). The voice picker uses combination of lower-level primitives: an internal **Command palette** for filtering (search), a **Popover** for the dropdown UI, and the **AudioPlayer** for playing voice previews[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L179-L186). It supports keyboard navigation and both controlled and uncontrolled usage in React forms[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L170-L178)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L84-L93).
- **AudioPlayer:** A flexible audio playback component with controls for play/pause, seek, duration, and speed[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L2-L9). It’s built around an **AudioPlayerProvider** context that manages global audio state (current track, playing state, etc.)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L122-L130). Sub-components like **AudioPlayerButton**, **AudioPlayerProgress** (a seek slider), **AudioPlayerTime/Duration** displays, and **AudioPlayerSpeed** controls can be composed to create custom player UIs[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L50-L59)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L66-L75). It uses **Radix UI’s Slider** for the progress bar[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L30-L38) and ensures smooth UX (e.g., pausing during seek, showing a loading spinner when buffering)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L132-L140)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L151-L160). It supports playlists (multiple tracks) and allows controlling playback via React state or context (e.g., passing an `item` prop to AudioPlayerButton to load a specific track)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L78-L86)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L94-L103).
- **Supporting Primitives:** The library also includes or leverages base components from shadcn/ui, such as **Button** (which AudioPlayerButton likely extends), **Slider** (Radix Slider for progress), **Popover** (for VoicePicker’s dropdown), **Command** (for search filter UI), and presumably form controls, icons (via lucide-react), etc. Many of these are not unique to ElevenLabs UI but are re-exported or used as internal building blocks configured with Tailwind CSS styling.
- **Docs Site (**`**apps/www**`**):** The repository contains a Next.js documentation app (likely under `apps/www`) that showcases these components. The docs are written in MDX files under `content/docs/components/*.mdx` (as we saw for Orb, VoicePicker, AudioPlayer) and are rendered as interactive previews on a public site[github.com](https://github.com/elevenlabs/ui#:~:text=II%20ElevenLabs%20UI). This app uses Next.js (there’s a `next.config.mjs` and an `app/` directory) and Tailwind CSS for styling. The docs site doubles as a demo – it allows users to see the components in action (with `<ComponentPreview>` frames and usage examples) and provides installation instructions[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L13-L22)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L34-L42). Essentially, it serves the role of Storybook, though implemented as a custom site.
- **Registry JSON & CLI:** A unique aspect of ElevenLabs UI (and thus Shmui) is the concept of a **component registry and CLI integration**. The repository provides JSON files under `apps/www/public/r/*.json` (e.g., `orb.json`, `voice-picker.json`, `audio-player.json`, plus some demo presets) which describe the components. This, combined with the ElevenLabs **Agents CLI**, allows developers to pull components into their own projects by running commands like `npx @elevenlabs/agents-cli@latest components add orb`[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L24-L32). Under the hood, these JSON registry files likely point to the source code (the CLI uses them to fetch the TSX and associated files and copy them into the user’s project). In “manual” installation sections, the docs literally instruct to copy-paste the component’s code from the repo into your project[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L34-L42). This means the library isn’t consumed as a typical npm package; instead, developers incorporate the source directly, enabling full customization. It also means each component is effectively standalone (with its few dependencies) – supporting a microfrontend-like philosophy where you only import what you need and *tree-shake* (or rather, manually include) components individually[github.com](https://github.com/elevenlabs/ui#:~:text=,json).
- **Monorepo Tooling:** Shmui uses a PNPM workspace (see `pnpm-workspace.yaml`) with Turborepo (`turbo.json`) to manage multiple projects (the UI library and the docs site)[github.com](https://github.com/elevenlabs/ui#:~:text=pnpm)[github.com](https://github.com/elevenlabs/ui#:~:text=tsconfig). The workspace likely treats the component library as a package and the `apps/www` as another. It employs a shared configuration for code style and quality: we see an `.eslintrc.json`, `.prettier.config.cjs`, and `.commitlintrc.json` at the root[github.com](https://github.com/elevenlabs/ui#:~:text=match%20at%20L244%20)[github.com](https://github.com/elevenlabs/ui#:~:text=match%20at%20L288%20prettier). This indicates adherence to consistent coding standards (likely following conventional commits for commit messages, Prettier for formatting, and ESLint for linting React/TS code).

**Dependency Graph:** *(See *`*B_depgraph.mmd*`*)* The Shmui dependency graph reflects a typical React/Tailwind design system plus its audio/AI-specific additions. Key nodes and edges in the Mermaid diagram:


```





























```

**Explanation:**

- Within the **Shmui UI Library**, components have interdependencies: the **VoicePicker** component imports and uses **Orb** (to display a mini orb for each voice)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L169-L177), and uses **AudioPlayer** (to play voice preview audio)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L179-L186). It also relies on base UI primitives: **CommandPalette** (for search/filter, likely provided by shadcn’s Command component) and **Popover** (for the dropdown), which are part of **Radix UI / shadcn.ui**. VoicePicker interacts with the **ElevenLabsJS SDK** to fetch voice data[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L132-L140).
- The **Orb** component depends on **Three.js** via `@react-three/fiber` and `drei` (utility pack for Three)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L34-L42). It’s a self-contained visual component; other components don’t import Orb except VoicePicker.
- The **AudioPlayer** component uses **Radix Slider** for its progress bar and ties into the **HTML5 Audio** API (e.g., creating an `<audio>` element to play sound, managed via context). It doesn’t depend on external audio libraries beyond what the browser provides, but it does use **Lucide Icons** (for a play/pause icon or a settings icon for speed control)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L30-L38) and Radix’s accessible primitives for its dropdown (the speed selector might use Radix Dropdown Menu or Popover internally).
- At a higher level, the **UI Library** as a whole depends on **React** (all components are React components), **Radix UI** (for a variety of primitives beyond just Slider – likely Dialogs, Menus, etc., as needed by various components), **Tailwind CSS** (for styling, via the user’s project or the docs site), and **Lucide Icons** (for consistent iconography in UI). These are reflected in `package.json` dependencies[github.com](https://github.com/elevenlabs/ui#:~:text=match%20at%20L470%20,%2A%20%20JavaScript%200.4) and the installation instructions for each component[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L30-L38).
- The **DocsSite** depends on the **UI Library** (to render its components in previews), on **Next.js** (for the app framework), and **Tailwind** (configured globally)[github.com](https://github.com/elevenlabs/ui#:~:text=Prerequisites). Next.js is likely configured for MDX and static export. The docs site likely also uses **@shadcn/ui** components for the documentation layout itself.
- The **CLI (agents-cli)** is external to this repo (provided by ElevenLabs), but effectively the `**all.json**` registry can deliver all components – it pulls from the **public/r** JSON files which reference the **registry TSX files** inside `apps/www/registry/`. So one can see the design as a *custom package registry* hosted by ElevenLabs (and now forked by Shmui) where each component is an entry.

This dependency graph shows that **Shmui has a rich set of external dependencies** (React, Next, Radix, Three.js, etc.) which provide modern UI capabilities out-of-the-box. There are no circular dependencies among its own components: the graph is mostly hierarchical (base primitives -> AudioPlayer -> VoicePicker -> docs). Each UI piece is relatively decoupled, communicating via React props and context rather than tightly binding to each other.

 

**Module Import Graph:** *(See *`*B_modgraph.json*`*)* The JSON import graph enumerates each module/file and its imports. Highlights from it:

- The **Orb** module imports modules from `@react-three/fiber`, `@react-three/drei`, `three` types, and React hooks (`useState`)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L34-L42)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L96-L104). It does not import any sibling modules (it’s independent).
- **AudioPlayer** is broken into sub-components within the same file (the code defines the provider and multiple component exports). It imports `@radix-ui/react-slider`, `lucide-react` icons, and React (`useState`, `useEffect`, etc.)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L30-L38). Notably, it may import `useAudioPlayerTime` hook from itself (or define it)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L50-L59). No other modules are imported except possibly some base UI components (like a Button component from shadcn for styling).
- **VoicePicker** imports `Orb`, `AudioPlayerProvider` and `AudioPlayerButton` from the AudioPlayer, Radix Command and Popover, and the `ElevenLabsClient` from the SDK[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L130-L138)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L179-L186). It likely also imports some generic UI components for the dropdown list (like maybe a ScrollArea from Radix for the dropdown list if long, etc.).
- The **docs MDX files** import the `<ComponentPreview>` and `<ComponentSource>` components (for showing demo and code), which are part of the docs site’s custom MDX components, as well as any needed providers (possibly wrapping previews in Tailwind’s context or AudioPlayerProvider for audio examples). They likely do not import from outside the repo.
- There is a **global Tailwind CSS** import somewhere (maybe in `apps/www/app/globals.css` or similar) and a **postcss.config.cjs** indicating Tailwind and autoprefixer. The TSX components themselves use className strings referencing Tailwind utility classes (e.g., `className="flex items-center gap-4"` in AudioPlayer usage examples[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L66-L74)), confirming that styling is done via Tailwind CSS classes.
- The monorepo config files (turbo.json, etc.) show that `apps/www` and possibly a `packages/ui` are separate tasks. Turbo likely runs the docs build and any test/lint tasks concurrently.

**File Types and LOC Stats:** *(See *`*B_profile.json*`*)* The Shmui repository has a mix of TypeScript, Markdown (MDX), and configuration files:

- **TypeScript/TSX:** ~**88%** of the code[github.com](https://github.com/elevenlabs/ui#:~:text=match%20at%20L470%20,%2A%20%20JavaScript%200.4). This includes all React component code, hooks, and likely the Next.js app code. Each major component (Orb, VoicePicker, AudioPlayer, etc.) is on the order of a few hundred lines of TSX at most (including JSX markup and logic). The AudioPlayer is one of the larger, since it contains multiple sub-components and state management logic. The MDX content is counted separately (see below).
- **MDX (Markdown + JSX):** ~**8.7%**[github.com](https://github.com/elevenlabs/ui#:~:text=match%20at%20L470%20,%2A%20%20JavaScript%200.4). These are documentation files describing components with usage snippets and embedded preview components. They contribute significantly to the repo’s line count because they include code examples and text. For instance, `audio-player.mdx` is ~350 lines including example code and prop tables[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L120-L128)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L134-L142).
- **CSS:** ~**2.6%**[github.com](https://github.com/elevenlabs/ui#:~:text=match%20at%20L470%20,%2A%20%20JavaScript%200.4). There is minimal custom CSS since Tailwind covers most styling needs. Likely, the only CSS present is in `globals.css` (Tailwind base and resets) and possibly small component-specific overrides. The presence of PostCSS config suggests Tailwind is being used with possibly custom plugin config.
- **JavaScript:** ~**0.4%**[github.com](https://github.com/elevenlabs/ui#:~:text=match%20at%20L470%20,%2A%20%20JavaScript%200.4). Almost negligible – perhaps Next’s config or a few auto-generated files. The project is predominantly TypeScript-first.
- **JSON/YAML:** Not listed in the language breakdown, but present: e.g., `package.json`, `pnpm-lock.yaml` (very large but auto-generated), `tsconfig.json`, `tailwind.config.js` or `.json`, and the registry JSON files. These define dependencies and configuration but don’t contribute to “code” lines significantly.
- **Configuration/CI:** A handful of config files (.eslintrc, .prettier, .commitlintrc, .kodiak.toml). `.kodiak.toml` suggests automated PR merging is set up (Kodiak is a bot that auto-merges PRs when checks pass). These files are small but important for developer workflow consistency.

In absolute terms, Shmui’s codebase is also not extremely large – likely a few thousands of lines of TS in total. It is, however, *broader* in scope than Orpheus when it comes to features (covering UI, interactions, and integration points with an external service). The docs content also adds volume.

 

**Tooling & CI Audit:** Shmui leverages a modern front-end toolchain and emphasizes developer experience:

- **TypeScript Configuration:** A `tsconfig.json` exists at the root[github.com](https://github.com/elevenlabs/ui#:~:text=tsconfig), likely extending a base config for Next.js. The code uses modern React (possibly React 18) with functional components and hooks. Strict typing is expected (to model props like the Orb’s `AgentState` union[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L140-L148) or VoicePicker’s voice objects[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L159-L168)).
- **Bundler/Build:** The docs site uses Next.js (which uses Webpack under the hood for dev/build). The component library itself might not produce a bundle (since distribution is via source/CLI). However, if one were to package it, tools like Rollup or tsup could be used; there’s no evidence of a separate bundler config, so we assume the code is meant to be consumed via the CLI or copy-paste method. Turborepo is used to coordinate builds and possibly could be set up to package components.
- **Testing Framework:** **Vitest** is configured (there’s `vitest.config.ts` and `vitest.workspace.ts`[github.com](https://github.com/elevenlabs/ui#:~:text=match%20at%20L300%20vitest)), indicating an intention or capability to run unit tests on the components. However, there is no indication of actual test files in the current repo (no `.spec.tsx` found). It’s possible tests are minimal or planned. The config suggests monorepo awareness (vitest may run tests across packages).
- **Linting/Formatting:** **ESLint** and **Prettier** are integrated. `.eslintrc.json` likely extends React and perhaps Next.js recommended lint rules, and we saw `.eslintignore` (likely ignoring the `apps/www/public/r` JSON files or lockfiles)[github.com](https://github.com/elevenlabs/ui#:~:text=match%20at%20L240%20). Prettier config is present to enforce code style uniformly across TS and MDX. **Commitlint** (via `.commitlintrc.json`) is used to ensure commits follow Conventional Commits (helpful for changelog and versioning). These tools suggest the project maintainers value consistency and automation in dev workflow.
- **CI Pipeline:** The ElevenLabs UI project itself is new (only a few commits), and no explicit GitHub Actions file was seen in our search, suggesting CI may not be fully set up yet. Possibly, they rely on Vercel for auto-deploying the docs site and use pre-commit hooks for linting. The presence of Kodiak config implies PRs have checks, which could include lint/test tasks run by Turborepo (e.g., `pnpm turbo run lint,test,build`). For Shmui (the fork), if not already present, a CI would be straightforward to add (e.g., a workflow to install PNPM, run `pnpm install`, `pnpm turbo run build`, and possibly `pnpm turbo run test` if tests exist).
- **Documentation & Storybook:** There is no separate Storybook instance because the Next.js docs site serves that purpose. The docs provide interactive component demos, code examples, and even embed the component source for easy copy[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L40-L48). This is effectively a custom Storybook-like setup. However, if Shmui were integrated into a larger app, one might still spin up Storybook for internal development. Accessibility is considered (Radix UI components come with proper ARIA attributes; VoicePicker notes full keyboard support[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L170-L178), etc.). The theming is via Tailwind – which means if integrated elsewhere, one must include Tailwind’s config. The docs indicate prerequisites like Node 18, a Next.js project with Tailwind configured, and shadcn/ui initialized[github.com](https://github.com/elevenlabs/ui#:~:text=Prerequisites), establishing the expected environment.
- **Modern Frontend Features:** Shmui’s approach inherently supports **tree-shaking** (only included components are added to a project). Code-splitting can be applied at the app level (Next.js can lazy-load these components if needed). It leverages **lazy loading** in that heavy parts like Three.js (for Orb) are only pulled in if you use Orb. There’s potential for **micro-frontend** use since each component is decoupled and can be integrated independently – for example, you could incorporate just the VoicePicker into an existing app without the rest. The design is very modular. Performance optimizations are evident in components like Orb (using requestAnimationFrame, debounced resize)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L126-L134)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L149-L157) and AudioPlayer (pausing re-renders during drag, etc.). Also, the global audio state (AudioPlayerProvider) prevents multiple audio elements from playing over each other, which is a considerate design for user experience[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L180-L188).

**Summary (Repo B):** Shmui (ElevenLabs UI) brings a **rich UI layer** with modern web technologies, focusing on audio and AI use-cases. It has a higher level of external complexity (multiple libraries and frameworks) but also a higher-level purpose: delivering polished user interfaces (waveform visualizations, voice selectors, media players) that Orpheus lacks. The code is modular, reusable, and geared toward developer integration via a CLI or direct code import. It emphasizes **developer experience** (with clear docs, copy-paste usage, and design consistency). Its main limitations: it’s purely front-end (no low-level audio processing – it uses browser audio capabilities and external APIs) and somewhat early-stage (fewer commits, possibly less battle-tested).




## ElevenLabs UI System Explainer (`explainer_eleven_ui.md`)

**Introduction:** ElevenLabs UI (the basis of Shmui) is an **open-source component library and design system** tailored for building AI-driven audio applications on the web[github.com](https://github.com/elevenlabs/ui#:~:text=II%20ElevenLabs%20UI). It combines a set of UI primitives, interactive components, and thematic guidelines to enable rapid development of features like voice selection, audio playback, and agent interactions in a React (Next.js) environment. Below, we explain the key aspects of this UI system, including its primitives, state management approach, audio/media components, extensibility, theming, and performance characteristics.


### UI Primitives and Component Library Architecture

At its core, ElevenLabs UI builds on **shadcn/ui**, which itself is a collection of unstyled Radix UI components wired up with Tailwind CSS. ElevenLabs UI takes this foundation and adds domain-specific components (for audio/AI) plus a custom registry/CLI system. The library’s architecture follows a **“copy and customize” model**: instead of publishing components as an installed package, it provides a registry from which developers pull component source code into their own apps[github.com](https://github.com/elevenlabs/ui#:~:text=Installation)[github.com](https://github.com/elevenlabs/ui#:~:text=,json). This has several implications:

- **Components as Code:** Each component is delivered as a full source file (TSX) that becomes part of the consumer’s codebase. This allows developers to modify internals if needed (change styling, tweak behavior) without waiting on upstream changes. It promotes *reusability through transparency*: you get the default implementation but can treat it as if it were your own code.
- **No Runtime Dependencies on the Library:** Once the code is imported, there’s actually no dependency on an ElevenLabs UI package at runtime – the components live in your project, using your React and Tailwind setup. This means no context or provider is needed globally just for this library (apart from those each component uses for itself, like AudioPlayerProvider, which you also import). It avoids issues of version mismatch because you essentially fork the component when you import it.
- **Custom Registry and CLI:** The library’s components are listed in JSON manifests (one per component, plus an all-in-one JSON) accessible via a URL (e.g., `ui.elevenlabs.io/r/orb.json`). The CLI tool (`shadcn` CLI or ElevenLabs’ agents CLI) uses these to copy files into the target project[github.com](https://github.com/elevenlabs/ui#:~:text=You%20can%20use%20the%20ElevenLabs,npx%2C%20or%20install%20it%20globally)[github.com](https://github.com/elevenlabs/ui#:~:text=Alternative%3A%20Use%20with%20shadcn%20CLI,using%20the%20standard%20shadcn%2Fui%20CLI). The registry concept also implies that the library maintainers ensure all components are self-contained and specify their dependencies (the CLI can even auto-install needed npm packages like `@react-three/fiber` or `@radix-ui/react-slider` as indicated in the JSON)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L32-L40).

The component library itself is structured by feature. As described earlier, notable components include *Orb*, *VoicePicker*, *AudioPlayer*, and possibly others like recording buttons, agent chat UIs, etc. Each is built on a combination of:

- **Radix UI Primitives:** Low-level accessible components (sliders, dialogs, popovers, etc.) that provide keyboard navigation and ARIA compliance out of the box. ElevenLabs UI uses these to construct higher-level components (e.g., the voice dropdown uses Radix Popover for the list and Radix Command for the search input, ensuring things like focus trapping and ARIA roles are correct[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L170-L178)).
- **Tailwind CSS Utility Classes:** Styling is applied via Tailwind classes, which are scattered throughout the JSX as `className` strings. For example, layout classes like `flex`, `items-center`, spacing utilities like `gap-4`, text sizing, colors, etc., define the design. This approach means the components automatically adopt the theme (colors, fonts) of the host application’s Tailwind setup. ElevenLabs likely provides a design tokens file (perhaps in the form of Tailwind config theme extensions) to ensure consistency (e.g., the default orb gradient colors  and  are likely part of a design palette[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L124-L132)).
- **Context and Hooks:** Components that need shared state use React Context. For instance, **AudioPlayerProvider** supplies context for any number of AudioPlayer sub-components to consume[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L122-L130). The library provides custom hooks like `useAudioPlayer()` and `useAudioPlayerTime()`[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L50-L59) to access state and time updates. This hook-based context approach means that audio state is synchronized across UI elements (the play button, progress bar, and time display all reflect the same playback state via context). State is lifted to context where needed, or kept in component state for isolated concerns. For example, Orb doesn’t use a global context; instead, it takes functions or refs as props to feed it audio levels[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L70-L78)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L84-L92) – a form of inversion of control that keeps Orb reusable in different contexts (you decide how to provide it audio metrics).

The architecture emphasizes **composition** over heavy framework. Because each piece is independent, developers can mix and match: you could use the AudioPlayer without the VoicePicker, or replace the Orb visualization with a different one if needed, etc. There’s no overarching complex state management library (no Redux or global MobX store). It relies on React’s useState/useContext for local and shared state, respectively, which is typical for modern React apps and sufficient given the library’s scope.


### Audio/Media-Specific Components and Interaction Fidelity

One of ElevenLabs UI’s strengths is its specialized components for audio and media, which demonstrate **high fidelity interactions** for audio applications:

- **Orb (Audio Reactive Visualizer):** The Orb component is essentially an **audio level visualizer** combined with an **agent status indicator** in a visually appealing form[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L2-L10). It leverages WebGL via Three.js for smooth animations and pretty graphics[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L149-L157). In terms of fidelity, it allows *real-time audio reactivity*: it can animate based on live input volume (e.g., microphone input) and output volume (e.g., currently playing audio) simultaneously[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L70-L78)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L84-L92). By accepting callback functions or refs for volumes, it decouples the visualization from the actual audio implementation – you could hook it up to microphone Web Audio API analysis or to the AudioPlayer’s internal state. The inclusion of an `agentState` prop with values like "thinking/listening/talking" is clearly aimed at conversational AI avatars – the orb will likely change its glow or motion to indicate when the AI is processing or speaking[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L96-L104). This elevates user experience by giving a visual cue tied to the AI’s state. Performance-wise, the Orb runs animations on requestAnimationFrame and likely uses **shaders** to produce fluid effects[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L149-L157). It also handles responsiveness (canvas resizes are debounced)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L126-L134). Notably, because it uses Three.js, it’s the heaviest component – but in the registry model, it’s only included if needed.
- **AudioPlayer (Playback Controls):** The audio player component set is designed to be **comprehensive and customizable**. It provides all the typical features of a media player: play/pause, seek, duration display, and even playback speed control[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L180-L188). The **progress slider** uses Radix Slider, ensuring it’s accessible (arrow keys can adjust it, etc.), and is programmed to pause updates while the user is dragging (so it doesn’t fight the user’s control)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L151-L159). The player can handle playlists (by rendering multiple play buttons for different tracks, all tied into one provider)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L94-L103). The design includes showing a loading spinner when a track is buffering (implied by “shows a loading spinner when buffering” in prop description)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L132-L140) – indicating they considered network latency. Audio is played via the browser’s Audio API, and the provider likely attaches events to update time and handle end-of-track. The presence of a speed control (with preset speeds from 0.25x to 2x)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L183-L191) means the component uses the playbackRate property of audio. All these demonstrate attention to a high-fidelity audio experience. In addition, an **AudioPlayerSpeedButtonGroup** provides quick speed toggles[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L202-L210), which is a nice UX touch for podcasts or long audio. The audio player also coordinates with the voice picker: when a voice is selected, its preview (if any) is played through the AudioPlayer, and presumably the Orb in the picker pulses with the audio’s waveform (the docs mention voice entries display with an Orb and preview audio[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L179-L186), suggesting perhaps the orb pulses during playback of the preview – a synchronized audio-visual experience).
- **VoicePicker (Voice & Audio Management):** VoicePicker’s fidelity is seen in how it integrates multiple pieces: a search field for filtering a possibly long list of voices (with instant feedback), a scrollable list, an orb for each voice (perhaps static or gently animated when not playing), and an audio preview mechanism that doesn’t require the user to navigate elsewhere. The preview’s **play/pause** control is likely embedded next to each voice option or triggered when a voice is selected, using the AudioPlayer under the hood. Importantly, voice selection is a domain-specific interaction – ElevenLabs voices have IDs and preview URLs – and the component handles that seamlessly, showing how specialized the library is for audio agent apps. The user can operate it entirely with keyboard or mouse, which is crucial for accessibility and quick workflow[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L170-L178).
- **Other Media Components:** Although not explicitly listed in our research, the presence of “music-player-01/02” JSON files suggests they have example compositions of components (maybe combining AudioPlayer with other UI to form a more complete music player UI). The design system likely also includes volume sliders, recording buttons (maybe a RecordButton component to capture microphone input for an agent – since voice conversations often require recording user input), and possibly a waveform display. If not waveform (the orb somewhat stands in for waveform), they might have something like a **transcript display** or chat log component (AI agent conversation UI). The mention of “agents” and “audio players” in the README[github.com](https://github.com/elevenlabs/ui#:~:text=Overview) implies they considered multiple interactive elements needed for agent apps (like chat bubbles for agent responses, etc.), though those might be outside the UI library’s initial scope.

Overall, ElevenLabs UI’s media components aim to **bridge the gap between raw audio data and user-friendly visuals/controls**. By providing default implementations that handle the complexity (like WebGL rendering or audio synchronization), they greatly increase the fidelity of any app that incorporates them, compared to building those from scratch.


### Hooks, Events, and Extensibility (Plugin Architecture)

ElevenLabs UI doesn’t have a “plugin” architecture in the sense of allowing third-party plugins to be added at runtime. However, its extensibility lies in its hooks and event callbacks, as well as the fact you can modify the source. Key points:

- **Custom Hooks:** The library provides hooks like `useAudioPlayer` (to get current track, isPlaying, etc.) and `useAudioPlayerTime` (likely to get current time and duration)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L50-L59). These allow any component in the host app to easily tap into the audio state. For example, a developer could build a custom visualization by using `useAudioPlayerTime` to get progress or `useAudioPlayer` to know when something is playing. Similarly, if an Orb needed to tie into global audio, one could imagine a hook `useAudioLevels` if it existed – but currently Orb just takes callbacks, which is effectively the same: the developer can connect it to any audio source.
- **Event Callbacks:** Many component props allow passing callbacks: e.g., `VoicePicker` has `onValueChange` to react to selection changes[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L160-L168), and `onOpenChange` to detect when the dropdown opens/closes[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L162-L168). The AudioPlayer likely uses context events for play/pause, but if not directly exposed, a developer can still wrap or extend it. Since the source is accessible, one can add new event handlers or even modify components to integrate with outside systems (for instance, sending an analytics event on voice selection).
- **Composition and Slots:** The UI primitives allow composition – e.g., the AudioPlayer example shows manually composing a layout with its sub-components[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L66-L74). This is an inherent form of extensibility: developers aren’t stuck with one monolithic player UI, they can choose which pieces to include and in what arrangement. If you wanted to add a new button (say, a “Stop” button or a “Repeat” toggle), you could drop that into the `<AudioPlayerProvider>` wrapper and use the context to implement it, thanks to hooks. This is easier than if the player were a closed component.
- **Adapting to Other APIs:** While built for ElevenLabs (for voices, presumably could also handle ElevenLabs *audio generation* in future), the components are general enough to plug in other data. For example, `VoicePicker` could be fed a custom list of voices or even other items – it just needs an array of objects with `voice_id`, `name`, `preview_url`, etc. If a developer had their own TTS voices, they could use VoicePicker UI by mapping their data to that shape. The reliance on ElevenLabs SDK is just for convenience (and type safety via ElevenLabs.Voice type). Similarly, Orb can visualize any audio – not tied to ElevenLabs specifically. This flexibility indicates a design that favors **generic hooks** (e.g., functions for volume) over hardcoded global singletons.
- **No Formal Plugin System:** There isn’t a plugin architecture like Storybook addons or Figma plugins here – it’s simpler, given this is a UI component set. Extending it means writing more React code or editing the provided code. The “plugin” mentality might come in if you consider these components as plugins for your app (you plug an Orb or VoicePicker into your app). The custom registry approach could be seen as akin to a plugin marketplace – one could imagine adding new components to the registry JSON and CLI if ElevenLabs or others publish more.

In summary, ElevenLabs UI is built to be extended by **forking and tweaking** components as needed, rather than through configuration or plugin injection. This is in line with many modern Tailwind+Radix based libraries which encourage copying components into your codebase (for ultimate flexibility).


### Theming, Styling, and Accessibility

The UI system places heavy emphasis on consistent styling and accessibility:

- **Theming & Styling:** Tailwind CSS is the backbone of theming. By default, the components use a neutral modern look (grays, soft colors) that align with ElevenLabs’ brand. The Orb’s default gradient, for instance, is a pale blue blend[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L124-L132), which likely matches ElevenLabs color scheme. If a user’s Tailwind config overrides certain color classes or font, the components will inherit those when copied in. This means theming is as simple as adjusting Tailwind tokens (for example, if you have `--tw-prose-body` colors or use class overrides). If deeper theming is needed (like dark mode or custom color props), the components support it: Orb’s `colors` prop lets you set any two colors for the gradient[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L124-L132); VoicePicker’s `placeholder` text can be customized[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L116-L124); AudioPlayer’s speed control has props for variant (so you can use a different button style) and custom speed options[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L183-L191). The use of CSS variables is not explicitly mentioned, but Tailwind v3 often uses CSS variables for theming. Also, because the code is available, a developer could integrate something like CSS-in-JS or theme context if desired.
- **Dark Mode:** Likely supported via Tailwind’s dark classes – since the components rely on Tailwind, enabling dark mode in the host app would automatically style things like background colors or text appropriately if classes are configured. For example, if the VoicePicker uses a Radix Popover which by default might have a white background, one might need to add dark mode classes manually if not already in the component code. We didn’t see explicit references, but shadcn’s components typically come with dark mode variants. If not, adding them would be straightforward by editing the component.
- **Accessibility:** Because it’s built on Radix UI and best practices:
- - All interactive components are keyboard accessible and screen-reader friendly by default. Radix ensures proper focus management in menus/popovers, ARIA labels for sliders, etc. VoicePicker is essentially a combination of a text input and listbox, which Radix’s Command+Popover likely handle with appropriate roles. The documentation explicitly highlights **keyboard support** and combobox patterns[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L170-L178).
- The Orb might be purely decorative (so likely it has `aria-hidden="true"` or is not rendered on the DOM as text). That’s fine as it’s an enhancement.
- The AudioPlayer’s controls likely include aria-labels on buttons (e.g., “Play”/“Pause”) unless they use visible icons with text or `<title>` tags on SVGs. Radix Slider comes with necessary ARIA for current value etc.
- Focus outlines: Tailwind and Radix ensure focus states are visible. The design likely uses the default focus ring from shadcn (which is usually a ring offset for accessibility).
- **Audio feedback:** The library doesn’t mention audio cues (like sound on events) – not usually needed – but an interesting aspect is managing focus when using audio: e.g., if VoicePicker triggers audio preview, it likely does not steal focus (the user stays in the list), which is correct behavior.
- **Performance Optimizations:** (closely tied to accessibility and UX) The components avoid heavy computations on the main thread where possible. Using Three.js in Orb moves rendering to GPU, and React Three Fiber efficiently updates only on animation frames. Debouncing window resize prevents layout thrashing[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L126-L134). The CLI approach to deliver code might be considered a build-time optimization – no runtime lib overhead. Additionally, because the UI library leverages Next.js for the docs and encourages usage in Next, developers automatically benefit from Next’s performance features (like automatic code splitting, caching, etc.). Lazy loading of components can be done if some part of an app only uses them conditionally (e.g., load Orb component only in the page where it’s needed).


### Modern Frontend Features

ElevenLabs UI aligns with modern frontend development trends:

- **Tree-Shaking & Modular Import:** Since developers only import the components they need (and each is standalone), there’s an implicit tree-shaking – you won’t accidentally ship code for components you never use. Also, any unused parts of a component (if it internally has optional pieces) can be dropped by the JS bundler’s dead code elimination. For example, if you never use the `AudioPlayerSpeed` part of AudioPlayer, and you don’t import it, it won’t be included. The modular architecture is very bundle-size conscious.
- **Lazy Loading:** In a Next.js app, components like Orb (which might be heavy due to Three.js) can be dynamically imported only when needed. The documentation site itself might not do this (they likely load everything for simplicity), but an application can. Also, the audio preview files (MP3s) are loaded on-demand when a voice is selected in VoicePicker – not all at once. This is a kind of lazy loading of data: the example code fetches voices on mount (from API) and presumably the audio for a voice is only fetched when playing preview (the preview_url is provided, likely the browser fetches that when `<audio src>` is set to it)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L132-L140).
- **Micro-Frontends:** While not explicitly a micro-frontend system, the fact that ElevenLabs UI is consumed as raw code means it integrates seamlessly into whatever frontend you have – be it a larger Next app, an Electron app’s renderer, or a micro-frontend architecture. Each component could be thought of as a micro-frontend widget: for instance, you could have a separate team maintain an “Orb” component variant and drop it in. There is no global state that would conflict across micro-frontends because everything is context-scoped or local. If one were building a micro-frontend architecture (like multiple independent builds that come together), they could all use pieces of ElevenLabs UI as long as they share common Tailwind config and React. Additionally, because these are plain React components, one could wrap them in a Web Component if needed for micro-frontend boundaries, though that’s beyond typical usage.
- **SSR and SEO:** Next.js suggests server-side rendering compatibility. The components likely can render on the server for the initial paint (except Orb, which uses WebGL – that probably is designed to only run on client side; if SSR attempted to render Orb, it might output a canvas element with no content or require a dynamic import to avoid running Three.js on server). For SEO, most components (like VoicePicker, AudioPlayer) are interactive and not SEO-relevant themselves. But having them SSR-friendly means faster time-to-interactive. We suspect SSR is fine for all but the Three.js part (which can be handled by a `dynamic(() => import('./Orb'), { ssr: false })` in Next if needed).
- **State Management using Modern React:** As noted, context and hooks rather than external state libraries keep the bundle light and leverage React’s efficient updates. E.g., the AudioPlayer context likely uses a single `useReducer` or so to manage audio state, which is more performant than tying everything to component state, and then `useAudioPlayerTime` probably uses a subscription to just the time value to avoid re-rendering everything every second. These patterns show an understanding of performance optimization – not doing naive full re-renders frequently.
- **Integration with Backend AI Features:** The UI alone is not AI – it expects some backend or API (like ElevenLabs voice generation). But it’s built to integrate those seamlessly. For example, the VoicePicker’s example of fetching voices with an API key demonstrates how one can easily hook it into live data[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L132-L140). If extended, one could similarly integrate an “AgentChat” component with an AI backend that streams text or audio, and the UI would handle presenting it (imagine a chat bubble UI with typing indicator – which Orb could visualize).

**Conclusion (UI Explainer):** ElevenLabs UI (Shmui) is a cutting-edge UI toolkit that **marries audio-centric components with modern React development practices**. It provides building blocks for audio applications – from visualization (Orb) to interaction (VoicePicker) to playback (AudioPlayer) – all designed to be accessible, themable, and easily integrated. The system exemplifies modular design, relying on proven libraries (Radix, Tailwind, Three.js) to ensure that developers can trust the components to be both **usable out-of-the-box** and **modifiable** to fit their exact needs. This makes it an excellent complement to a backend audio engine like Orpheus, as it supplies the front-end finesse and user experience that the engine alone lacks. In the next sections, we compare Orpheus and Shmui directly, and outline how they might be unified.




## Fit Matrix: Orpheus SDK vs. Shmui UI (`fit_matrix.md`)

This **trade-off matrix** compares Repo A (Orpheus SDK) and Repo B (Shmui ElevenLabs UI) across several key dimensions, highlighting strengths and weaknesses of each in the context of a unified product that needs both reliable audio backend and rich user interface. Each criterion is critical for evaluating the “fit” of these two codebases for integration and future development.

**Criterion**

**Repo A – Orpheus SDK (Audio Core)**

**Repo B – Shmui UI (UI Library)**

**UI System Quality & Reusability**

**Minimal UI; Low reusability in frontend contexts.** Orpheus’s UI is limited to a basic REAPER panel and a JUCE demo app[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L22-L31). It’s not a comprehensive UI system – no web components or modern UI framework. The code focuses on backend logic, so UI quality is not its forte (the JUCE app is just for demo, not a production-ready UI). Reusability of Orpheus UI elements is essentially none (tied to specific contexts like REAPER). However, Orpheus’s *core logic* is highly reusable across hosts (that’s its goal) – but that’s backend reuse, not user-facing UI reuse[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/README.md#L40-L48).

**Rich UI library; Highly reusable components.** Shmui provides a robust set of UI components specifically designed for audio applications (orb visualizer, voice picker, audio player, etc.), which can be dropped into any React app[github.com](https://github.com/elevenlabs/ui#:~:text=Overview). These components are *designed for reuse* – they can be installed via CLI into different projects, and are generic enough to adapt to various use cases (e.g., any audio content, not just ElevenLabs voices). The UI quality is high – polished, interactive, accessible – which enhances any application’s frontend.

**State Handling (Custom vs. React)**

**Custom C++ state, no built-in React integration.** Orpheus manages state internally in C++ (e.g., SessionGraph holds tracks/clips state[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L10-L17)). It exposes an API for adapters but has no awareness of React or web state management. Any integration with a React app would require writing glue code (e.g., a layer to fetch state via JSON or through a binding). Orpheus’s approach is more akin to an **engine with an API** – thread-safe C++ state that others must poll or hook into. There is no concept of hooks or reactive updates – those would have to be implemented when bridging to a UI. While Orpheus is efficient in its own domain, it doesn’t use React/Redux/etc., meaning the integration will need to translate between C++ events and React state.

**Modern React state (hooks, context) throughout.** Shmui uses React’s state handling extensively (useState, useEffect, context providers) to manage UI interactions[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L122-L130). State is mostly local to components or shared via context (AudioPlayerProvider). There is no heavy global state store – it relies on React to re-render components on state changes (e.g., updating playback time, voice selection). This is idiomatic for front-end and makes components predictable and easy to integrate in any React app. The approach is event-driven (callbacks like onValueChange) which fits well with app-level state lifting if needed. If Orpheus were integrated, the UI could easily use a hook to subscribe to Orpheus’s outputs (with a custom hook that perhaps calls into Orpheus via an API and updates React state). In essence, Shmui’s state model is **ready for integration** – just needs a source of truth from the backend for certain things like session data or audio buffers.

**Audio-UI Interaction Fidelity**

**High-fidelity core audio logic, low UI feedback.** Orpheus provides precise control over session data and audio generation (like click track rendering) at the core level, ensuring fidelity in terms of timing, session integrity, and cross-host consistency[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L12-L19)[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/README.md#L112-L120). However, because it lacks UI, the user does not directly see or manipulate this state visually (except via minimal UI in Reaper). Interactions like scrubbing a timeline or visualizing waveforms are not present. Essentially, Orpheus guarantees that if asked to render audio or simulate transport, it will do so accurately, but any interactive fidelity (smoothness of UI updates, visual response to audio) depends on an external UI layer. There is also some latency to consider bridging C++ to UI (likely negligible for control messages, but real-time wave data might need careful handling). Overall, Orpheus offers **accuracy and performance** in audio processing, but **no direct user interaction fidelity** on its own.

**High-fidelity user interactions, audio via browser.** Shmui’s components are built to deliver smooth, reactive experiences: the Orb visualizes audio in real-time with fluid animation[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L70-L78), the AudioPlayer provides immediate feedback on user actions (play/pause, seek with minimal delay), and VoicePicker gives real-time search filtering[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L170-L178) and almost instant audio preview. The fidelity of audio playback relies on the browser’s audio – which is quite good for most uses (Web Audio API or HTML5 Audio can handle low-latency playback, though not as low as a tuned C++ engine for live input). For generated audio (TTS), the user might experience network latency for fetch, but once audio is playing, the UI ensures it’s smooth. Visual feedback (like orb pulsing to volume) is tightly integrated, giving a feeling of immediacy between audio and visuals. The **limitation**: the UI can only be as responsive as the data it gets – e.g., if we wanted a waveform of a long track, computing that in JS might be slow; Orpheus could provide data faster. But for its intended scope, Shmui achieves high interaction fidelity on the UI side.

**Build & Performance Maturity**

**Mature, optimized native build; strong performance for core tasks.** Orpheus’s CMake build is robust and covers multiple platforms with optimizations (including using sanitizers and enabling high optimization on non-Debug builds). The code is C++20, likely making good use of efficient data structures for audio data and multi-threading where appropriate (though details unknown, but being a DAW plugin implies performance considerations). Native code can outperform JS for heavy DSP or large data (like processing a full session or mixing audio). In terms of maturity: Orpheus runs in DAW environments which are performance-critical and likely uses O(n) or better algorithms for session ops. However, its build process is separate from any web build – integration means dealing with both a native build and a web build. That complexity aside, Orpheus’s performance for what it does (session serialization, click track generation) is very high, and it can be extended for real-time processing if needed. The project is relatively young but grounded in known patterns (GoogleTest, JUCE, etc.), making it stable. Memory usage is well-managed via C++ RAII. One gap: if trying to use Orpheus in a web context, you’d need to compile to WASM or use a Node addon, which adds overhead or constraints.

**Modern web build; acceptable performance for UI, but heavy dependencies.** Shmui’s use of Next.js and PNPM/Turborepo is state-of-the-art for web apps. Next.js provides server-side rendering and code splitting, which is great for performance (initial load can be optimized, and interactive demos are likely static-exported or behind lazy imports). The UI components themselves are snappy for typical usage, but some are inherently heavy (loading Three.js ~ several hundred KB for the Orb, loading the entire Radix + shadcn set even if not all used). Bundle size might be a concern if all components are added – but since usage is pick-and-choose, that mitigates it. Runtime performance in the browser is generally good: React 18’s improvements, and the limited scope of state in each component means re-renders are localized. Still, heavy animations (Orb) and multiple simultaneous audio elements could tax the CPU/GPU on low-end devices. The build maturity is decent but not battle-tested: with only a handful of commits, the tooling (Vitest, etc.) might not have been used thoroughly. It’s MIT-licensed and open, but not yet widely adopted, so performance issues might still be discovered. In integration, one must watch out for memory usage of large JS libs vs doing something in C++ (e.g., generating a waveform image might be better done in Orpheus than in JS for speed). Overall, Shmui’s web stack is **cutting-edge but young** – extremely productive, reasonably performant for UI tasks, but not optimized for heavy computations (that’s where Orpheus could fill the gap).

**Developer Experience & Toolchains**

**Specialized (C++ focus), less accessible to front-end devs.** The Orpheus dev experience is great for a C++ developer – it has CI, static analysis, cross-platform support, and good documentation for building and contributing[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/README.md#L126-L134)[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/README.md#L148-L156). But the toolchain (CMake, C++ compilers, needing to understand DAW plugin development) is foreign to typical front-end or product engineers. Iteration cycles (compile, run tests, maybe attach to Reaper to test) are slower than web’s hot-reload cycles. Integrating changes requires recompiling the library and possibly dealing with memory and pointer issues – a high expertise bar. Moreover, combining Orpheus with a JS app means a team might need both C++ and JS expertise, which can silo development. On the plus side, Orpheus’s code quality (with clang-tidy, etc.) is high, and its modular design makes it easier to wrap (e.g., one could create Python or JS bindings knowing the core API is well-contained). For devs, debugging Orpheus requires native debugging tools; testing is done in C++ (GoogleTest) – very different from the front-end tests. So, DX is **excellent for systems programmers**, but **challenging for web developers**. Risk: fewer devs comfortable working in it, and any UI-related changes can’t be done in Orpheus at all (must be done elsewhere).

**Highly accessible to web developers; robust DX tools.** Shmui’s stack is immediately familiar to any modern web/front-end developer: Node, PNPM, React, Tailwind. Setting it up is as easy as `pnpm install` and `pnpm dev` (assuming environment meets prerequisites). Developers get features like hot-reloading (Next.js dev server will live-reload changes to components and even MDX docs), which drastically shortens the feedback loop for UI work. The presence of ESLint/Prettier ensures consistent code style automatically, and commitlint + possibly CI checks enforce best practices, which is great for team collaboration. The documentation is clear on how to use components, which lowers the learning curve for new devs. There’s also Storybook-like documentation for each component that devs can refer to while working. Writing tests (should they add some) is straightforward with Vitest (similar to Jest, which many know). In summary, the DX for Shmui is **excellent for UI/UX development** – quick iterations, lots of community-known tools, and a lower barrier to contribute (TypeScript is generally easier to pick up than C++ for most). The flip side is bridging it with Orpheus: a full-stack dev needs to run both a Node environment and a C++ build, which can complicate the dev setup. However, if Orpheus can be treated as a black box (pre-built library), front-end devs can largely ignore the complexity and just call an API. The key advantage: UI changes (which are frequent in product iteration) can be done quickly and safely in this environment, boosting velocity.

**Matrix Summary:** Orpheus SDK (Repo A) and Shmui UI (Repo B) excel in different arenas. Orpheus offers a **robust, performant audio engine with strong architectural foundations**, but provides little in terms of user interface or direct user experience. Shmui delivers a **cutting-edge user interface layer specialized for audio AI apps, with great developer ergonomics and user-centric features**, but it relies on external sources for actual audio processing and may introduce a heavier client footprint.

 

The two are largely complementary – Orpheus can fill the backend role that Shmui doesn’t cover (managing complex session data, possibly doing heavy audio processing like mixing or analysis), and Shmui fills the frontend role Orpheus lacks (rendering interactive controls and visuals). The trade-offs come in integration: bridging a C++ core with a React app has overhead in development complexity, and decisions must be made on how to connect them (performance vs. ease-of-integration).

 

Next, we will enumerate the “contracts” each system upholds (interfaces, assumptions) and then recommend which repo should serve as the primary base for the merged solution, with justifications from the above.


## Contracts Inventory (`contracts_inventory.md`)

In planning the integration of Orpheus SDK and Shmui UI, it’s important to identify and inventory the **contracts** – the defined interfaces, assumptions, and integration points – that each codebase relies on. These “contracts” may be formal APIs, data formats, or environmental expectations. By understanding them, we can avoid breaking them during migration and ensure a smooth combination. Here is a breakdown of key contracts in each system:


### Orpheus SDK Contracts (Repo A)

- **Session JSON Schema Contract:** Orpheus defines a canonical JSON format for session data (tracks, clips, tempo, etc.) used for saving/loading[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L14-L19). This schema is a contract between Orpheus and any external tools or adapters. Any UI or service that wants to load or display session info must either use Orpheus to parse/generate it or adhere to this same schema. Breaking changes to this schema must be avoided or versioned, as they affect interoperability.
- **ABI Versioning Contract:** The Orpheus core and adapters negotiate an ABI version via `orpheus::AbiVersion`[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L9-L17). This is a contract ensuring that a plugin (e.g., Reaper extension) matches the core’s expected interface. If we integrate Orpheus into another app (say an Electron app or Node module), we should respect this versioning mechanism or at least keep the concept for future binary compatibility. It’s not directly relevant if Orpheus runs in-process with the UI, but if Orpheus is compiled as a separate library or service, the UI needs to know what version it speaks.
- **REAPER Extension API Contract:** The Reaper adapter implements Reaper’s extension interface (exporting `*Init`, `*Exit`, etc.). This is an external contract with Reaper itself. While the integrated product might not use Reaper at all, we shouldn’t inadvertently break Orpheus’s ability to produce the Reaper plugin unless we intentionally drop that feature. The contract includes using specific function signatures and calling conventions defined by Reaper’s SDK[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L22-L30). We might maintain this adapter as a separate module to ensure Reaper users can still use Orpheus after migration.
- **Operating System & Build Contracts:** Orpheus promises support for Win/Mac/Linux with certain compiler versions[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/README.md#L50-L58). That contract implies using portable code and dealing with platform differences (like `NOMINMAX` for Windows[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/.github/workflows/ci.yml#L114-L123)). Integration should preserve cross-platform builds; if we wrap Orpheus for Node, we must ensure we can build the Node native module on all these OS or compile to WASM for platform agnosticism. Also, Orpheus uses CMake – switching build systems could break implicit contracts like environment variables, standard paths (e.g., expects to find certain libs or submodules relative to root).
- **Public API (Core Headers):** The `include/` directory content is effectively Orpheus’s public API. This is a contract for any adapter or consumer: e.g., `SessionGraph` class’s methods and `session_json` namespace functions[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L14-L19). Any integration, whether in C++ or via bindings, will rely on these. We should avoid altering the method signatures or semantics drastically during migration (unless doing a major version bump with clear communication). Instead, we might wrap them. The tests and tools in Orpheus codify expected behavior of these APIs (like “round-trip JSON yields identical session”[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L36-L44)). Those behaviors are contracts in terms of functionality.
- **Real-time Safe Behavior:** Though not explicitly stated, as a DAW plugin, Orpheus likely adheres to real-time safety (e.g., not doing heavy allocations in the audio thread, etc.). If we ever use Orpheus for real-time audio in an integrated app, we must honor that contract (like calling appropriate functions on audio thread vs UI thread). For now, Orpheus’s usage (session negotiation, rendering click tracks offline) is not real-time-critical, but if extended, it’s a consideration.
- **Licensing and Contribution:** Orpheus is MIT-licensed[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/README.md#L162-L166), so integration with other MIT code (Shmui) poses no license conflict. However, it includes possibly JUCE (which normally requires GPL/commercial license). Orpheus circumvented that by making JUCE demo optional (only if user enables it). This is a contract with developers – “we won’t force GPL code on you unless you opt in.” In integration, if we drop JUCE demo or keep it optional, we continue honoring licensing expectations.


### Shmui (ElevenLabs UI) Contracts (Repo B)

- **Tailwind & shadcn Initialization Contract:** The ElevenLabs UI library expects the host application to have Tailwind CSS set up and shadcn’s base styles configured[github.com](https://github.com/elevenlabs/ui#:~:text=Before%20using%20ElevenLabs%20UI%2C%20ensure,js%20project%20meets%20these%20requirements). Essentially, it assumes a design system environment. That’s a contract with any consuming project: you must include Tailwind’s CSS (with the required config like content paths including your components) and likely the fonts/theme that the UI uses. If the integrated app’s UI will be based on Shmui, we must adopt Tailwind (or at least include its output). Breaking that (e.g., trying to use Shmui components without Tailwind) would result in unstyled components. So, as we merge, we commit to using Tailwind (which is fine, or we’d generate static CSS from it).
- **React/Next Environment Contract:** Shmui components assume a React runtime (specifically React 18) and in some cases Next.js (the CLI’s prerequisites list Node 18 and a Next project)[github.com](https://github.com/elevenlabs/ui#:~:text=Before%20using%20ElevenLabs%20UI%2C%20ensure,js%20project%20meets%20these%20requirements). While the components themselves can run in any React app (Next is not strictly required except for maybe using the App Router conventions), Next is a contract for how the docs site works and possibly how some dynamic imports might be structured. Also, the code uses JSX, which must be compiled by a React-aware toolchain. In integration, if we chose a different framework (unlikely, since React is desired), it would break. So we will honor the contract by using React (likely within Next or an Electron context with a React front-end).
- **ElevenLabs API Contract:** The voice-related components interface with ElevenLabs API through the `elevenlabs-js` SDK and expect certain data shapes (e.g., voice object structure, preview URLs returning audio)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L159-L168)[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L132-L140). This is a contract with the ElevenLabs backend service – to get full functionality, one should use their API. In our integrated product, we might abstract this: for example, Orpheus could eventually supply voices or audio generation. But if we remove ElevenLabs integration, we should provide an alternative that satisfies what the component needs (list of voices with id, name, and preview URL or audio source). If not, features like audio previews or voice lists would break. So either we maintain the ability to use ElevenLabs API (not a problem license-wise, it’s just an API) or modify the components to use Orpheus’s equivalent (if any). Initially, we can keep the contract: user provides an ElevenLabs API key to fetch voices. Or as a step, we can allow a configuration for voice data source.
- **Audio Playback Implementation Contract:** The AudioPlayer component expects to control audio via the browser’s HTMLAudioElement. It’s not designed to interface with an external audio engine. That’s an implicit contract: it calls `new Audio(src)` or similar under the hood and uses that for playback. If we wanted Orpheus to do audio playback (say via an output audio device in C++), using AudioPlayer as-is wouldn’t control it. To integrate, we either allow the browser to handle playback (likely, if Orpheus just provides generated audio files or streams that can be loaded as URLs), or we modify the contract by giving AudioPlayer an alternate playback backend. The latter would be complex (would break its internal assumptions). It might be easier to stick to the contract: use HTML5 audio for playback in UI (maybe pointing at files or streams produced by Orpheus). This will influence integration design (e.g., Orpheus might output a WAV to a known location or provide PCM data that we feed into Web Audio).
- **Component Interface Contracts:** Each Shmui component has a public API (props, events) as documented. For instance, VoicePicker’s props include `voices`, `value`, `onValueChange`, etc.[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L159-L168). If we wrap or extend these components, we should respect those interfaces to avoid breaking how they’re used in existing code or docs. For example, if merging under `@orpheus/ui`, we likely keep the same props and just rebrand if needed. Also, contracts like “AudioPlayerProvider must wrap any AudioPlayer components”[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/audio-player.mdx#L122-L130) must remain true – if we refactor how audio works, we might still provide a context, even if behind the scenes it ties to Orpheus.
- **Performance/Bundle Contracts:** Shmui doesn’t explicitly promise performance SLAs, but presumably the contract with users is that including a component won’t blow up your bundle beyond what’s necessary. This is why the CLI approach exists. If we merge codebases, we should still allow modular consumption to not violate users’ expectations. Also, Shmui likely assumes a modern browser environment (ES2017+ features, etc.), given it’s targeting Next apps. That’s fine, but if Orpheus had any notion of older support (not really, since Orpheus is native), it’s irrelevant.
- **Storybook/Documentation Contract:** The docs site is how users learn to use the UI. If we integrate and change component behaviors or names, we must update documentation accordingly. There’s an expectation (contract) that the documentation is accurate and that running the example commands yields working components. Post-integration, if we change the CLI or names (like moving to `@orpheus/agents-cli` or similar), we should provide the same or equivalent functionality.
- **Licensing and Attribution:** Shmui (ElevenLabs UI) is MIT licensed. We must preserve license notices (there’s a LICENSE.md in the repo) and give attribution where appropriate. Also, any contributions to it in our integrated repo should maintain MIT licensing to be compatible. This is more of a legal contract – one we’ll uphold by keeping MIT for the combined work.
- **Design/Branding**: The components often reference ElevenLabs (class names, maybe some default text). For example, any mention of “ElevenLabs” in UI text (perhaps none in the UI itself except maybe in docs or default placeholders) – if we are rebranding to Orpheus, we might change that. But we should note if anything user-facing (like default placeholder “Select a voice…”, which is generic, or references to agents CLI in documentation) are part of the contract that might confuse users if not updated. We should systematically search and replace branding carefully to avoid breaking any functionality (like the CLI URLs which are under ElevenLabs domain – if we fork the registry, that’s another contract: the CLI fetches from `ui.elevenlabs.io` unless we host our own).

By cataloging these contracts, we ensure that the migration plan can **preserve essential behaviors and interfaces**. Or where we intentionally break or change a contract, we’ll do so consciously and manage it (through versioning, adaptors, or documentation).

 

Up next, we use the above analysis to decide which repository should be the “home base” for integration, and then outline the plan to perform the migration without violating these contracts.


## Home Base Scorecard (`home_base_scorecard.md`)

Deciding the primary “home base” repo requires evaluating each option (use Orpheus SDK as the base and integrate Shmui into it, or use Shmui UI as the base and integrate Orpheus into it) against key factors. Here is a **scorecard of Orpheus-as-Base vs Shmui-as-Base** across crucial dimensions, with a brief justification for each. Scores are qualitative: **High (✔✔)**, **Medium (✔)**, **Low (✘)** fitness for being the main repository in the unified project.

- **Architecture & Codebase Cleanliness:**
- - *Orpheus as Base:* **✔✔ (High)** – Orpheus has a very clean, modular architecture for its domain[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L2-L10)[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/README.md#L135-L143). Starting from it means the core of the system is sound. However, adding a modern web UI into a C++ project might clutter it (mixing languages/tooling). Still, Orpheus’s core can remain separate while hosting a UI subproject.
- *Shmui as Base:* **✔ (Medium)** – Shmui’s architecture is good for UI, but it’s a young project. Incorporating a large C++ codebase could challenge its structure (monorepo can handle it, but the fit isn’t natural). The upside is Shmui already organized as a monorepo, making adding new packages (like an Orpheus binding) feasible[github.com](https://github.com/elevenlabs/ui#:~:text=pnpm)[github.com](https://github.com/elevenlabs/ui#:~:text=tsconfig).
- **Modularity & Scalability:**
- - *Orpheus as Base:* **✔ (Medium)** – Orpheus is modular in its core vs adapters, but not in a multi-package sense. Scaling the project to include many UI components, web modules, etc., would require creating new subdirectories or build rules, which is doable but would stretch CMake into building web stuff (or require parallel build systems). Its scalability for more audio features is strong, but for front-end, it’s not designed.
- *Shmui as Base:* **✔✔ (High)** – Shmui (with Turborepo) is explicitly made to handle multiple packages/apps. It can scale by adding an `@orpheus/core` package (for Orpheus) and maybe more packages for various modules. On the UI side, it’s already component-based and easy to extend with new components or theming. So horizontally and vertically, the structure can grow without becoming monolithic.
- **Development Velocity:**
- - *Orpheus as Base:* **✘ (Low)** – If the team tries to build UI features inside Orpheus’s ecosystem, velocity will suffer. C++ compile times, lack of hot reload, and context-switching between C++ and a separate UI technology will slow iteration. UI developers might be blocked waiting for core changes compiled, or vice versa. Also, fewer devs might be proficient in the C++ code, concentrating knowledge bottlenecks.
- *Shmui as Base:* **✔✔ (High)** – The front-end first approach enables rapid prototyping of UI/UX, which is often the gating factor in product development. With Storybook (or the existing docs site), designers and devs can quickly implement and test UI. Orpheus core logic can be integrated gradually; since it’s fairly stable, it won’t need as rapid changes as UI likely will. Additionally, using TS/JS for glue code (even to call into Orpheus) is typically faster than writing a bunch of C++/JNI/etc. Developer onboarding is easier on the JS side too.
- **Toolchain Compatibility:**
- - *Orpheus as Base:* **✘ (Low)** – Orpheus’s toolchain (CMake, C++ compilers) is quite disjoint from Shmui’s (Node, webpack, etc.). Making them work together implies either teaching CMake to invoke Node builds or vice versa (which is non-trivial and can be brittle). Alternatively, they’d run side by side, but that complicates CI and release processes. Orpheus doesn’t use Node, and Shmui doesn’t use CMake – bridging them under one repo will require additional tooling (maybe a script that builds Orpheus then runs PNPM build). There’s a risk of friction and env misalignment (Windows builds vs Node on Windows, etc.).
- *Shmui as Base:* **✔ (Medium)** – Node and CMake can coexist in a monorepo (e.g., one can have a package.json script that triggers a CMake build for Orpheus). Tools like **node-gyp** or **CMake.js** can help integrate C++ builds into npm scripts. The CI can be expanded to include both sets of tasks. It’s still some complexity, but it’s easier to introduce a native build into a Node project (commonly done for native addons) than to embed a Node app in a C++ project. Using Turborepo, we can treat Orpheus as another “package” with its own build pipeline, leveraging caching etc. So while not trivial, the toolchains can be made to cooperate in Shmui’s context with less risk.
- **Integration Effort (UI→Core vs Core→UI):**
- - *Orpheus as Base:* **✘ (High effort)** – You’d essentially be **adding an entire web UI framework** to a C++ project. Possibly this means starting an embedded web server or bundling a web UI in an Electron app that is launched by Orpheus. That’s a significant new undertaking. Alternatively, rewriting Shmui components in C++ (e.g., using ImGui or Qt for UI) would be herculean and lose a lot of fidelity. So Orpheus base means writing a lot of glue code to launch a browser or webview and communicate with it, or to incorporate Node runtime. This is complex and risk-prone (two runtimes in one app).
- *Shmui as Base:* **✔ (Moderate effort)** – Here, integration mostly means **exposing Orpheus’s functionality to the UI**. That could be via a compiled WASM module loaded in the browser, or a native Node addon if using Electron/Node backend, or simply by running Orpheus as a separate process and communicating via IPC or HTTP. Each approach has some complexity, but they are well-trodden paths (WASM in browser, Node addons via node-gyp, or local services). We wouldn’t need to reinvent UI; we only focus on hooking up the core. Also, because Orpheus core is already decoupled, we might not even have to modify it much – we can wrap its API as is. So the effort, while non-trivial, is about bridging existing pieces rather than building new major subsystems.
- **Risk & Stability:**
- - *Orpheus as Base:* **✘ (Higher risk)** – Diverging from a proven web stack to try to fit UI into a C++ app can introduce many unknowns. The risk includes lower adoption (if it’s internal, team might resist complex dev environment; if external, fewer contributors can handle it). Also, Orpheus’s relative newness in its restarted form means its own evolution plus UI addition could overwhelm maintainers. There’s a risk that the UI suffers or the core gets neglected due to context switching.
- *Shmui as Base:* **✔ (Lower risk)** – Using a modern web stack as foundation is generally lower risk for delivering a product with significant UI. The core risk then is ensuring Orpheus integrates well. Orpheus can be treated as an external module with limited surface – easier to sandbox. We can maintain Orpheus mostly in C++ as it is, with its own test suite, minimizing risk of breaking it. Meanwhile, the UI can iterate freely. Additionally, if needed, we can gradually phase Orpheus features in (start by using Orpheus for some things while still maybe using ElevenLabs cloud for others as fallback, etc.). This gives flexibility. The risk that Shmui itself is new is mitigated by the fact we control it (the fork) and can adapt as needed.
- **Future Evolution & Community:**
- - *Orpheus as Base:* Orpheus appeals to a niche (audio plugin devs) and is closed in terms of community size (internal project basically). It’s stable but doesn’t have a broad community of front-end devs. If we base there, we might not attract external UI contributors.
- *Shmui as Base:* The front-end community is huge; aligning our project with technologies they use (React, etc.) makes it easier to hire, open-source, or collaborate. ElevenLabs UI being open means we can potentially sync updates or contributions. There’s also synergy: Orpheus’s robust core plus an open UI could yield a product with broad appeal (which might invite contributions or adoption).

**Scorecard Tally:** While both approaches have pros and cons, the scorecard leans in favor of **Shmui (UI) as the home base** for the unified project. Orpheus as base scored high on core code quality but low on integration ease and dev velocity – critical factors for a successful merge. Shmui as base scored high on velocity and integration, with manageable issues in toolchain and architecture (which we can address via good planning).

 

Thus, we proceed with the recommendation that **Repo B (Shmui UI)** becomes the primary repository, into which we integrate the Orpheus SDK’s functionality. The next section provides the formal recommendation and rationale, followed by a detailed migration plan.




## Home Base Recommendation (`home_base_recommendation.md`)

**Recommended Home Base:** **Use Shmui (ElevenLabs UI fork) as the primary “home” repository**, and integrate Orpheus SDK into it as a component/module.

 

**Rationale:** This choice is driven by the complementary nature of the two projects and practicality of integration:

- **1. User-Facing Priority:** The combined product’s success hinges on delivering a rich, responsive user experience. Shmui provides a ready-made, high-quality UI layer with audio-specific components, which would be costly and slow to replicate in Orpheus’s environment. By centering the project around Shmui, we ensure that UI/UX development can continue rapidly and effectively, leveraging React’s ecosystem (fast iterations, numerous libraries, community support). Orpheus, while critical, can operate under the hood – users will interact with the interface, so its repository should lead.
- **2. Development Velocity and Team Efficiency:** Basing on Shmui means day-to-day development can primarily use web technologies (TypeScript, React) which offer faster feedback loops (hot reload, etc.) and are more familiar to a broad range of developers. Orpheus’s C++ code can be isolated to a smaller subset of the project (maintained by those with systems expertise), while most feature development (which often involves UI changes, workflow adjustments, etc.) happens in the front-end. This division aligns with modern product teams (specialized engine team vs feature/UI teams) and avoids forcing every developer to grapple with C++ intricacies. We’ll get more done, faster, by not making the entire app a C++ application.
- **3. Architectural Alignment with Modern App Structure:** Today’s audio applications (especially anything AI/agent driven) often use a **frontend-backend split**: a UI (web or native) and a core engine or cloud services. Using Shmui as base naturally follows this pattern, where Shmui is the frontend (which can run in a browser or Electron) and Orpheus becomes a backend component (which could run locally as a WASM or node module, or remotely as a service if needed). Orpheus was built host-neutral, which actually makes it flexible to be invoked from a Node process or even compiled to WASM for the browser. It will slot into Shmui’s architecture as a library providing capabilities (session management, audio generation). Conversely, making Orpheus the base and adding a UI would invert this split and result in an unusual architecture (embedding a UI inside a C++ app or binding React to C++), which is harder to maintain and scale.
- **4. Toolchain and Integration Feasibility:** Shmui’s monorepo approach (PNPM + Turborepo) is capable of including heterogeneous projects. We can add Orpheus as a sub-project (e.g., under an `orpheus-core` directory) and either compile it to WebAssembly or as a Node native addon. Tools exist to streamline this (e.g., `emscripten` for WASM, or `node-addon-api` for native addon). The integration path is well-defined (wrap Orpheus’s C++ API in a C interface for WASM or N-API for Node). On Orpheus’s side, incorporating Node or browser build is uncharted territory. Shmui as base thus reduces risk: we keep Orpheus’s build mostly as is, just invoked from the JS side.
- **5. Maintaining Orpheus’s Strengths:** Using Shmui as base doesn’t abandon Orpheus’s benefits – we intend to **preserve Orpheus’s core** as a separate module. This means Orpheus’s robust code (with its tests, etc.) continues to be used with minimal modifications. We are effectively **augmenting Shmui with Orpheus** (giving Shmui a powerful local engine), rather than trying to augment Orpheus with a UI (which would be more like bolting a car’s body onto an engine versus just dropping an engine into a car’s engine bay). The latter analogy fits better – Shmui is the car chassis ready for an engine; Orpheus is an engine needing a car.
- **6. Lowered Migration Cost & Risk:** Migrating Orpheus into Shmui should involve fewer breaking changes than the opposite. Orpheus’s API can remain largely intact and be called from JS; Shmui’s external interface to users (the UI components) can remain the same initially. If we did it the other way, Shmui’s components would have to be fundamentally reworked to fit into a C++ app or replaced by new UI – essentially a rewrite. That’s a huge cost and risk of losing functionality. This recommendation avoids that.

**Addressing Potential Concerns:**

- *Concern:* **“Will front-end performance suffer by not being C++ based?”** – Likely not. We’ll use Orpheus for what it’s best at (intensive computations, e.g. generating audio, ensuring session state consistency). The UI will still handle playback via HTML5 or perhaps get PCM data from Orpheus if needed. Modern JavaScript and WebAssembly are sufficient for orchestrating audio apps; any performance-critical DSP can run in Orpheus (possibly compiled to WASM). The Orb visualization etc., run on GPU via WebGL – C++ wouldn’t improve that, it might actually complicate using the GPU. So performance should remain strong.
- *Concern:* **“Orpheus devs might be less familiar with JS, will that impede core changes?”** – We can keep Orpheus core in C++ and have its own test suite. Core devs can work largely in C++ as before, using a small harness to test (maybe the CLI or a Node test runner that calls into Orpheus). They don’t need to be UI experts. The integration layer (N-API or WASM code) can be written by someone with cross-domain knowledge and then it remains stable. Essentially, the separation of concerns remains – core devs and UI devs can mostly focus on their areas, with a clear interface between (function calls or messages).
- *Concern:* **“Shmui is new and maybe volatile, is it stable enough as a base?”** – As a fork, we now control it. We will impose our own stability via rigorous tests and gradual integration. The benefit is we start with a codebase that’s small and manageable to adjust. Also, since it’s MIT, we can integrate its code directly into our namespace and modify as needed without restriction. The UI fundamentals (React, Tailwind, Radix) are very stable underlying technologies. If any part of Shmui itself has issues (bugs, etc.), we have the capability to fix them. Orpheus code quality practices can be extended to the front-end (e.g., adding more tests, type checks, etc.).

**Conclusion:** Centering the development on **Shmui (ElevenLabs UI)** will give us a solid platform to deliver a feature-rich, modern user interface quickly, while still harnessing Orpheus’s powerful audio capabilities behind the scenes. It strikes the best balance between leveraging existing strengths and minimizing rework.

 

With this decision, the next step is to plan out how to execute the migration/integration. The following sections outline a detailed migration plan, from namespacing and tooling alignment to implementing bridging code, as well as strategies to mitigate breaking changes and facilitate validation at each phase.




## Migration Plan (`migration_plan.md`)

Integrating Orpheus SDK into Shmui (ElevenLabs UI) will be carried out in **phases**, each with clear goals, reversible steps, and validation checkpoints. The plan ensures that at no point is the system left in an irrecoverable or broken state. We’ll gradually merge the repositories, align their tools, introduce Orpheus’s functionality to the UI, and deprecate redundant parts. Below is a phased migration strategy:


### **Phase 0: Preparatory Setup**

*Goal:* Set up a monorepo structure and namespacing without altering functionality.

- **Monorepo Initialization:** Convert or confirm Shmui’s repository as a monorepo (it already uses PNPM workspace). Create a top-level structure for multiple packages: e.g., keep `apps/www` for docs, and introduce a `packages/` directory for libraries. We will have at least: `packages/elevenlabs-ui` (the UI components library code, currently living under `apps/www/registry/elevenlabs-ui`), and a placeholder for `packages/orpheus-core`. Update `pnpm-workspace.yaml` to include these packages.
- **Import Orpheus Codebase:** Bring Orpheus SDK’s code into the monorepo under `packages/orpheus-core` (or `libs/orpheus-core`). This can be done by adding Orpheus as a git subtree or submodule initially, or simply copying files. We will preserve history if possible (to maintain blame and reference to Orpheus commit history). Place Orpheus’s `src/`, `include/`, `adapters/`, etc., inside this new directory. Initially, this is just a code import; Orpheus will not be built or integrated yet in this phase. It remains dormant, except we might run its tests separately to ensure code came over intact.
- **Namespacing & Rebranding:** Decide on a namespace for the combined project’s packages: likely `@orpheus/` as indicated. Rename the UI package from whatever it might be (if it was going to be published as `@elevenlabs/ui`, we will use `@orpheus/ui` or similar). Update its package.json name to “@orpheus/ui”. Likewise, for Orpheus core, if we package it for Node, call it `@orpheus/core`. Update any internal references: e.g., in docs or CLI JSON, referencing old names. However, at this stage, we might leave user-facing names (like in docs site text) for later phases to minimize changes at once.
- **Tooling Baseline:** Add any necessary root-level config to handle new structure. For example, ensure ESLint and Prettier ignore or handle C++ code (we might exclude `packages/orpheus-core` from ESLint since it’s not JS). Conversely, bring Orpheus’s clang-format file into `packages/orpheus-core` to keep C++ styled, and possibly set up a pre-commit hook for it. No merging of build systems yet – Orpheus code is just there.
- **CI Pipeline Setup:** Merge the CI workflows conceptually. For now, keep Orpheus’s original GitHub Actions (`ci.yml`) separate but ensure it runs in the new repo (adjust paths in triggers from `src/**` to `packages/orpheus-core/src/**`, etc.). Also run Shmui’s (if any tests or build). Essentially, Phase 0 CI will run two independent workflows: one to build/test Orpheus C++ on multiple platforms (no changes yet) and one to build the UI (the docs site build or lint). This double CI ensures nothing is broken by code reorganization.

*Validation:* After Phase 0, **the repository builds and tests should all pass as they did before**, independently. Orpheus’s C++ tests run and pass in CI (ensuring the code still works in new location). The Shmui docs site should run as before (no changes in functionality). No integration between them yet, but we have a single repo containing both. If any issues (like path adjustments, or needing to set environment for Orpheus build), address them. This phase is reversible – if issues arise, we can revert the import and try again; we haven’t changed core code.


### **Phase 1: Tooling Normalization and Basic Integration**

*Goal:* Make Orpheus build available as a consumable module and get minimal integration between UI and core.

- **Build Orpheus as a Library Artifact:** Decide integration mode: for a browser-based app, compile Orpheus to WebAssembly; for an Electron/Node app, compile as native addon. Perhaps we target both eventually. Initially, we can target Node addon to ease development (tests can run in Node). Use **CMake’s add_library** to build a static or shared library. Then introduce a **Node.js build step**: e.g., use `node-addon-api` or NAN to create a Node module that exports Orpheus functionality (maybe the SessionGraph class and some basic functions). Alternatively, use Emscripten to build a WASM and a JS wrapper. This step is technical: we write a C++ wrapper file (e.g., `binding.cpp`) that converts between JS types and Orpheus C++ calls. We might expose a simplified API initially (e.g., `loadSession(jsonString) -> SessionHandle`, `getTracks(SessionHandle) -> array`, just enough to test things). Integrate this build into the monorepo: e.g., create a `packages/orpheus-node` that has a package.json with `"gypfile": true` or uses CMake.js to build the addon. Configure Turborepo to run this build in pipeline (so `pnpm build` triggers building the addon).
- **Basic Bridging of UI and Core:** Now, add a minimal connection: for example, implement a feature where the UI can call Orpheus to do something simple. A good candidate: **validate session JSON**. Orpheus can load a session JSON and immediately save it (round-trip) to ensure it’s valid. We could add a UI button in the docs site (for internal testing) to load a hardcoded JSON through Orpheus and print result. Or integrate Orpheus with the VoicePicker or a new small component. Possibly create an “SessionInfo” component that uses Orpheus: feed it a session file (maybe a static one from Orpheus’s fixtures), and display how many tracks/clips via Orpheus library call. This will test end-to-end: React -> Node addon -> Orpheus core -> result -> React. This is mostly an internal test component initially, not exposed to users, but it proves the pipeline.
- **Tooling Alignment:** Unify testing frameworks where possible. For instance, we might create a way to run Orpheus’s tests via CTest in the CI along with JS tests. Or conversely, if planning to use JS to test integration, write a small Jest/Vitest test that requires the Orpheus addon and calls a function (like ensure a known JSON yields expected output). This begins to unify quality checks. Also ensure code linting covers appropriate files (maybe introduce a clang-tidy runner in CI for Orpheus code, or incorporate it in a pre-commit). Align commit messages (if Orpheus devs weren’t using Conventional Commits, they should now as commitlint is enforced).
- **Storybook/Docs integration (early):** If we will incorporate Storybook for UI, this is a time to set it up (or in Phase 2). But at least, ensure the existing docs site builds with the new monorepo structure. Possibly mount new Orpheus-related docs under a section.
- **Namespace and Deprecate ElevenLabs references:** By now, rename references in docs and code from “ElevenLabs” to “Orpheus” where appropriate. For example, the `ui.elevenlabs.io` links in docs – if we plan to host our own registry JSON, update those URLs (or plan to). If our fork will use the same CLI approach, perhaps we host `ui.orpheus.dev` or package an offline registry. In any case, mark that we’re moving away from ElevenLabs cloud dependency. Possibly still use the ElevenLabs voice API for now, but we can note to change that later if Orpheus can do TTS.

*Validation:* After Phase 1, we should be able to **call Orpheus code from the UI environment**. For example, run a Node script or a Next.js API route that invokes Orpheus to do something and returns output. Validate Orpheus’s functions work via the binding (correct data conversion, no crashes or memory leaks as far as basic tests show). All existing features of UI still work (we have not replaced any internals yet, just added new capability). The CI now should include building the Orpheus addon and possibly running a basic integration test. This phase is partially reversible: we could disable the addon and still have a functioning UI if needed.


### **Phase 2: Feature Integration & Bridging**

*Goal:* Start using Orpheus’s capabilities in the UI features and migrate logic from any ElevenLabs/cloud reliance to local where appropriate.

- **Voice Integration (optional scenario):** If Orpheus in future can provide voice list or TTS, integrate here. (Currently Orpheus doesn’t do TTS, so we may skip this, but if Orpheus had a role in synthesizing audio or listing voices from a local model, this is where we’d plug it in). Possibly skip; focus on things Orpheus actually does, like session management.
- **Session/Project Management UI:** Design how the front-end will present Orpheus’s core functionality. Perhaps create a new section in the app for “Session Management” where a user can create/edit a session (arrangement of tracks/clips). This would use Orpheus core exclusively: e.g., Orpheus could hold a SessionGraph in memory, and UI calls methods to add tracks, etc., reflecting changes in UI. Because building a full DAW UI is huge, we might limit scope to some key interactions that showcase Orpheus: e.g., an “import session JSON” button (to load a project), a list of tracks display, maybe a “simulate transport” button where Orpheus can tick through beats and the UI shows a progress bar (or orb pulses to tempo). Or a “render click track” function – user enters BPM and bars, UI calls Orpheus to generate click.wav. This ties Orpheus into the UI flow meaningfully. Implementation: Extend the Node/WASM binding to expose needed Orpheus APIs (createSession, addTrack, etc., or a generic “invoke command” model). On UI side, build React state around Orpheus’s state: possibly treat Orpheus Session as authoritative and mirror in React, or always query Orpheus when needed. Because Orpheus is in memory, we might need to store a handle (pointer id). We’ll likely maintain a singleton Orpheus instance for now (like one session at a time loaded).
- **Audio Playback Integration:** Currently, audio playback is done via AudioPlayer with HTML audio. If Orpheus core eventually can output audio streams (for example, if in future it generates a click track in memory or streams timeline audio), we should integrate that. For now, Orpheus’s “render click track” writes to a file[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/README.md#L112-L120). We can have Orpheus produce a WAV file (in a known temp directory), then use the AudioPlayer to play that file (using a file:// URI in Electron, or by exposing an endpoint in Next that serves the file). Alternatively, compile Orpheus with libsoundio or something to play directly – but that complicates cross-platform. Simpler: use Orpheus to create content, use UI to play it. That aligns with contracts (AudioPlayer still uses browser audio). So define a path: e.g., Orpheus can return a path or bytes of WAV, UI gets it and feeds to AudioPlayer (maybe using a Blob URL if in browser). This step will prove we can pipe data from core to UI seamlessly.
- **Replace/augment existing features:** If any existing UI component can utilize Orpheus for better fidelity, do it. For example, the Orb currently just reacts to some dummy volume function (in docs they pass static or random values). We could connect Orb’s `getInputVolume` to Orpheus’s transport simulation output (for instance, if Orpheus had a concept of output volume or metronome tick, etc., feed that to Orb). Or if Orpheus could provide a waveform for a clip, we could create a new visual component for it. Another idea: use Orpheus’s session data to populate a **timeline component** (maybe using a Canvas to draw blocks for clips). We might not fully implement that, but even a rudimentary text-based display showing clips via Orpheus data would demonstrate integration.
- **Coexistence and Backwards Compat:** At this phase, the app might still have the option to use ElevenLabs for voices or not use Orpheus at all (i.e., run as it originally did). Ensure we can turn on/off Orpheus features easily (perhaps via a config toggle). This is fallback in case something goes wrong. But ideally, the new features don’t break old ones.
- **Documentation & Demo Updates:** Expand the docs site to include a section for “Orpheus Integration” or similar, demonstrating how to use the new combined features. For instance, provide MDX guides: “Using Orpheus for local sessions”, with code samples. If we plan to expose these to users, also update the README etc., to reflect that this UI library now has an audio engine inside.

*Validation:* By end of Phase 2, we should have **user-visible functionality powered by Orpheus**. Test that: load a session JSON via UI, confirm Orpheus parsed it and UI displays content correctly; run a click track render, confirm a WAV is produced and plays; try any Orpheus-driven UI controls (like adding a track) and verify Orpheus’s data (maybe by saving session). Also ensure existing UI functions (voice picker, audio player for normal audio) still work unaffected (regression test voice selection, search, etc.). Validate performance – e.g., when calling Orpheus from UI, is it responsive (maybe measure that a click track generation at 2 bars, 100 BPM returns quickly)? If we see any slowdown or heavy blocking, consider moving Orpheus calls to a Web Worker or separate thread (that might be a Phase 3 optimization). At this stage, the integrated system should deliver clear value beyond what either alone did.


### **Phase 3: Tooling Unification & Optimization**

*Goal:* Finalize CI, testing, release process, and address any tech debt from integration.

- **Unified CI Pipeline:** Merge the previously parallel CI. Now, the build/test is integrated: a single pipeline that installs PNPM, bootstraps workspace, builds the UI and Orpheus together. Include matrix only where needed (e.g., maybe run Node tests on all OS if using native addon, or build WASM once). Also run UI tests on one OS. Ensure artifacts or test coverage from both sides are collected. The CI should also run any lint (both ESLint and clang-tidy) and formatting checks. Essentially, institutionalize the quality checks uniformly.
- **Automated Tests & Coverage:** Augment test coverage for integration points: e.g., add a Vitest/Jest test suite for Orpheus binding (calling core methods and asserting results, which complements Orpheus’s C++ tests by testing the binding layer). Add integration tests for UI flows (maybe using Playwright or Cypress to simulate user clicking a button that triggers Orpheus). Establish code coverage tools if possible across both C++ and TS (though combining coverage might be tricky, but at least separate).
- **Performance Tuning:** Evaluate if any part is slow or heavy. For instance, the size of the WASM, or the latency calling into Orpheus. If WASM is too large, consider using dynamic loading or trimming unused parts (e.g., if we don’t use JUCE part, ensure it’s not compiled in). If using a Node addon, ensure we produce release builds for production (turn off sanitizers unless needed). Possibly implement multi-threading: Orpheus could run a background thread for heavy tasks; Node can offload using worker threads if needed to not block the event loop. Also consider memory constraints: Orpheus in Node could allocate, so ensure proper freeing (maybe run valgrind or address sanitizer on the integration).
- **Finalize Storybook/Docs for UI:** If not already done, integrate **Storybook** to visualize all components (especially new integrated ones). This helps catch issues and will serve in future for UI regression testing. Add stories that maybe simulate Orpheus data (could use a mock Orpheus if needed for Storybook environment, or run with WASM in browser storybook). This is also a place to add **visual regression tests** (with a tool like Chromatic or Loki) to detect unintended UI changes going forward.
- **Deprecate Legacy Paths:** If any original aspects are now redundant, phase them out. For example, if Orpheus replaced some ElevenLabs functionality, clearly deprecate the old method. Or if we had a stub Orb volume function and now always use real volume, remove the stub. Clean up any “toggle” code if we decide to fully switch. Document any breaking changes (this will go into breaking_changes.md).
- **Release Packaging:** Set up how releases will be cut. Possibly we publish the UI library (now enhanced) to npm under `@orpheus/ui`. Also, if providing Orpheus as an npm package (for others to use the core), publish `@orpheus/core` or similar with the native module or WASM. Establish versioning scheme (maybe a single version across packages for simplicity, using changesets to bump versions when changes occur). Also, incorporate any release automation (e.g., GitHub Action to publish on tag). In this unified approach, maybe we do one release that contains both UI and core – or separate if they can be used independently (not likely intended though, because core as a Node module might be useful to others too).
- **Documentation & Examples:** Complete user documentation: how to install and use the new integrated library. Provide examples: e.g., a minimal Next.js project that uses @orpheus/ui to load a session and play a click track. Or a tutorial in docs for building a simple app with these pieces. Include notes on how to get an ElevenLabs API key if voice selection still uses it (or how to configure TTS if in future Orpheus had offline TTS). Essentially, polish the explainer to reflect the new unified product.

*Validation:* Phase 3 is about polish and final checks. Validation here includes: all tests (unit, integration, UI) passing consistently, code quality checks green, and manual testing of the entire app flows on all target platforms (open the app on Windows, Mac, Linux if applicable, ensure Orpheus part works everywhere). Also test distribution artifacts: if someone uses our published package, does it work (especially the native module – ensure prebuilds or build instructions are clear)? This phase ensures we’re ready for actual users to use the integrated system with confidence.


### **Phase 4: Gradual Rollout and Reversibility**

*Goal:* Deploy or release in a controlled way, with ability to roll back if issues.

 

This isn’t exactly coding but strategy: initially, maybe release a beta version of `@orpheus/ui` and have internal projects or selected users try it. Keep the old Orpheus (as standalone) around for a bit – e.g., do not immediately deprecate the standalone until we see the integrated works well. We can maintain Orpheus’s old repo read-only with a note pointing to new one, for archival. If a severe issue is found with integrated approach, worst-case we still have Orpheus’s code untouched in an older release that could be used independently. That said, given internal use, likely we’ll transition fully.

 

We ensure each phase above is reversible in that:

- Phase 1: If binding fails, we still have separate systems (no user impact).
- Phase 2: If certain feature integration fails (say Orpheus doesn’t play nice), we can feature-flag it off and fall back to old way temporarily (e.g., use cloud service instead).
- Phase 3: If CI reveals issues, we fix before release.

By final release, we might not need reversal, but we plan for hotfixes or disabling features if something goes wrong in production.

 

**Timeline & Checkpoints:**

- Phase 0: ~1 week (just moving code and ensuring builds pass).
- Phase 1: ~2-3 weeks (implementing Node addon or WASM, basic integration test).
- Phase 2: ~4-6 weeks (depending on how deep we integrate Orpheus features into UI; includes building new UI for session management perhaps).
- Phase 3: ~2 weeks (CI/test polish, docs).

In total, ~2-3 months of work for a robust integration, but we should see incremental benefits along the way (especially after Phase 2, we can demo new capabilities).

 

This migration plan emphasizes **incremental progress, continuous validation, and flexibility to adjust**. Next, we outline the anticipated breaking changes and how to manage them, as well as any codemods to automate code adjustments needed during the migration.




## Breaking Changes (`breaking_changes.md`)

Merging Orpheus SDK and Shmui UI will inevitably introduce some breaking changes. It’s important to document them and plan mitigation or communication to users. Below is a list of expected breaking changes, categorized by area, along with notes on impact:

 

**1. Package and Import Names:**

- *Change:* The NPM package name for the UI library will change from its ElevenLabs identity to the new `@orpheus/ui` scope. Similarly, any import paths in code that referred to `'elevenlabs-ui'` or similar will need to update to `'@orpheus/ui'`.
- *Impact:* External projects using the old package (if any, given it was new) will have to switch to the new package name. Internal code or documentation referencing old names will break until updated.
- *Mitigation:* We will publish at least one version under the old name that simply re-exports from the new name (if possible) or clearly throws an error pointing to the new package. Provide a migration guide snippet: e.g., “Replace `import { VoicePicker } from "elevenlabs-ui"` with `import { VoicePicker } from "@orpheus/ui"`.”

**2. TypeScript API Adjustments:**

- *Change:* Props or components may have slight modifications. For instance, we might rename some components for clarity or unify naming conventions (e.g., ensure everything uses Orpheus naming, like `OrpheusAudioPlayer` vs `AudioPlayer` – though likely we keep names the same for now). The introduction of Orpheus might also add new optional props or change behavior (e.g., AudioPlayer might now require an Orpheus session in some contexts).
- *Impact:* For UI library consumers, if we remove or rename props, their code may not compile or may behave differently. Example: if `VoicePicker` now by default tries to use Orpheus for voices, but previously it expected an array of voices passed in, that might be a change (though probably we keep it controlled).
- *Mitigation:* Where possible, maintain backward-compatible function signatures and component props. If we add Orpheus integration behind the scenes, ensure it doesn’t change how a consumer uses the component (for example, VoicePicker can still accept `voices` prop as before, even if internally it could fetch via Orpheus if not provided). Document any intentional changes in props in a changelog. Use TypeScript’s deprecation JSDoc on anything we plan to remove later but keep now.

**3. Removal of ElevenLabs CLI Integration:**

- *Change:* The current usage with `npx @elevenlabs/agents-cli add component` may no longer be applicable if we host our own registry or package differently. If we discontinue the ElevenLabs CLI JSON registry approach, that is a big change in how developers consume components.
- *Impact:* Developers who were using the CLI to pull updates might not get updates the same way. They would instead install the NPM package or copy code manually.
- *Mitigation:* Provide our own equivalent CLI or instructions for manual installation. Possibly adopt the official shadcn UI CLI pointing to our registry JSON (maybe we host `r/all.json` on our GitHub pages or package it). Or, at least, clearly state the new method in documentation. This is a breaking change in developer workflow rather than runtime – communicated via docs and release notes.

**4. Orpheus Core Integration Changes:**

- *Change:* Orpheus’s functionality might be exposed differently. If a developer used Orpheus SDK standalone, the new integrated approach might not support the same standalone usage (e.g., building Orpheus as a separate lib to use in a C++ app). Particularly, if Orpheus was open-sourced for others to use as a C++ SDK, now we’re folding it into a different context. We might drop support for building Orpheus outside of our monorepo (maybe not immediately, but eventually).
- *Impact:* Any external Orpheus SDK users (likely none yet, if internal). Internally, build processes for Orpheus (like packaging a Reaper plugin) might change because the code moved. For example, `CMakeLists.txt` location changed, or one has to run through PNPM scripts to build.
- *Mitigation:* Continue to allow building Orpheus library via its own CMake for now (we can keep a top-level CMakeLists in packages/orpheus-core that one can run independently). Document that Orpheus is now meant to be used via the integrated Node/WASM module for most use-cases. If we intend to still ship the Reaper plugin, ensure instructions are updated (they might now need to run a specific CMake command or a PNPM command to build it).

**5. Deprecation of Legacy UI or Duplicate Features:**

- *Change:* If any features overlapped between Orpheus and Shmui, one might be removed. For example, Shmui’s audio playback vs Orpheus’s audio playback: we decided to use Shmui’s way. If Orpheus had some GUI or CLI not needed anymore, we might deprecate them. Specifically, Orpheus’s JUCE demo host might be discontinued in favor of the new integrated UI.
- *Impact:* Orpheus contributors or users might lose that demo app (which might be fine). Also, Orpheus’s Reaper adapter might see reduced focus or separate life. If we decide not to maintain the Reaper extension (since we have our own UI now), that’s a break for any REAPER user of Orpheus.
- *Mitigation:* Decide on strategy: possibly keep Reaper extension building for now (no harm to keep it if it’s working, and mark it as an alternate integration). Or explicitly deprecate it and communicate to any stakeholder (if internal, let product know). If needed, spin it off as a separate minimal repo referencing Orpheus’s last standalone version, so it doesn’t block our integration.

**6. Behavior Changes in Components:**

- *Change:* There may be subtle behavior changes. Example: AudioPlayer might have slightly different buffering behavior if integrated with Orpheus vs pure browser. Or VoicePicker might default to showing no voices until Orpheus provides them. Or Orb might respond to a new global state differently.
- *Impact:* These might not “break” compilation, but could change the user experience or expectations. For instance, if previously voice preview started instantly via URL and now if we had Orpheus do something before playback, it might add delay.
- *Mitigation:* Test and document differences. If any such changes are intentional improvements (e.g., now voice preview goes through local engine for maybe custom processing), highlight that in release notes (“Voice preview audio is now streamed through Orpheus engine, which might cause a slight initial delay but allows future X feature”). Where not intended, try to minimize changes to avoid surprises.

**7. Consolidation of Licensing/Attribution:**

- *Change:* We will consolidate the code under Orpheus’s MIT license and attribution. There should be no license break because both sides are MIT, but we need to ensure we carry over copyright notices from ElevenLabs UI components to respect open-source licenses.
- *Impact:* Possibly no runtime impact, but a packaging impact: e.g., the distributed package will have multiple license notices (one for Orpheus, one acknowledging ElevenLabs original work). If we fail to include those, we risk legal issues.
- *Mitigation:* Include a `LICENSE.md` that is a merged text or includes sub-sections for each project’s original license. Mention ElevenLabs UI origin in README. Not a breaking change for users, but something to handle carefully.

**8. Configuration Changes:**

- *Change:* Projects integrating might need new config. For example, if the Orpheus WASM needs a particular MIME type served or a certain fetch policy, integrators must handle that (our docs site will, but external might not know). Or if using the Node addon, they might need to ensure it’s built for their OS (maybe adding an install script or bundling prebuilt binaries).
- *Impact:* Users of the new package might face setup steps they didn’t before (like needing to run `pnpm install` which triggers a compile for Orpheus if no prebuild, requiring them to have a C++ compiler installed – that’s a breaking requirement compared to a pure JS lib).
- *Mitigation:* Provide prebuilt binaries for common OS via npm (like some packages do with node-pre-gyp), or if using WASM, bundle it so it just works in the browser. Document any additional requirements (e.g., “If using Node addon and no prebuild for your platform, you’ll need build tools – see docs”). Possibly offer an alternative (like a pure JS fallback for certain functionality if build isn’t available).

**Plan for Communication:**
All breaking changes will be compiled into a **migration guide** for users (which this document forms the basis of). We’ll version-bump to a new major version (`1.x` to `2.0.0`, for example) when releasing the integrated package, signifying these changes. Each breaking change entry above will be listed in the changelog with guidance: what changed, what you need to do. For internal stakeholders (e.g., teams using Orpheus SDK internally), we’ll hold walkthrough sessions and provide support for updating their usage (like updating any references to Orpheus’s build or adjusting to the new UI).

 

Since the user base may not be large at this point (the projects are new), the breakage impact is manageable, but we will still treat it with caution and thoroughness.




## Codemods (`codemods/`)

To ease the migration of code and help users update to the new integrated system, we will develop a set of **codemods** (automated code transformation scripts). These scripts will handle repetitive or mechanical changes in the codebase or user projects. Below is an outline of planned codemods and their functionality:

 

**1. Rename Imports Codemod** (`codemods/renameImports.ts`):

- **Purpose:** Update import statements from old package names/paths to new ones.
- **What it does:** Scans project files (especially `.tsx?` and `.jsx?`) for any imports of `"elevenlabs-ui"` or relative paths that have changed (like if some components moved from one directory to another in our restructure). Rewrites them to the new scope, e.g.:
```



```

Also handles cases like `import something from "elevenlabs-ui/whatever"` if that existed, mapping to new structure (though likely not needed since it was all in one package).
- **Usage:** We’ll run this codemod on our own codebase when merging, and provide it to external users (if any) to help them transition. Possibly integrated with `jscodeshift` for distribution.

**2. Prop Changes Codemod** (`codemods/updateProps.ts`):

- **Purpose:** Adjust component prop names or usage patterns that changed due to integration.
- **Examples:** If we decided to rename a prop like `onValueChange` to `onChange` for consistency across components, or if a prop became optional/removed. The codemod can find `<VoicePicker ... onValueChange={...} />` patterns and rename accordingly. Another example: If `AudioPlayerProvider` now requires a prop (say, a Session ID to know which Orpheus session to control), we could insert something like `<AudioPlayerProvider session={currentSession}>` if we can infer `currentSession` from context. Though insertion logic is hard, we might at least warn.
- **What it does:** Uses AST transforms to find JSX elements by name and adjust their props. We’ll write specific transforms for each known breaking prop change. If some changes can’t be automated (too context-specific), we’ll either not attempt or log a TODO for manual fix.
- **Usage:** Mainly for internal refactoring. We’ll run it on our code examples and possibly include it in the library’s upgrade script for users.

**3. Remove Deprecated Code Codemod** (`codemods/removeDeprecated.ts`):

- **Purpose:** Clean up any deprecated patterns.
- **Examples:** If previously the recommended usage was to call a function that is now removed, or use a context that’s gone. One scenario: if earlier usage needed `AudioPlayerProvider` around your app and now we changed it to be optional or different, we might help remove redundant providers. Or removing imports of ElevenLabs SDK if not needed (like if user imported `@elevenlabs/elevenlabs-js` for voices but now our component handles it). This is tricky because if user code was using that SDK for other things, we can’t blindly remove it. So likely minimal here.
- **What it does:** Possibly search and remove specific import lines or code blocks that match known no-ops now. For instance, if a user had some setup code for the old UI (maybe initializing something that no longer exists), we can drop it. This one might just output warnings for manual removal unless very straightforward.

**4. Reaper Adapter Stub Codemod** (internal, if needed):

- If we separate out the Reaper plugin, and the new Orpheus no longer includes it, we might provide a codemod or script to extract those files into a separate repo. But this is more of a one-time internal operation than a codemod for general use.

**5. CLI to Import Migration**:

- Not exactly a codemod on code, but we could script converting a project that relied on the CLI JSON to using the npm package. For example, if a user had integrated by copying components, maybe provide a script to install our package and remove the copied files. This is complex and maybe unnecessary.

Each codemod script will be thoroughly tested on example code (we’ll simulate an older version usage and run the codemod to see that it correctly transforms to new usage). We will package them possibly as part of the repo (under a `codemods` folder, as requested, likely containing JS/TS scripts that can be run with jscodeshift or similar). Documentation will instruct users how to run them (e.g., “npx jscodeshift -t codemods/renameImports.ts src/”).

 

For our internal codebase, we might integrate these codemods into the migration process (like run them automatically during commit to update references in docs, etc.).

 

**Important:** Codemods will be versioned and tied to specific upgrade steps. We’ll label them with the version (e.g., `v1_to_v2_renameImports.ts`) to avoid confusion.

 

By providing codemods, we aim to make the upgrade path less painful, saving time and reducing human error in performing repetitive changes.




## CI Pipeline (`ci_pipeline.yml`)

We will implement a unified **Continuous Integration (CI) pipeline** that automates building, testing, and validating the combined repository across different environments. The pipeline will be defined in a YAML (for GitHub Actions, as we’ve been using) and will incorporate steps for both the C++ core and the JS/TS UI. Below is an outline of the CI pipeline (in a GitHub Actions style pseudocode for clarity):


```

























































```

**Explanation of Pipeline Steps:**

- **Matrix Build**: We choose to compile/test on all three OS for thoroughness. The Orpheus core will be built on all, ensuring no platform-specific compile errors and that our Node addon or WASM works everywhere. We limit some tasks to one OS to save time (like running C++ tests fully on Linux only, assuming portability, or doing bundling checks on one).
- **Dependency Installation**: We cache PNPM installs to speed up repeated CI runs.
- **Build Phase**: `pnpm run build` will trigger our Turborepo. This will likely:
- - Compile Orpheus core: e.g., run CMake and make via a custom script (maybe `prebuild` in `@orpheus/core` package that invokes cmake).
- Build UI package: possibly just ensure TypeScript compiles, or since our UI is source-distributed, there might not be a traditional bundle. But for the docs site (Next.js), we might build it to ensure no errors.
- If Orpheus is WASM, build will produce a `.wasm` and maybe copy it to the docs public folder for usage.
- **Lint/Type-Check**: Ensures coding standard and types. ESLint covers TS/JS, maybe we also run clang-format or clang-tidy for C++ here:
- - We could add a step `run: packages/orpheus-core/run-clang-tidy.py` or use clang-format check. Possibly incorporate that in `pnpm run lint` by using a package like `clang-format` if installed, to check no diffs.
- Type-check uses `tsc --noEmit` to ensure TS correctness.
- **Test Phase**:
- - `pnpm run test` will run JS/TS tests. This includes unit tests (Vitest) and possibly integration tests. We may have separate scripts for different test types (e.g., `test:unit`, `test:integration`, `test:e2e`). We can choose to run headless browser tests (with Playwright) on one OS (Ubuntu, with xvfb). If not needed, skip.
- Orpheus’s C++ tests are run separately. We choose to run them only on one OS for speed (the code is identical, and GoogleTest covers logic thoroughly; platform differences are minimal due to cross-platform design).
- If any tests are timing or environment sensitive (like Node addon tests might have to load the compiled binary, ensure path correct), we handle that in configuration (maybe `core:test` script knows where build put the .node file).
- **Bundle Size & Dependency Cycles**: These are additional quality gates:
- - Bundle size: We can use Next.js `next build` with `ANALYZE` flag to get stats, or use a tool to ensure the main UI bundle is within expected limits (especially if adding Orpheus WASM, ensure it’s not ridiculously large or at least track it). Possibly only in PR runs, not every push.
- Dependency cycle: Use a tool like **Madge** to check circular dependencies in the TS code, which helps maintain code health. Or for C++, ensure no circular library deps (though CMake would catch that).
- **Artifacts & Cache**: Not explicitly shown, but we may add steps to upload certain artifacts:
- - If a test fails, upload logs (like CTest output, or Vitest results).
- We might also artifact the build outputs (e.g., the Orpheus .node or .wasm) for use in further pipeline (like a release pipeline).
- Possibly run coverage and upload to codecov (if desired).
- **Release Workflow**: Outside this CI, we’ll have another workflow (maybe triggered on tag) to publish to NPM. That one will build and then run `pnpm publish -r` for all packages, etc., with proper auth. This might reuse some of CI tasks (or we integrate it here with a conditional).

**In Summary**, the `ci_pipeline.yml` ensures that:

- Code style is enforced (via lint).
- Types are correct (no type errors).
- Both Orpheus core and UI build properly on all supported platforms.
- Tests for both run and pass, ensuring no regression.
- Performance guardrails (bundle size, no circular deps) are monitored – preventing accidental bloat or structural issues from creeping in (for example, if someone imports the entire Orpheus WASM in every component by mistake, the bundle check would flag an unusual size).

This pipeline will run on PRs so we catch issues before merging, and on main pushes to double-ensure main stays healthy. Because it’s a bit heavy (matrix across 3 OS), we could allow some jobs to fail or run less frequently to optimize (for instance, maybe run macOS and windows on a daily schedule or on release, rather than every commit, if speed is an issue). But initially, thoroughness is key.

 

Finally, this integrated CI replaces the separate pipelines from before – it’s one source of truth for project health.




## Release Policy (`release_policy.md`)

To manage the unified project’s evolution, we will establish a clear **release policy** covering versioning, branch strategy, and distribution channels. Here are the key points of the release policy:

 

**1. Semantic Versioning**: We will adopt **Semantic Versioning (SemVer)** for releases of the combined project. Given this integration is a major change, our first integrated release will be **v1.0.0** (or v2.0.0 if we consider previous UIs as v1). After that:

- **Major versions (X.0.0)** for incompatible API changes (e.g., removing deprecated components, big changes in Orpheus’s API or UI components).
- **Minor versions (0.Y.0)** for adding new features in a backward-compatible way (e.g., new components, new props that don’t break existing usage, performance improvements, new Orpheus functionalities that don’t require user code changes).
- **Patch versions (0.0.Z)** for backward-compatible bug fixes (e.g., UI bug fixes, minor performance optimizations, documentation updates).

This way, users can pin to a major version and trust no breaking changes will occur within that major.

 

**2. Monorepo Versioning Approach**: All packages in the monorepo (`@orpheus/ui`, `@orpheus/core`, etc.) will be versioned and released in lockstep. This simplifies management – since the UI and core are tightly coupled, we release them together with the same version number. (Alternatively, we could version them separately if someone might use core independently, but initially, lockstep avoids dependency confusion). We will use a tool (like Changesets or Lerna) to manage multi-package version bumps and changelogs.

 

**3. Pre-releases**: For significant upcoming changes, we will utilize pre-release tags (e.g., `1.1.0-beta.1`, `2.0.0-alpha.0`). This allows early testing by internal teams or power users without affecting stable channel. Pre-releases will be published to NPM with tags like `next` or `beta` and documented accordingly.

 

**4. Release Cadence**:

- During initial development (post integration), we might do frequent minor releases (for bug fixes and small features) – maybe weekly or bi-weekly, as features stabilize.
- Once matured, aim for a regular release cadence (for example, monthly minor releases, patch releases as needed in between for urgent fixes).
- We will avoid long periods of unreleased changes on main; instead, use feature flags or branches if something isn’t ready. This encourages incremental improvement and not deviating from main too much.

**5. Branching Strategy**:

- We will use the `main` branch as the integration branch for all new development (following a trunk-based approach with short-lived feature branches that merge into main via PRs after CI passes). `main` should always be in a releasable state (thanks to CI and review).
- For release maintenance, we may create release branches if needed (e.g., `1.x` branch for patch fixes on current major if we start working on next major on main). But given we are controlling both UI and core, we prefer to push forward rather than maintain many parallel versions.
- Hotfixes: if a critical bug in a release needs a fix, we can branch off the tag, apply fix, and publish a patch, then merge back to main. But ideally, small patch releases just come off main if main hasn’t diverged much.

**6. Deprecation Policy**:

- If we plan to remove or change a feature in a breaking way, we will first mark it as **@deprecated** (in code comments/TS JSDoc) and announce in release notes at least **one minor release** prior to removal. For example, if we intend to remove a prop or component in 2.0, we’ll deprecate it in 1.x with warnings and then remove in 2.0.
- Where possible, keep deprecated APIs working (maybe as wrappers) until the next major. For instance, if Orpheus core API changed, keep an adapter for old calls for one major cycle.
- Communicate clearly: maintain a **Deprecation List** in the changelog so users know what to migrate.

**7. Automated Changelog**: Use commit messages (enforced via commitlint) to auto-generate changelog. Each release will have notes categorized by Added/Changed/Deprecated/Removed/Fixes, etc. We’ll manually augment with any important context (especially for major releases or large features).

 

**8. Distribution**:

- **NPM Registry**: Primary distribution for the front-end components and Node addon (if applicable). The `@orpheus/ui` and `@orpheus/core` will be published to npm. We will use scoped packages (so user needs to be aware to install via `@orpheus/*`).
- **Binary Distribution**: If using Node addon, consider using a prebuild approach (like releasing binaries for win/mac/linux via GitHub Releases or using `prebuild` NPM packages). We might integrate with GitHub Actions to attach built .node files for each OS and then use `node-pre-gyp` to download them on install. This ensures users don’t always need to compile from source.
- **GitHub Releases**: For each version, create a release on GitHub with release notes and possibly attach any relevant artifacts (like WASM file, or CLI tool if we package one).
- **Storybook/Docs Deployment**: Host the documentation (perhaps via GitHub Pages or Vercel) for each release or continuously updated. This way, users can always refer to docs that match the version they use. Possibly tag docs with version if needed.

**9. Quality Gates for Release**:

- All tests must pass and coverage should not drop significantly before releasing.
- Bundle size is monitored – if a proposed feature drastically increases size, weigh it and possibly bump major if necessary (e.g., adding a huge dependency might be considered a breaking change if it affects consumers significantly).
- No high-severity open issues – we’ll try to at least address or have workarounds documented for any known serious bugs before releasing a stable version.
- Beta test: internal testing of a release in our own products before declaring it stable. For example, after merging integration, use the library in an internal project for a week to catch issues, then release to external.

**10. Backward Compatibility and Support**:

- Given this is early in the project’s life, we likely won’t support many old versions. We encourage upgrading to the latest minor. We might back-port critical fixes to the last minor if needed, but we won’t maintain a long LTS yet. If the user base grows and demands stability, we might designate an LTS branch in future.
- We’ll ensure that at least migrating from the immediate previous major is documented and supported via codemods. But we won’t, for example, support upgrading directly from a 0.x proof-of-concept to 2.0 – too much changed; we’ll focus on 1.x to 2.x transitions, etc.

**11. Communication**:

- For each release, update the README with any new information, bump version references in docs.
- Possibly maintain a “roadmap” in the repo (like Orpheus had ROADMAP.md[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/README.md#L148-L156)) so users know what’s coming (which helps them prepare for potential changes).
- If the project is open-source externally, engage via GitHub Issues/Discussions to gather feedback, and incorporate that into the release planning.

By following this release policy, we aim to deliver improvements quickly but safely, keep users informed, and maintain high trust in the quality and predictability of the Orpheus+Shmui unified library.




## Validation Checklist (`validation_checklist.md`)

To ensure a smooth migration and maintain quality at each phase, we will use a **validation checklist** for key milestones and releases. This checklist can also serve as acceptance criteria for each phase of the integration plan. The list below is organized by phase and general categories of validation:

 

**Phase 0 – Repository Setup Validation:**

- **Code Present and Accessible:** Verify that Orpheus core code is imported into the monorepo (all expected files in `packages/orpheus-core`), and Shmui UI code is in its package. Confirm we can navigate and open files in the new structure (IDs, paths updated accordingly in build configs).
- **Independent Builds:** Orpheus’s standalone build (CMake) runs without errors in the new location. Shmui’s existing build (Next app) runs without errors.
- **Tests Passing:** Run Orpheus C++ tests – all pass as in original repo[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L36-L44). Run any existing UI tests (if none, at least ensure the docs site runs without runtime errors).
- **CI Green on Phase 0 branch:** Ensure the interim CI (with possibly separate workflows) is passing on all targeted platforms.
- **No Broken Links/Paths:** Search the codebase for any references to old repo paths (like `backup/non_orpheus...` or hard-coded file paths) – they should either be updated or removed per audit[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/AUDIT.md#L16-L25). Ensure README and docs in new repo reflect the combined structure (at least a note that integration is in progress).

**Phase 1 – Basic Integration Validation:**

- **Orpheus Node/WASM Binding Compiles:** The new binding code compiles on all target platforms. On each OS, loading the module (require/import in Node or instantiating WASM in browser) does not throw an error (dependency resolution is correct).
- **Basic Orpheus Call Works:** Successfully call a simple Orpheus function through the binding and get expected result. For example, call a binding for `AbiVersion.get()` and ensure it returns the expected version number or structure[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L9-L17). Or load a known session JSON via binding and verify output JSON matches (round-trip)[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/ARCHITECTURE.md#L42-L50).
- **Memory and Stability:** If using Node addon, run it through AddressSanitizer or Valgrind to ensure no obvious memory leaks or crashes on simple usage. If using WASM, ensure that repeated calls don’t grow memory indefinitely (e.g., proper freeing in Orpheus code is in place).
- **Integration Test (manual):** Use the docs site or a small test harness to invoke Orpheus from UI context. For example, have a button in the docs that calls `renderClickTrack` and returns, verifying through logs or UI element that it succeeded.
- **Lint/Format Adherence:** Check that any newly written integration code (binding code, etc.) adheres to coding standards (C++ formatted with clang-format, TS with Prettier).
- **Phase 1 Rollback Plan:** Confirm that if binding is disabled (e.g., not loaded), the UI still runs normally. Essentially, the new code is additive and doesn’t break existing flows if toggled off.

**Phase 2 – Feature Integration Validation:**

- **End-to-End Scenario: Session Import:** In the integrated app, load a sample session (perhaps Orpheus’s `tools/fixtures/*.json`). Verify that the UI displays some representation of it (e.g., track count, etc.) and that no errors occur during import. The content in Orpheus core (SessionGraph) should match what UI shows (maybe via an API call back to get track names/counts).
- **Functional UI Elements:** For each new UI element or feature added:
- - *Transport Simulation:* If implemented, clicking play should cause beats to tick (e.g., console logs or UI update each beat). Confirm timing is roughly correct (simulate known BPM and count seconds).
- *Click Track Render:* Clicking “Render Click” yields an audio file or triggers audio playback in the UI. Check the audio: is it the correct BPM and bars as requested (listen manually if possible)? Check that file gets cleaned up if relevant.
- *Track/Clip Operations:* If UI allows adding/removing tracks, do it and ensure Orpheus core reflects it (maybe have a debug list of tracks from core to compare). Remove track and ensure no ghost state remains.
- **Audio Sync & Orb:** If Orb is hooked to audio or agent state from Orpheus, check it visually. E.g., if Orb is supposed to pulse on beat, see that it does when transport simulation runs. If agent state changes (maybe set to “thinking” when Orpheus is busy), confirm orb color changes accordingly[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/orb.mdx#L96-L104).
- **Legacy Feature Regression:** Re-test original features:
- - VoicePicker: can still select a voice and play preview (with or without Orpheus involvement). The search still filters correctly, orb still appears for voices[GitHub](https://github.com/elevenlabs/ui/blob/4457c8523e660b46b9a2a2f5503b35c60f18d33e/apps/www/content/docs/components/voice-picker.mdx#L169-L178).
- AudioPlayer: still plays regular audio files (like a sample music file) with controls working (play/pause, seek, speed change).
- Orb (standalone demo in docs): still renders and responds to dummy volume input as before if not connected to Orpheus (unless replaced entirely).
- **Performance Check:** Using the integrated UI, measure some interactions:
- - Loading a session doesn’t freeze the UI for an unacceptable duration (if it’s a large session, maybe a slight pause, but within reason – ideally under a second for moderate sessions). If it’s slow, note to optimize or at least warn.
- UI animations remain smooth even when Orpheus tasks run (e.g., if Orpheus generating audio, the UI progress spinner still animates – if not, consider offloading).
- **Cross-Platform User Testing:** Run the integrated app on Windows, macOS, Linux (where applicable).
- - On Windows, ensure the Node addon loads (no missing DLL issues). Also, if path handling (file paths for audio) are correct (Windows backslashes handled etc.).
- On Mac/Linux, check any case-sensitive path issues, and that sandboxing (on Mac maybe gatekeeper? If using WASM, no issues).
- If using WASM in browser, test on Chrome, Firefox, Safari – ensure compatibility (no peculiar WASM instantiation errors, audio works across).
- **Phase 2 Rollback Consideration:** Ensure that if any new feature fails, it’s isolated enough not to crash the entire app. For example, if Orpheus throws an error on a certain input, the UI catches it and shows a message rather than blank screen. This is less about rollback and more about resilience – but important for partial rollback (we could hide a malfunctioning feature flag without breaking others).

**Phase 3 – Final Integration Validation:**

- **All CI Checks Passing:** Lint, tests (unit/integration/e2e), type-check, and build on all OS should be green. No test flakiness – run tests multiple times if needed to ensure stability.
- **100% Manual Exploratory Test:** Do a thorough manual test pass of the application (or library in context of a sample app):
- - Go through documentation examples, copy code, see if it works as advertised.
- Try edge cases: extremely high BPM or long bars for click track, weird session JSON (like empty session, or one with a lot of tracks) – does Orpheus handle gracefully (maybe it has limits)?
- If using voice API, try with no API key or an invalid key – does UI show error nicely? (We might not heavily address that now, but note it).
- Test error handling: force an Orpheus error (maybe feed malformed JSON), ensure UI surfaces the error in a user-friendly way, not just console.
- **Bundle Size & Loading Performance:** Build the production bundle and measure:
- - Main bundle JS size – is it within targets? If not, consider code splitting or documenting requirements.
- WASM size if applicable – ensure it’s loaded asynchronously to not block initial render.
- Lighthouse/audit: get an idea of app load performance. For an integrated app, initial load might be heavier, but still aim for reasonable.
- **Documentation Accuracy:**
- - Read through `explainer_eleven_ui.md` (updated) – does it reflect the actual implementation post-integration? If not, update text or examples.
- Follow the steps in `migration_plan.md` and `breaking_changes.md` from a user perspective – do they make sense? Perhaps test running codemods on a pretend old code sample to ensure they produce correct output.
- Check `validation_checklist.md` (this doc itself) to ensure every item can be checked off (meta, but ensure we didn’t skip something).
- **Release Dry Run:** Simulate publishing:
- - Run `pnpm pack` or similar to produce the package tgz files for `@orpheus/ui` and `@orpheus/core`. Inspect their contents: correct files included (dist, .node, .wasm, all necessary assets, license files). No extraneous large files (like no including test fixtures or huge source maps unnecessarily).
- Install these tgz in a fresh project (outside monorepo) to verify installation works. For core, test that post-install script picks correct binary or compiles properly on a fresh system.
- Use the package in that fresh project with a basic usage to ensure it runs outside our dev environment (catch any missing peer dep or runtime require issue).
- **Final Approval Checks:** Team leads or stakeholders do a final review of architecture, code, and tests. Everyone agrees that the integrated product meets requirements and does not regress critical capabilities of either original project.

**Post-Release Monitoring** (not a checklist item, but plan it):

- After release, closely monitor any issue trackers or user feedback channels for problems (crashes, major bugs) and be ready to do a patch release quickly if needed.

By systematically going through this checklist before declaring the migration complete and releasing, we ensure high confidence in the quality and reliability of the new unified system. Each phase’s checklist items help catch issues early and avoid compounding problems later.
