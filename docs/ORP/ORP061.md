# ORP061 Migration Plan: Consolidating Shmui UI into Orpheus SDK Monorepo

**Overview:** This plan outlines a phased approach to merge the **Shmui React UI** (a browser-first UI, forked from ElevenLabs’ component library) into the **Orpheus SDK** repository. The objective is to unify Shmui as the front-end UI and development environment within the Orpheus SDK monorepo, which houses the canonical C++ core and its drivers (WASM, Electron/Node, service endpoints). We break the migration into safe, incremental phases, each with a clear goal and validation criteria. The plan emphasizes preserving Shmui’s development velocity and UI conventions (Storybook, component patterns) while introducing a shared CI/release workflow (using GitHub Actions and changesets versioning). We also describe any expected breaking changes, codemods for automating renames, CI pipeline updates, and the final directory structure in the unified repo.

## **Phase 0: Preparatory Repository Setup**

*Goal:* **Establish a monorepo structure and import Shmui without altering functionality.** This phase creates the scaffolding for multiple packages in Orpheus SDK and brings in Shmui’s codebase as-is, ensuring both projects build and test in one repo.

- **Monorepo Initialization:** Adopt a multi-package structure in the Orpheus SDK repo. Create a top-level `packages/` directory and configure a workspace (using PNPM or Yarn Workspaces, aligning with Shmui’s tooling). For now, encapsulate Shmui under `packages/shmui/` (retaining “Shmui” as an internal codename). Orpheus’s existing C++ code can remain at the root initially (to avoid breaking its CMake build), but we prepare to treat it as a package soon. Add a root `package.json` and workspace config (`pnpm-workspace.yaml` or similar) including `packages/*` so the repository can manage multiple subprojects.
- **Import Shmui Codebase:** Bring in all code from the `chrislyons/shmui` repo into `packages/shmui/`. Preserve history if possible (e.g. via a subtree merge) to maintain commit blame. The Shmui package will include its React components, styles, and Storybook/docs application (if Shmui had an `apps/` directory for a docs site, include that or merge it appropriately, e.g. as `packages/shmui-docs` or within `shmui` package). **No functional changes** are made to Shmui’s code in this phase – this is a straight copy. Orpheus core code remains unchanged in its structure. Both codebases coexist but are not yet integrated.
- **Namespacing and Package Names:** Establish a naming convention for packages under the Orpheus SDK. Likely use the `@orpheus/` NPM scope. For example, Shmui’s UI library was originally tied to ElevenLabs; we will eventually publish it as `**@orpheus/ui**` (or `@orpheus/shmui`) under our scope. In this phase, update `packages/shmui/package.json` name to `@orpheus/shmui` (to maintain codename internally) or `@orpheus/ui`. Similarly, plan for Orpheus core to be packaged (if exposing to Node or WASM) as `@orpheus/core` or subdivided (e.g., `@orpheus/engine-wasm`, `@orpheus/engine-electron`). We won’t rename all references yet, but we note that the ElevenLabs-specific naming (like import paths or URLs) will change in later phases. For now, document intended new names.
- **Tooling Baseline Alignment:** Add any root configurations needed to support both C++ and TS/JS projects side by side. For instance:
    - Ensure the Orpheus C++ build (CMake) is not disrupted by the new structure. We might move Orpheus source into `packages/orpheus-core` later, but in Phase 0 we can keep it at root to avoid path breakage. We may still add a dummy `packages/orpheus-core/package.json` to include it in the workspace for dependency graph purposes (if needed, pointing to the root for now).
    - Bring Shmui’s linting/prettier config into the repo and ensure it doesn’t run on C++ files. Conversely, include Orpheus’s coding standards: e.g. add Orpheus’s `.clang-format` and `.clang-tidy` in the C++ directories. The two sets of linters should ignore each other’s domains (no ESLint on C++, no clang-format on JS).
    - Unify the **changesets** configuration to include the new package. If a `.changeset/config.json` exists, list `packages/shmui` and any future packages so version bumps can be managed across the monorepo.
- **CI Parallelization:** Update GitHub Actions workflows to accommodate the new repository layout. Initially, run **existing CI tasks separately** for each part:
    - Run Orpheus’s C++ build and tests as before (adjust paths if needed, e.g., if moved, or simply run CMake from root as earlier).
    - Run Shmui’s build/lint/tests: e.g., if Shmui has a Storybook or Next.js docs site, ensure it can build without errors in CI. Install Node dependencies in CI and run `npm test` or `npm run build` inside `packages/shmui`.
    - We may keep two workflows or combine into one matrix. In Phase 0, the simplest path is to add a Node job to Orpheus CI. For example, one job sets up Node & PNPM, installs deps, and builds/tests the Shmui package (while another job does the C++ build on Ubuntu, Windows, Mac). Ensure both pass. This **parallel CI** ensures that merely co-locating the code didn’t break anything.

*Validation:* After Phase 0, **both projects build and test successfully in the unified repo** with no regressions. Orpheus’s C++ tests all pass (verifying the code still works from its new location), and Shmui’s app/UI runs as before (verify the Storybook or docs site can start and the UI components appear correctly). All CI checks should be green on a Phase 0 integration branch. This phase introduces no user-facing changes – it’s fully reversible if issues arise (we could remove the imported code and return to separate repos). A checklist of Phase 0 validation tasks is provided in the “Validation Checklists” section.

## **Phase 1: Tooling Normalization and Basic Integration**

*Goal:* **Enable Orpheus’s core to be consumed by the Shmui UI, with minimal feature integration.** In this phase, we start building the Orpheus C++ core into a form usable by the frontend (as a Node addon or WebAssembly module) and prove a basic end-to-end call from the React UI to the core. We also align project tooling further now that both parts live together.

- **Build Orpheus as a Library Artifact:** Set up the build of Orpheus core in the monorepo’s JS toolchain. We decide on a primary integration path:
    - For browser-first use, compile Orpheus to **WebAssembly (WASM)**. This involves using Emscripten or a similar toolchain to compile the C++ into a `.wasm` binary with a JS wrapper.
    - For an Electron or Node context (e.g. running the docs app in Electron or tests in Node), compile Orpheus as a **native Node addon** (using `node-addon-api` or N-API).
    - We may eventually support both. In Phase 1, we can start with whichever is easier to get working quickly. For example, begin with a Node addon for ease of debugging in Node.js, then add WASM.
    - Create a new package, e.g. `packages/orpheus-binding` or split by target (`engine-wasm` and `engine-electron`). For example, `packages/engine-node/` could house a Node addon build using CMake or node-gyp, and `packages/engine-wasm/` uses Emscripten. Initially, implement one target and stub the other.
    - Write a minimal C++ **binding layer** (e.g. a `binding.cpp`) that exposes a simple Orpheus API to JS. For instance, expose a function `getVersion()` that returns Orpheus’s `AbiVersion`, or a function `validateSession(jsonString)` that uses Orpheus to parse and re-serialize a session JSON (round-trip). Keep the scope small just to validate that C++ can be invoked from JS successfully.
    - Integrate this build into the monorepo’s package scripts. For example, in the root `package.json` or turborepo config, make `pnpm build` also trigger building the Orpheus binding. This might involve calling CMake from the `engine-node` package (possibly using a tool like `cmake-js` or a custom script). Ensure the output (`.node` binary or `.wasm` file) lands in a place the UI can import (perhaps output to `packages/engine-node/build/orpheus.node` and configure package exports).
- **Basic UI-Core Bridging:** In Shmui’s React code, introduce a **small feature or test hook** that uses the new Orpheus binding:
    - For example, add a debug panel or a temporary button in the Storybook/docs site that, when clicked, calls an Orpheus function. This could load a static session JSON (maybe use one of Orpheus’s example session files) and then display a result (e.g., show the number of tracks, or simply log success).
    - Another simple integration: have the UI call a `getVersion()` from Orpheus and display the core version in the UI to confirm the pipeline works.
    - The purpose is to ensure that **React -> (JS binding) -> C++ core** call is wired up correctly. We are not yet replacing any real UI functionality, just adding a proof-of-concept connection.
    - Keep this behind a feature flag or only in a development context so it doesn’t affect normal usage if something goes wrong (e.g., only load the Orpheus module in development or when a certain query param is present).
- **Tooling Alignment:** Now that we have mixed technologies, unify testing and dev workflows where possible:
    - Ensure that running `pnpm install` or `pnpm build` sets up both the UI and the core builds. For example, configure **Turborepo** pipelines if using (Shmui originally used Turborepo+PNPM) so that dependencies are known: the `shmui` package might depend on the `engine-node` package (so that it builds first).
    - Unify linting tasks: incorporate C++ formatting checks into the CI (maybe add a job or extend `pnpm run lint` to run `clang-format --check` on C++ code). Likewise, ensure ESLint/Prettier still only target TS/JS files. Both codebases should pass lint/style checks.
    - If Shmui didn’t have automated tests, consider adding a minimal test now that actually calls the Orpheus binding (for example, a Node script that requires the module and calls a function). This can be a Jest/Vitest test verifying that no exceptions are thrown and outputs match expected values.
    - **Storybook and Docs:** Make sure the Storybook (or docs app) still works after reorganizing. If needed, adjust its webpack/vite config to handle `.node` or `.wasm` files (for WASM, you might need to configure loading binary files). Possibly stub the Orpheus calls in Storybook if running purely in browser without a built WASM yet. At least confirm the docs app runs and new Orpheus features are not breaking it.
- **ElevenLabs Dependency Audit:** Begin **removing or replacing ElevenLabs-specific references**. For instance, if Shmui’s docs site references ElevenLabs URLs or uses the ElevenLabs CLI JSON registry for components, decide how to handle this:
    - We might plan to host our own component registry JSON under Orpheus (e.g., `ui.orpheus.dev`), or simply continue without the CLI. In Phase 1, we can leave this mostly as-is but mark it for change. Possibly fork or replace any ElevenLabs SDK imports (like `@elevenlabs/elevenlabs-js` for voice APIs) with configurations to use environment keys (so it’s not hard-coded).
    - Update any obvious branding in the UI (if any) from “ElevenLabs” to “Orpheus” as long as it doesn’t break functionality. Keep user-facing text changes minimal in Phase 1; focus on technical integration first. We will do a thorough rename in docs in a later phase.

*Validation:* After Phase 1, we should be able to **call Orpheus code from the UI environment**. Key checks:

- The Orpheus binding compiles and loads on all target platforms (e.g., the Node addon loads without error on Windows, Mac, Linux; the WASM instantiates in a browser if that path was taken).
- A simple Orpheus function call returns the expected result via the binding. For example, calling a version query returns the correct version number, or a session round-trip yields identical JSON.
- No regressions in Shmui: the UI should still run normally when the Orpheus module is not invoked. We should verify that if we disable the new integration (e.g., behind a flag), the app behaves exactly as before (this serves as a rollback plan).
- CI now includes the new build steps and an integration test. Ensure that the CI pipeline on Phase 1 branch runs the Orpheus build and the basic test on all OS (or at least one OS for the test) and passes.
- Developer workflow check: verify that running the dev server for the UI doesn’t become overly cumbersome. If building Orpheus is slow, we might need to provide pre-built binaries for development or only build on demand. At this stage, developers should be able to run and test the UI with Orpheus with minimal extra steps (maybe `pnpm dev` triggers building Orpheus in watch mode or uses a prebuilt).

## **Phase 2: Feature Integration & Bridging**

*Goal:* **Leverage Orpheus’s capabilities in the UI and migrate or augment features to use the local engine.** In this phase, we introduce actual user-facing features powered by Orpheus core, replacing or complementing what was previously done via external services or not possible before. We also ensure the UI design patterns (components, state management) can accommodate the engine integration.

- **Local Session Management:** Implement new UI flows that demonstrate Orpheus’s session-handling features. For example, add a “Session Manager” panel in Shmui:
    - The user can load a session JSON file (using Orpheus core to parse it) and see a list of tracks or basic session info displayed on the UI.
    - Provide controls to add or remove a track via the UI, calling Orpheus under the hood to update the SessionGraph, and then reflecting the updated track list in React.
    - This showcases a **real integration**: the UI is now not just calling a dummy function, but managing state through Orpheus. We may implement a React context or hook that holds an Orpheus session handle and provides methods to operate on it.
    - Keep the scope limited: we’re not building a full DAW UI, but enough to prove we can create/edit some session data. For instance, a simple form to input BPM and length and a button “Generate Click Track” (see below), or a list of tracks that can be appended to.
    - **Interface design:** Perhaps introduce an `OrpheusClient` in the `@orpheus/client` package that wraps calls to either the WASM or Node addon. The UI can use this client (which could manage a singleton Orpheus instance) to perform operations. This abstraction will hide whether it’s WASM or native underneath, and could manage async work (like offloading calls to a WebWorker if needed).
- **Audio Generation Integration (Click Track):** Use Orpheus to generate audio content and play it in the UI:
    - Implement a feature where the user inputs some parameters (e.g., tempo, bars) and clicks “Render Click Track”. The Orpheus core should produce a WAV file (e.g., via its `orpheus_minhost` logic) and return the file path or data[GitHub](https://github.com/chrislyons/orpheus-sdk/blob/78085c4c9711c2e3cba8dd46bcf59207ca0c321f/README.md#L113-L121).
    - In the browser context, since writing a file may not be straightforward, we could have Orpheus (if WASM) return raw audio data or a URL to a virtual file. In an Electron context, Orpheus can write `click.wav` to a temp directory and then the UI can load it.
    - Integrate this with Shmui’s existing **AudioPlayer** component: for example, automatically load the rendered click track into the AudioPlayer so the user can listen to it. This tests end-to-end media flow: Orpheus generates audio -> UI plays audio using its standard component.
    - Ensure to handle this cross-platform (file path differences on Windows, etc.) and clean up any temp files if needed after playback.
- **Enrich Existing Components with Orpheus Data:** Identify parts of Shmui (ElevenLabs UI) that can optionally use Orpheus:
    - **Orb Visualization:** If the “Orb” component (animated 3D orb) can reflect an AI/agent state, perhaps use Orpheus’s transport or generation state. For example, when Orpheus is busy rendering audio or processing, we could set the Orb to a “thinking” or “speaking” animation state (if such states exist). When a click track is playing, maybe pulse the orb to the beat (using the known tempo).
    - **Voice Picker / TTS**: Shmui’s VoicePicker likely uses ElevenLabs APIs. Orpheus doesn’t provide TTS, so this might remain as is. But if there are any hooks (like retrieving audio for a voice preview), we might later integrate Orpheus if it gains such capability. For now, ensure that nothing in Phase 2 breaks the voice picker – perhaps allow it to function normally (still calling ElevenLabs cloud) unless a local voice engine is available.
    - **Timeline or Waveform Display:** If time permits, create a simple **session timeline view** using Orpheus data. For instance, if a session JSON is loaded, draw a rudimentary timeline of clips/tracks (even just text or blocks). This could use a canvas or just a list, and is mainly to showcase Orpheus’s data in the UI. It’s an optional stretch goal, but would further validate that the UI can reflect complex data from Orpheus.
- **Backward Compatibility & Toggles:** At this stage, some features may have two implementations (e.g., a cloud-based one and a local one). Provide configuration or runtime toggles:
    - E.g., a setting “Use Local Engine” that enables the Orpheus-powered path. If off, the app could still function using the cloud or stubbed behavior as originally. This way, if a new Orpheus feature malfunctions, the user (or developer) can fall back.
    - By the end of Phase 2, we might intend to default to the Orpheus path, but having a kill switch is prudent during testing.
- **Docs & Storybook Updates:** Expand documentation to cover these new features:
    - Add documentation pages (MDX in Storybook or a `docs/` section) explaining how to use the new Orpheus integrated features. For example, “Using the Orpheus Engine for Local Sessions” with code samples.
    - If the UI library is intended for external users, document new component props or contexts (e.g., if a component now requires an `orpheusClient` prop or context provider).
    - Keep the Storybook stories for existing components (VoicePicker, etc.) working. If some components now optionally use Orpheus, ensure the story can either mock Orpheus or toggle it. Possibly create new stories that demonstrate components in an “Orpheus mode”.

*Validation:* By the end of Phase 2, **user-visible features powered by Orpheus should be functioning**:

- **Session import/export:** Load a sample session JSON through the UI and verify that it is parsed by Orpheus (perhaps by comparing the known content – number of tracks, track names – against what Orpheus provides). The UI should show the session data correctly.
- **Click track generation:** Using the UI control, generate a click track and ensure a WAV is produced and can be played in the AudioPlayer. Manually verify the audio (tempo and length) is correct.
- **Track operations:** If the UI allows adding/removing tracks, confirm that these operations are reflected in Orpheus’s internal session state (e.g., after adding a track via UI, an Orpheus API call lists the new track). Removing should also update state without errors.
- **Orb and UI reactivity:** If the Orb or other components are tied to Orpheus state, check that they react as expected (e.g., Orb pulses on playback beats, or changes color when Orpheus is busy).
- **No regressions in other features:** Test the original features:
    - VoicePicker: can still fetch and play voice previews as before (using cloud API, unless changed).
    - AudioPlayer: still plays normal audio files (e.g., a sample MP3 in the docs) with all controls working.
    - Any other UI demos in the docs (Orb demo, etc.) still function if Orpheus features are turned off.
- **Performance:** Using the integrated app, ensure that Orpheus operations do not freeze the UI significantly. For instance, generating a 2-bar click track (a quick operation) should return almost instantly. Loading a moderate session file should not hang the UI thread for long. If any heavy operation is slow, note it for Phase 3 optimization (e.g., we might move it to a WebWorker or background thread).
- **Cross-platform testing:** Run through the new features on all target environments:
    - In a pure web build (if supported via WASM) on Chrome/Firefox/Safari – ensure the WASM loads and executes correctly.
    - In an Electron/Node environment – ensure the native module loads on Windows, Mac, Linux without missing dependencies. On Windows, check that the `.node` addon has no DLL issues; on Mac/Linux, ensure file paths and casing issues are handled.
- The completion of Phase 2 should demonstrate real value: the combined system does something it couldn’t do before (local session handling and audio generation), with all core features validated.

## **Phase 3: Unified Build, Testing, and Release Preparation**

*Goal:* **Unify the tooling, CI/CD pipeline, and quality checks for the monorepo, and prepare for releasing the integrated packages.** Phase 3 focuses on cleaning up tech debt, ensuring all automated processes treat the repo as one cohesive unit, and optimizing performance or build as needed. It’s about going from a working integrated prototype to a polished, maintainable product.

- **Merge CI Pipelines:** By now, we likely maintained parallel CI workflows; in this phase we consolidate them. Develop a single **GitHub Actions workflow** (`ci_pipeline.yml`) that can build and test everything in one run (with matrix for OS where needed). Key points for the new CI:
    - Use the monorepo’s package manager (PNPM) to install all dependencies in one go.
    - Build all packages: run the root build script (which triggers building of Orpheus core and the UI).
    - Run linters, format checks, and type checks across the repo.
    - Run tests for both C++ and JS: possibly run C++ unit tests on one representative OS (e.g., Linux) and run JS tests on at least one OS, while essential integration tests (like ensuring the addon loads) run on all. The matrix can ensure the native build works on all OS, even if not running all tests on each.
    - Include any special steps such as generating Storybook (maybe build it to ensure no errors) or running a bundle analyzer to keep an eye on bundle size (especially if adding WASM bloats it).
    - Ensure caching is used for dependencies and perhaps compiled objects to speed up CI (PNPM cache, ccache for C++ if beneficial).
    - The unified pipeline should be the gate for pull requests, ensuring that any change in either UI or core does not break the other.
- **Comprehensive Testing:** Increase automated test coverage for the integrated system:
    - Write **integration tests** in the JS side that specifically exercise the engine integration. For example, a Jest or Mocha test that spins up a headless instance of the app or calls the Orpheus client functions (simulate loading a session, verify output). If possible, run a headless browser (via Playwright) to simulate a user clicking the new UI buttons and assert on results, though that might be complex for CI.
    - Ensure Orpheus’s own C++ tests still run in CI. Possibly incorporate them via CTest called from an npm script.
    - Set up coverage reporting for both C++ and TS if feasible. At least, collect coverage for TS/TSX code and ensure critical integration code is tested. For C++, we might run with coverage flags locally or ensure logic is covered by existing tests.
- **Performance and Build Optimization:** Now that everything works, profile and optimize:
    - **Bundle Size:** Evaluate the size impact of Orpheus on the front-end bundle. If using WASM, the `.wasm` file might be several MB. We might decide to load it on-demand (e.g., only when a feature is used, using dynamic `import()` so it doesn’t bloat initial load). If too large, consider stripping unneeded parts of the core for the web build (for example, exclude the REAPER adapter or other unused code from the WASM build).
    - **Speed:** If any Orpheus calls were identified as blocking the UI (e.g., a heavy compute), move them to a Web Worker or Node worker thread. We can introduce that now that functionality is settled. Perhaps encapsulate Orpheus WASM in a Worker that communicates via messages, so the main thread isn’t stuck during processing.
    - **Memory:** Test for memory leaks or bloat. For Node, run the binding under AddressSanitizer/Valgrind in CI to ensure no leaks (we might have done this in Phase 1, but do a thorough check now). For WASM, ensure repeated usage doesn’t balloon memory (maybe call a function 1000 times and see if memory grows).
    - Clean up any **temporary or toggle code** that is no longer needed. If we are confident in the Orpheus features, we might remove the fallback toggles and deprecated paths now (or we can leave them in until after release, depending on strategy). Any code marked for removal (like old ElevenLabs integration pieces) can be stripped out in this phase if it’s safe.
- **Finalize Storybook & Component Library Quality:** Ensure the UI library is in top shape:
    - If not already done, integrate **Storybook** fully with the monorepo, so it can display components that use Orpheus. This might involve mocking Orpheus or providing a fake Orpheus client in the Storybook context. We want to maintain the ability to visually test components in isolation.
    - Possibly set up **visual regression tests** for the UI (using a tool like Chromatic or Storybook’s testing features) now that the UI might have more states.
    - Review the UI components for any inconsistencies introduced by Orpheus integration (for example, if a prop became unused, clean it up; if a new prop was added for Orpheus, ensure it’s optional and has defaults for backwards compatibility).
- **Release Versioning and Packaging:** Configure how we will release the new integrated packages:
    - Determine version strategy: using **changesets**, we can either maintain independent versions for each package or move to a single unified version. A simple approach is to version all packages in lockstep (since they are meant to work together). For instance, release version `1.0.0` of `@orpheus/shmui` (UI) alongside `@orpheus/engine-wasm` etc. Alternatively, core engine packages might have their own version if they will be used separately.
    - Update `changeset` config if needed to define relationships (maybe mark some packages as dev-only or private if not meant to be published, such as an internal docs app).
    - Prepare the **NPM publish workflow** in CI: possibly add a GitHub Action for release on tag or on merge to main with version bump, that publishes `@orpheus/shmui`, `@orpheus/engine-wasm`, `@orpheus/client`, etc. Ensure the package names and fields in each `package.json` are correct (including pointing to correct type declarations, specifying bundled files like the WASM or native binaries).
    - If applicable, prepare pre-built binaries for the Node addon for major OS (using GitHub Actions artifacts or `node-pre-gyp`) so that users installing `@orpheus/engine-electron` don’t always need to compile from source. This might be complex, but even documenting how to build is fine if we treat it as a developer SDK.
- **Documentation & Example Updates:** Complete all documentation:
    - Update the main README of the Orpheus SDK to mention the integrated UI and how it fits into the architecture.
    - Provide a guide for developers on how to use the new monorepo. For example, “Developing Orpheus UI: how to run Storybook, how to run C++ tests, etc.” so new contributors understand the layout.
    - If releasing publicly, create a migration guide for any existing users of either project. This would summarize breaking changes (from `breaking_changes.md`) and how to transition (for instance, how to switch from ElevenLabs UI to Orpheus UI package, how to build Orpheus core now that it’s merged).
    - Possibly include a simple example repository or codesandbox that uses `@orpheus/ui` to demonstrate installation and usage outside of our repo, to validate that our packages work standalone.

*Validation:* Phase 3 is complete when the unified pipeline is green and we have high confidence in quality:

- CI passes on all steps consistently (build, lint, type check, tests on all OS).
- All new features from Phase 2 are tested automatically and manually, and any performance issues are addressed (or ticketed for future improvements if minor).
- Verify that we can produce release artifacts: e.g., run a dry-run of `pnpm publish` locally (to a dry-run or local registry) to ensure packages are correct. If using a tag-based release, simulate it.
- Documentation reviewed: ensure no stale references to the old repo or ElevenLabs remain (unless noting them historically). The Storybook/docs site should reflect the “Orpheus SDK UI” identity rather than ElevenLabs.
- Confirm that developers can still do all daily tasks easily (spin up the UI dev server, run tests, build C++). Document any new requirements (e.g., need Emscripten installed, etc., in CONTRIBUTING docs).

## **Phase 4: Gradual Rollout and Post-Migration Support**

*Goal:* **Deploy the unified system in a controlled manner and retain fallback options until confidence is established.** In this final phase, we transition from development to releasing and using the integrated monorepo in production or wider distribution, while ensuring we can roll back if needed.

- **Beta Release & Testing:** Publish a beta version of the new packages (e.g., `@orpheus/shmui@1.0.0-beta.1`). Internally, start using this in a staging environment or have a few power users test it. Gather feedback on any issues.
- **Deprecate Old Repos:** Mark the separate `chrislyons/shmui` repo as read-only or archive it, with a note pointing to the Orpheus SDK repo for the latest code. Do the same for any Orpheus SDK distributions that are now changed (if Orpheus was separate for other users, indicate the move).
- **Monitor and Rollback Plan:** For any critical issues that arise in production, be ready to either:
    - Release a hotfix version from the monorepo if minor.
    - Or if something fundamentally fails, temporarily revert to using the pre-integration versions. (Because Orpheus was primarily internal, this likely isn’t needed for users, but internally one could keep an older build of Orpheus or the old UI if needed.)
    - Keep the feature-flag toggles in place initially so that if an Orpheus-powered feature misbehaves, it can be turned off without affecting the rest of the UI (e.g., disable local session management while leaving the rest of UI operational using cloud services).
- **Full Transition:** Once the beta testing period is successful, promote the release to stable (remove beta tag). Remove or clean up the fallback paths if we decide they’re no longer necessary. Officially announce the unified Orpheus SDK with Shmui UI as the default front-end environment.

Throughout all phases, we ensure each phase’s changes are **reversible or isolatable**:

- In Phase 0, we can always remove the integrated code if needed.
- In Phase 1, if the binding doesn’t work, we haven’t removed any existing capability – we simply don’t use it and nothing breaks for users.
- In Phase 2, if a specific integrated feature fails, we can toggle it off (e.g., fall back to not having that feature or using the older method).
- By Phase 3, most integration is done, but any issue should be caught in CI or staging. At this point, we resolve issues before release rather than rolling back.
- Phase 4 is about ensuring confidence; if something catastrophic were discovered after release, we could release a patch or even temporarily unpublish the broken package and advise using the previous separate versions.

Finally, once stability is confirmed, we will operate in the new unified model going forward, and all development will happen in the Orpheus SDK monorepo with Shmui UI as a first-class component.

## **Expected Breaking Changes** (`breaking_changes.md`)

Merging Shmui into Orpheus SDK introduces some breaking changes for developers (and possibly users) that we must document and mitigate:

1. **NPM Package Name Changes:**
*Change:* The Shmui UI library was originally under the ElevenLabs scope/name (for example, `elevenlabs-ui`). It will now be published as `@orpheus/ui` (or `@orpheus/shmui`) under the Orpheus scope. All import paths or package references will change to this new name. Likewise, if Orpheus core becomes available as an NPM package, it will use the `@orpheus` namespace (e.g., `@orpheus/core`, `@orpheus/engine-wasm`).
*Impact:* Any external project using the old Shmui package will not automatically get updates until they change the dependency to the new name. Import statements in code like `import { Component } from 'elevenlabs-ui'` will break.
*Mitigation:* We will consider publishing one last version to the old package that simply re-exports everything from the new package, with a warning, to smooth the transition. Documentation will include a clear migration step: “Replace imports of `elevenlabs-ui` with `@orpheus/ui`”. We will also provide a codemod (see Codemods section) to automate this renaming in user code.
2. **API and Component Prop Adjustments:**
*Change:* As we integrate Orpheus, some React component APIs may change. We aim to keep Shmui’s ElevenLabs-derived components API-stable, but there could be minor differences. For example, if we introduce a context for Orpheus, some components might require wrapping in a provider. Or if we added optional props (like a session ID for an audio component to know which session to use), using the component without those might have different behavior. We might also unify naming conventions (ensuring anything Orpheus-specific is clearly named).
*Impact:* If a component prop is renamed or its behavior changes, consuming applications might need to update their code. For instance, if `VoicePicker` previously expected a list of voices via props but now can fetch from Orpheus, the usage might change (though ideally we keep the prop for backward compatibility and just make it optional).
*Mitigation:* Strive to make changes in a backward-compatible way. Continue to accept old props and usage, with new functionality being optional. If any prop is to be deprecated or changed, mark it with JSDoc `@deprecated` and handle both forms in code for now. Document these changes in a **Migration Guide** and summarize in the changelog. Provide codemods for simple mechanical prop renames if we do them.
3. **ElevenLabs CLI / Component Registry Deprecation:**
*Change:* ElevenLabs UI had a workflow where developers could use a CLI (e.g., `npx @elevenlabs/agents-cli add component`) to fetch or update UI components from a registry. In the unified Orpheus UI, we may not maintain this workflow (especially if we move to publishing on NPM). The JSON registry and CLI integration might be discontinued.
*Impact:* Developers who were using the ElevenLabs CLI to manage UI components will have to switch to using our NPM packages (or another mechanism). This is a significant change in how updates are consumed – from a dynamic pull to a static package dependency.
*Mitigation:* Provide our own simplified CLI or script if similar functionality is needed (for example, a script to copy template code for a new component, or hosting a JSON registry on GitHub Pages if we want to keep that concept). Clearly communicate the change: update documentation to instruct users to install `@orpheus/ui` via package managers. Possibly coordinate with ElevenLabs UI project if open source – since Shmui is a fork, upstream changes won’t directly apply anymore, which should be noted.
4. **Build and Installation Process Changes:**
*Change:* Orpheus SDK’s build being merged means that building the C++ core might require Node toolchain steps and vice versa. For those used to building Orpheus C++ from source, now the source is in a monorepo with JS build tools. Conversely, UI developers now have a C++ compilation step when working with the library.
*Impact:* Internal developers will need to adjust to new commands (e.g., running `pnpm install` instead of just opening VSCode for C++). If Orpheus was open to external C++ developers, they now have to fetch a larger repo and possibly install PNPM and Node to build the whole thing. This could raise the bar for C++ contributors. Also, paths to includes or library output might have changed (e.g., `#include "orpheus/session.h"` might remain the same internally, but the location of built libraries changed if any).
*Mitigation:* Continue to support standalone builds of the core where feasible. For example, we can keep Orpheus’s core CMakeLists.txt functional so that a developer can still build the C++ library alone by invoking CMake in `packages/orpheus-core`. Document this in the README (e.g., “For core development only: you can still build using CMake as before, see instructions...”). Over time, we might fully embrace the monorepo toolchain, but we can provide a transition period. Also, ensure the CI or scripts set up everything automatically (so one command builds all, to simplify usage).
5. **Removal or Deprecation of Redundant Components/Features:**
*Change:* If there were any overlapping features between Shmui and Orpheus, we will remove one side. For example, Orpheus had a JUCE-based demo app (and perhaps some minimal UI) – now that Shmui covers UI, we might drop that demo. Similarly, if Shmui had any placeholders for functionality now provided by Orpheus, those might be removed. The REAPER integration in Orpheus might be deemphasized if focus shifts to the browser UI.
*Impact:* The JUCE demo removal means anyone relying on it (likely only internally) won’t have it after integration. The REAPER extension, if not maintained, could affect users who used Orpheus in REAPER (again, likely a small set).
*Mitigation:* Mark such features as deprecated rather than outright deleting in the first integrated release. For example, keep the REAPER adapter building but note that it’s no longer a primary focus. Possibly spin it out if needed to a separate repo. If removing the JUCE demo, ensure that any functionality it provided (like certain tests or usage examples) is replicated in the new UI or documented elsewhere. Communicate these decisions in the project’s ROADMAP or changelog.
6. **License and Attribution Changes:**
*Change:* Bringing in Shmui (fork of ElevenLabs UI) means our repository now includes code originally from another source. We need to consolidate licenses (likely both Orpheus and Shmui are MIT or similar). We should carry over any required attributions (for shadcn/ui or ElevenLabs) into the unified repo’s LICENSE or NOTICE files.
*Impact:* If there’s any discrepancy in licensing, that could affect distribution. We must ensure the combined work’s license is clear to users.
*Mitigation:* Verify the license of ElevenLabs UI (presumably MIT). Include a mention in Orpheus SDK’s license file about the UI fork and its origin. Preserve any author attributions in the source headers. There’s no functional break here, but it’s important for openness and might be flagged by users if not handled.

Every breaking change above will be documented in the release notes. We will also provide tooling (codemods, etc.) to help users adapt to the changes where possible.

## **Codemods and Automated Migrations** (`codemods/`)

To ease the transition for any projects using Shmui or Orpheus, we will create a set of **codemod scripts** (using JS code transformation tools like jscodeshift) to automate common refactoring tasks:

1. **Import Namespace Update Codemod** – *renameImports.ts*:
**Purpose:** Update import statements from the old package names to the new `@orpheus` scoped packages.
**Details:** This script will find strings in import declarations that match `"elevenlabs-ui"` (or any other old package like `@elevenlabs/ui`) and replace them with `"@orpheus/ui"`. It will also handle scoped imports if any (e.g., `'elevenlabs-ui/components/XYZ'` if that existed, mapping them accordingly in the new structure, though we aim to have a single entry package). We will run this codemod on our own codebase as part of the merge (to update internal references in docs or tests), and we’ll provide it to external users in the `codemods/` folder with usage instructions (e.g., “run `npx jscodeshift -t codemods/renameImports.ts <your-src>`”). This reduces tedious find-replace for library consumers.
2. **Props and API Changes Codemod** – *updateProps.ts*:
**Purpose:** Adjust React component usages in user code to match any prop renames or API changes resulting from the integration.
**Details:** For each breaking prop change we introduce, the codemod will perform transformations. For example, if we decided to rename a prop `onValueChange` to `onChange` for consistency, the codemod finds JSX elements `<VoicePicker onValueChange={...}>` and renames the prop to `onChange`. If we removed a prop or made it optional, we might insert a comment or warning for the user. This codemod will be more specific and might not cover every case, but it will handle straightforward renames or signature changes we know about. We will test it on our own code (possibly simulating an older usage) to ensure it works.
3. **Remove Deprecated Usage Codemod** – *removeDeprecated.ts*:
**Purpose:** Remove or warn about patterns that are no longer needed after migration.
**Details:** For instance, if previously one had to initialize something for ElevenLabs (like importing an ElevenLabs SDK or setting up a provider that is now obsolete), this script can detect those patterns and either remove them or annotate them. Another example: if an older guide told users to wrap their app in `<AudioPlayerProvider>`, and we no longer require that because we manage it internally, the codemod could remove that wrapper from the code. This needs careful design to avoid deleting user code that might still be needed, so we’ll likely limit it to very clear-cut cases. In some instances, the codemod might just add a `// TODO: remove, no longer needed` comment to flag the user.
4. **Internal Adapter Extraction (if needed)**:
If we decide to remove the REAPER integration or other adapters from the main repo, we might create a one-time script to extract those to a separate repository for archival. This is more of an internal migration step than a user codemod, so it might not be provided to users generally. It could simply be a script that copies files and preserves history for that adapter into a new repo.
5. **CLI Workflow Migration Script**:
Not a codemod on source code, but if feasible, a script to assist projects that used the ElevenLabs component JSON registry. For example, a script could read a project’s `components.json` (if such exists from shadcn/ElevenLabs workflow) and output `npm install @orpheus/ui@latest` or similar instructions. This might be overkill, as most users can just install the new package, but we’ll at least document the process clearly.

All codemods will reside in a `packages/shmui/codemods/` or top-level `codemods/` directory. We will include README instructions on how to run them. They will be versioned if necessary (for example, if we have multiple future breaking changes, codemods might be named by version). Before releasing, we’ll test each codemod on example input to ensure they perform the intended transformations accurately.
 
Using these automated tools, we aim to save developers time and reduce human error when upgrading to the new unified Orpheus SDK UI.

## **CI Pipeline Updates** (`.github/workflows/ci_pipeline.yml`)

The CI pipeline will be updated to support the monorepo structure and ensure quality gates for both C++ and UI parts. Below is a high-level outline (in a GitHub Actions style pseudocode) of the new pipeline:

```javascript
name: CI

on:
  push:
    branches: [ main ]
  pull_request:
    branches: [ main ]

jobs:
  build-and-test:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        os: [ubuntu-latest, windows-latest, macos-latest]
    steps:
      - uses: actions/checkout@v4

      - name: Set up Node.js (for UI build)
        uses: actions/setup-node@v3
        with:
          node-version: 18
          cache: pnpm

      - name: Install PNPM
        run: npm install -g pnpm

      - name: Install Dependencies
        run: pnpm install
        # Installs all packages (UI and core dependencies)

      - name: Build Packages
        run: pnpm run build
        # Runs build for all packages:
        # - Compiles Orpheus core (via CMake or Emscripten)
        # - Builds UI (TypeScript compile, and maybe Storybook static build for docs)

      - name: Lint & Format Check (JS/TS)
        run: pnpm run lint

      - name: Type Check (TS)
        run: pnpm run type-check

      - name: Run UI Tests
        run: pnpm run test
        env:
          DISPLAY: :99  # if tests require a GUI (browser tests)

      - name: Run C++ Unit Tests
        if: matrix.os == 'ubuntu-latest'
        run: pnpm run core:test
        # e.g., calls CTest or a script to run Orpheus C++ tests

      - name: Build Storybook (Docs) and Audit Bundle
        if: matrix.os == 'ubuntu-latest'
        run: |
          pnpm run docs:build  # build Storybook or Next.js app
          pnpm run bundle:analyze  # optional: output bundle size stats

      - name: Check Dependency Graph
        if: matrix.os == 'ubuntu-latest'
        run: pnpm run dep:check
        # e.g., uses madge to check for circular dependencies in JS, or ensures no unexpected deps

```

**Key points in the CI configuration:**

- **Matrix Builds:** We run the core build on all major OS (Linux, Windows, macOS) to catch platform-specific issues. The UI tests can run on one OS (Linux) since those are platform-independent, but the Node addon/WASM needs to be built on each to ensure compatibility.
- **Dependency Installation:** Using `actions/setup-node` with PNPM caching ensures we don’t re-download all Node modules on every run. We also need compilers on each platform (on Windows, perhaps run `choco install vsbuildtools` if not already present for CMake).
- **Build Step:** `pnpm run build` will orchestrate both C++ and JS builds. We will wire this command to call:
    - CMake to build Orpheus core (possibly through a script or a CMake wrapper in package.json).
    - `tsc` or Next.js build for the UI. If the UI is purely a component library, we might not need a bundle, just a type check and maybe Storybook build for docs.
    - Emscripten build for WASM if configured. We might separate these into multiple scripts (e.g., `core:build`, `ui:build`) and have `build` run both.
- **Lint/Format:** We’ll include both ESLint and Prettier for UI code. For C++ format, we may add a separate step or integrate a format check into `pnpm run lint`. For example, run `clang-format -n` on C++ files to ensure no diffs, and maybe `clang-tidy` for static analysis warnings.
- **Tests:**
    - JS tests (`pnpm run test`) will cover React unit tests and possibly integration tests. If we add end-to-end tests (with Playwright), we’d start a web server and run those in CI (needs xvfb for headless browser on Linux, indicated by `DISPLAY: :99` env).
    - C++ tests (`pnpm run core:test`) will likely invoke `ctest` on the built binaries. We choose to run full C++ tests on one OS (Ubuntu) for speed, trusting C++ portability for others.
- **Storybook/Docs build:** To ensure the docs site (which might be a Next.js app or Storybook static build) still compiles fine, we include a build step for it (and optionally deploy it as artifact or to GitHub Pages in a separate job). We also run a bundle analyzer to keep an eye on size growth.
- **Dependency Cycle Check:** Using a tool like Madge for TS to detect circular imports, and possibly ensuring our internal dependency graph (between packages) has no cycles (Turborepo can enforce that). This helps maintain healthy architecture.

Additionally, we will likely set up a **Release workflow** (not shown above) using Changesets. For example, on pushing a tag or merging a PR with a changeset, a separate GitHub Action runs `pnpm changeset version && pnpm publish` to publish new package versions. This ensures our CI pipeline remains focused on verification, while publishing is a controlled separate step.
 
By Phase 3, this unified CI pipeline will be in place and required to pass for all merges, giving us confidence that any integration issues are caught early.

## **Validation Checklists**

To ensure each phase achieves its objectives without regressions, we use the following checklists as gates before moving to the next phase. (These would be maintained in a `validation_checklist.md` file for tracking.)
 
**Phase 0 – Repository Setup Validation:**

- [ ]  **Monorepo Structure in Place:** Verify that the `packages/` directory exists with `shmui` inside, and that the workspace tooling (PNPM/Yarn) recognizes it. Running `pnpm install` should install dependencies for Shmui and not throw errors.
- [ ]  **Orpheus Build Unbroken:** Run Orpheus’s CMake build and tests in the new repository structure. All tests pass as they did pre-merge. If Orpheus code was moved (if we decided to put it under `packages/orpheus-core` in Phase 0), ensure include paths and CMakeLists are updated accordingly.
- [ ]  **Shmui App Runs:** Start the Shmui docs/storybook application from the monorepo (e.g., `pnpm run dev` in `packages/shmui`). Confirm the UI loads in a browser with no runtime errors and components appear as expected. Any asset paths or imports that might have changed due to relocation should be fixed.
- [ ]  **CI Passing (Separate Workflows):** The interim CI workflow that builds both projects should succeed on all platforms. Check that Windows and Mac builds of Orpheus pass as well, since path changes (if any) might cause issues on case-sensitive vs insensitive filesystems.
- [ ]  **No Lint/Test Regression:** Run linting and any tests for Shmui. If Shmui had no tests, at least run its build. Ensure no new lint errors introduced by combining repos (e.g., relative import path issues or config conflicts).

**Phase 1 – Basic Integration Validation:**

- [ ]  **Orpheus Binding Compiles on All OS:** The Node addon or WASM build introduced in Phase 1 should compile successfully on Windows, Mac, and Linux. For Node addon, confirm the `.node` binary is produced; for WASM, the `.wasm` file is output. Try loading the module in each environment (require it in Node on each OS, instantiate WASM in a headless browser environment) to ensure no load errors (missing symbols, etc.).
- [ ]  **Smoke Test Orpheus Call:** Call an exposed Orpheus function and verify correctness. For example, execute a small script: `import orpheus from '@orpheus/engine-node'; console.log(orpheus.getVersion())` and ensure it prints the expected version or data structure. Or use the UI test button to invoke a round-trip JSON validation and check the result matches input.
- [ ]  **No Memory Leaks/Crashes:** If possible, run the Node addon under a memory checker for a simple call (this can be done locally). Ensure that a basic use (load module, call a function, unload) does not leak memory or crash. For WASM, call the function repeatedly and watch memory in dev tools to ensure it’s stable.
- [ ]  **UI Integration Manual Test:** Using the development UI, click the test hook that calls Orpheus (e.g., “Get Core Version” button or similar). Confirm that the UI receives and displays the data without error. Check the browser console or Node console for any exceptions thrown during the call.
- [ ]  **Storybook/Docs Build Works:** Build or run Storybook with the new integration in place. If Storybook cannot handle the actual Orpheus module, use a mock for now, but ensure it doesn’t fail to compile. The presence of Orpheus code should not break the docs site generation.
- [ ]  **Backwards Compatibility Toggle:** Disable the Orpheus integration (e.g., by a flag or simply not invoking it) and run all UI functionality. Everything should work exactly as it did before Phase 1. This ensures our additions are additive and don’t interfere unless active.

**Phase 2 – Feature Integration Validation:**

- [ ]  **Session Import/Export E2E:** In the UI, use the new Session Manager to load a known session JSON (we can use an example from `orpheus-sdk/tools/fixtures/`). After loading, verify the UI shows the expected session details (track count, names, etc.) and no errors occur. If possible, use an Orpheus API to fetch the session data back out and confirm it matches the original (ensuring no data loss in transit).
- [ ]  **Functional Testing of New UI Elements:** For each new feature implemented:
    - *Add/Remove Track:* Add a track via UI, then either call an Orpheus function to list tracks or observe the UI list to ensure the new track is present. Remove it and ensure it disappears and no orphan state remains.
    - *Render Click Track:* Click the render button. Confirm that audio starts playing (or a download link appears if that’s the design). Listen to the output – verify it’s indeed a metronome click at the specified BPM for the specified bars (subjective test, but important for trust). Also check that the file is handled properly (e.g., cleaned up if stored in temp).
    - *Transport Simulation (if added):* If there’s a play button to simulate transport, click it and ensure the UI’s beat indicator or progress bar moves in time. Check console logs or on-screen indicators to verify timing is approximately correct (we might not have a full audio engine to play sound, but we can simulate ticks).
- [ ]  **Orpheus-Enhanced Components:** If Orb or others were connected to Orpheus:
    - Simulate or initiate the state that should trigger the change (e.g., start playback to make Orb pulse). Visually confirm Orb responds (pulsing on beat).
    - If VoicePicker was left unchanged (still using cloud), ensure it still works by selecting a voice and playing preview – this is a regression test to ensure our integration didn’t break unrelated parts.
- [ ]  **Error Handling:** Intentionally cause an error in Orpheus (e.g., load a malformed session JSON) and confirm the UI handles it gracefully (shows a message, doesn’t crash). This tests that our integration has proper try/catch around Orpheus calls.
- [ ]  **Performance Checks:** Measure the UI responsiveness:
    - Loading a typical session JSON should take only a short moment (say < 500ms for a small session). If it’s slower, note it and ensure a loading spinner or feedback is shown.
    - While Orpheus is busy (e.g., generating audio), the UI should still be responsive (thanks to background threading or async calls). Verify that the UI can be interacted with (aside from the action in progress) and animations like the Orb or a loading spinner continue to animate (meaning the main thread isn’t blocked).
- [ ]  **Cross-Platform & Browser Compatibility:** Build/test the web app in multiple browsers (Chrome, Firefox, Safari). Ensure features like WASM instantiation work across them (Safari often is a good test for WASM issues). On Electron/Node, test on Windows and Mac that the click track file can be written and accessed (Windows path issues, Mac sandbox if applicable). Any platform-specific bug found here should be fixed now.
- [ ]  **Regression Testing Original Features:** Do a full run-through of the original ElevenLabs UI component features with Orpheus features turned *off* or on default:
    - Use VoicePicker to select a voice, ensure preview audio still plays correctly.
    - Use AudioPlayer with a regular audio URL to ensure we didn’t break playback controls.
    - Open any existing component demos (if the docs site has them) to ensure styles and interactions are unchanged.
- [ ]  **Phase 2 Feature Flags:** Toggle off the new features (if we built a config toggle). Verify the app behaves as if Phase 2 hadn’t been applied (which might mean some features simply vanish, but nothing crashes). This is a safety net if we need to deploy with features off.

**Phase 3 – Unified System Validation:**

- [ ]  **All Tests Passing in CI:** The unified CI pipeline should show green across all jobs. Verify that the matrix build ran on all OS and all expected checks (lint, unit tests, integration tests, etc.) executed. Address any flaky tests or timing issues now to avoid false failures.
- [ ]  **Coverage Targets Met:** If we set a goal (e.g., 80% coverage on critical modules), check the coverage reports. Especially ensure new integration code (binding layer, Orpheus client wrapper, etc.) has tests.
- [ ]  **Manual Full Regression:** Perform a manual end-to-end test on a fresh environment:
    - Follow the README steps as if you were a new developer: clone the repo, install, build, run the app. It should work by following documentation without extra tribal knowledge.
    - Launch the integrated app (if an Electron wrapper exists, test that; if just web, run it) on each platform manually to catch any last-mile issues (like a missing library on Windows that CI might not catch due to cached environment).
- [ ]  **Performance Profile Acceptable:** Run a production build of the UI and measure performance:
    - Time to load the app (with Orpheus WASM) – ensure it’s reasonable (if WASM is large, maybe lazy-load it and note that approach).
    - Memory usage after performing Orpheus-heavy tasks – ensure no obvious leaks (monitor the browser memory after loading several sessions or generating audio multiple times).
    - If any performance issues were identified and addressed (e.g., moved to Web Worker), verify those solutions are effective (the UI thread is free while background work happens).
- [ ]  **Final Package Audit:** Check the contents of the packages that will be published:
    - `@orpheus/shmui` (UI) package should contain the compiled JS/CSS assets, type definitions, etc., and not include unnecessary files (like we shouldn’t ship the entire Orpheus C++ source in it).
    - `@orpheus/engine-wasm` package should include the `.wasm` and a JS loader, and maybe type definitions for the API.
    - Verify that license files are included as needed (especially since we incorporate third-party code).
    - Ensure the package versions are set correctly and inter-dependencies have correct version ranges (e.g., `@orpheus/client` depends on `@orpheus/engine-wasm` version X).
- [ ]  **Documentation Complete:** Review all docs (README, Storybook docs, migration guide). Have a teammate follow the migration guide on a small project to test if our instructions and codemods suffice.
- [ ]  **Stakeholder Sign-off:** If applicable, get sign-off from product or team leads that the new integrated system meets requirements (e.g., does it provide the expected user experience improvements, and have we communicated any changes clearly).

**Phase 4 – Deployment Validation:**

- [ ]  **Beta Testing Feedback:** Collect feedback from beta users. List any bugs or UX issues found and ensure they are resolved or understood. Only proceed when no showstopper issues remain.
- [ ]  **Old Repo Archived:** Confirm that the old Shmui repository is archived or marked clearly to avoid confusion. Its README should point to the new monorepo.
- [ ]  **Release Process Tested:** Do a trial run of the release (either an actual beta release or a dry run). Ensure the CI publish workflow publishes to a npm registry (could be a test registry if not ready for public) without errors.
- [ ]  **Roll-back Plan in Place:** Document what to do if post-release something goes wrong. For instance, “if a severe bug is discovered, we will fix forward with a patch release; if that’s not possible, instruct users to revert to version X of Orpheus core and Y of UI (last separate versions)”. Since the separate versions might not be easily usable together, ideally fix-forward is the plan.
- [ ]  **Monitoring:** If the app will be used in production, ensure monitoring is updated. E.g., if any error logging or telemetry exists, it should cover new parts (like if Orpheus fails to render audio, it logs appropriately).
- [ ]  **Final Toggle Removal:** Once confident (after a certain period of stable usage), remove any feature flags or fallbacks in the next minor release, thereby fully committing to the integrated implementation.

By rigorously checking off these items, we ensure the migration proceeds with minimal disruption and that each phase’s goals are truly met before moving on.

## **Final Repository Structure**

After completing the migration, the Orpheus SDK monorepo will be organized into clearly delineated packages and support files. Below is the expected top-level layout (simplified for brevity):

```javascript
orpheus-sdk/ 
├── packages/
│   ├── shmui/                      # "Shmui" UI package (React component library + docs)
│   │   ├── src/                    # React components, hooks, etc.
│   │   ├── style/                  # Styles (e.g., Tailwind config, CSS files)
│   │   ├── stories/                # Storybook stories and MDX docs
│   │   ├── package.json            # Name @orpheus/ui (codename Shmui)
│   │   └── ... (build configs, etc.)
│   ├── client/                     # Orpheus client JS package (unified API for engine)
│   │   ├── src/                    # TypeScript wrappers that load WASM or call native
│   │   └── package.json            # Name @orpheus/client
│   ├── engine-wasm/                # Orpheus Core compiled to WebAssembly
│   │   ├── src/                    # (Could include C++ source or build scripts)
│   │   ├── build/                  # Output directory for .wasm and JS glue
│   │   └── package.json            # Name @orpheus/engine-wasm
│   ├── engine-electron/            # Orpheus Core as Node/Electron native module (if separate)
│   │   ├── src/                    # C++ binding source (binding.cpp, node-addon code)
│   │   ├── CMakeLists.txt          # CMake build scripts for the native addon
│   │   └── package.json            # Name @orpheus/engine-electron (or engine-node)
│   ├── engine-service/             # Optional service endpoints wrapping Orpheus
│   │   ├── src/                    # e.g., Express or RPC server using Orpheus core
│   │   └── package.json            # Name @orpheus/engine-service
│   └── orpheus-core/ (optional)    # Orpheus core C++ source as a package (if we move it)
│       ├── src/                    # Core C++ source code
│       ├── include/                # Core C++ headers
│       ├── CMakeLists.txt          # Build script for core library
│       └── package.json            # Might be private or used for coordination
├── apps/
│   └── shmui-docs/ (optional)      # If the docs site is a separate Next.js app, could live here
├── docs/                           # Additional docs or ADRs (e.g., migration guide, adapters info)
├── .github/
│   └── workflows/ci_pipeline.yml   # Unified CI configuration for builds and tests
├── package.json                    # Root workspace config (scripts to run CI tasks, etc.)
├── pnpm-workspace.yaml             # PNPM workspace definition listing packages/*
├── changeset/                      # Changesets config and change files
├── tsconfig.base.json              # Shared TypeScript config for all TS packages
├── .eslintrc.js                    # Shared ESLint config (with overrides for C++ maybe)
├── .clang-format, .clang-tidy      # C++ formatting and lint configs
└── CMakeLists.txt                  # Top-level CMake (could delegate to packages/orpheus-core)

```

**Notes on the structure:**

- The `packages/shmui` directory contains the UI library (previously the entire Shmui repo). It maintains its internal structure for components and uses Storybook for documentation. It preserves the ElevenLabs UI patterns (likely a similar file organization and design system) to ensure continuity for UI developers.
- We introduced new packages under `packages/` for the Orpheus engine in various forms:
    - `client`: a convenient JS layer that a front-end can use. This might dynamically choose between WASM or native at runtime (for example, try `engine-electron` and fall back to WASM in browser).
    - `engine-wasm`: contains build instructions to produce `orpheus.wasm` and perhaps a small JS file to load it. This package could be published so that web apps can depend on it.
    - `engine-electron` (or `engine-node`): contains the Node addon (compiled `.node` library). We might combine this with `engine-wasm` under one package with dual builds, but separating concerns is cleaner.
    - `engine-service`: optional, only if we want to provide Orpheus as a standalone service (e.g., an HTTP API). If not immediately needed, this can be added later.
- The original Orpheus C++ code, if not moved in Phase 0, could remain in the root (with directories like `/src`, `/include`, etc.). However, by the end state, we likely encapsulated it into `packages/orpheus-core` or at least reference it from the engine packages. The above layout shows an `orpheus-core` package for clarity, but it could be just the source referenced by engine-wasm/electron builds. In any case, the core code is present and built via CMake.
- Shared configs (TypeScript, ESLint) are at the root or in common config files, extended by each package as needed.
- The CI workflow and other meta files are updated to cover new paths.

This final structure achieves a unified repository where all pieces of the Orpheus SDK – front-end UI (Shmui), client libraries, core engine implementations – reside together and are released together. Developers can work on a feature that spans UI and engine within one repo, and the CI will test it as a whole.
 
**Rationale:** This organization balances separation of concerns (each subpackage has a clear responsibility and can be versioned) with integration (all in one repo for consistency). It maintains Shmui (now @orpheus/ui) as the dedicated UI layer, preserving its development workflow (Storybook, etc.), and places Orpheus’s runtime components in adjacent packages, enabling tight iteration loops between UI and engine changes. The final monorepo, with changesets and GitHub Actions, will allow us to maintain high velocity on UI development while ensuring the core and UI remain compatible through each change – fulfilling the goal of a unified, browser-first Orpheus SDK platform.