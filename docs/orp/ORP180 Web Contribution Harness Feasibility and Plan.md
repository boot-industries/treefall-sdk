# ORP180 Web Contribution Harness Feasibility and Plan

**Status:** Adopted feasibility plan and decision record. No prototype, code, or deployment exists; this document records the analysis, the decision, and the accepted engineering scope. Implementation (Phase 0) is a candidate for a future sprint.
**Date:** 2026-09-07
**Scope:** Whether a widget-class version of the OMP harness can be embedded in the orpheus-sdk website frontend to encourage collaborative participation on this open-source audio SDK, and the engineering shape that follows.
**Decision:** Build a read-only "ask this repo" onboarding widget, open to everyone. Build a write-capable harness only for a vetted (trust-gated) contributor circle. Do not build a public, anonymous write/PR funnel.

## Problem

Proposal: embed an OMP-harness widget into the project website so site visitors can participate in the project — explore the codebase, propose changes, and generate contributions. The OMP harness supports a browser collab client (live transcripts, tool cards, composer), an end-to-end-encrypted collab relay protocol, an stdio newline-delimited RPC mode, an in-process SDK embedding surface, and a precedent service (`robomp`) that runs per-issue `omp --mode rpc` sessions in isolated worktrees. The question is whether a public web widget built on these pieces is a viable contributor funnel for orpheus-sdk, and at what cost.

## Feasibility analysis

### The harness cannot run in a browser

OMP is host-native by construction: Bun runtime, real filesystem, `bash`/PTY/`eval` tools, LSP servers, Chromium automation, native crates. A browser cannot host it; it can only *drive* it. The viable shape is therefore: a website widget (transcript + composer UI) talking to a server-side session host that runs the agent against an orpheus-sdk worktree. "Embed the harness in the frontend" is not possible; "embed the session UI and control plane, with the harness executing on a server" is the only reachable form.

### Existing pieces cover most of the surface

- **`packages/collab-web`** (in the OMP harness) is already the widget-shaped browser client: streaming transcript, thinking, tool cards, subagent panel, composer with prompt/interrupt actions. Packaging it as an iframe or component is a packaging change, not a rewrite.
- **Collab protocol** already bridges host-native sessions to browsers: content-blind WebSocket relay, AES-256-GCM end-to-end, room secret confined to the URL fragment, view-only vs. full links (32-byte key vs. key plus 16-byte write token). Note: the production relay is not distributable — a self-hosted deployment must provide an equivalent server (an omp instance acting as collab host, or a new service publishing collab frames).
- **RPC mode** (`omp --mode rpc`) is the server-side control plane: prompt/steer/abort, host-owned tools, host-owned URI schemes, streamed session events, message pagination.
- **SDK embedding** (`createAgentSession`) supports in-process sessions with tool allowlisting (`restrictToolNames`, `toolNames`), in-memory or file-backed session managers, and explicit model/auth wiring.
- **robomp precedent** proves the pattern of server-hosted, worktree-isolated agent sessions with a least-privilege bot credential whose only write path is opening PRs.

### Free-provider economics remove the dollar objection

Routing the widget through a free flash-tier provider (GLM 5.3-flash via OrcaRouter) removes per-prompt token cost as the blocker. What it does not remove:

- **Rate limits** — free tiers cap throughput; the widget needs per-visitor quotas regardless.
- **Quality** — a flash-tier model on C++20 realtime audio proposes plausible-but-wrong changes. The dangerous failures in this SDK (allocation on the audio thread, ABI/export breakage, vtable insertion, ownership races at the worker/control boundary) are exactly the ones tests and novices miss.
- **Maintainer review cost — unchanged.** Every generated PR still costs maintainer time to judge. A funnel that maximizes volume at flash-tier quality maximizes review tax.

### Why a public write funnel is a dead end

Converging analysis (this record's conclusion; matches observed patterns in other AI-contribution experiments):

1. **It recruits intent, not talent.** Visitors who delegate authorship to a widget were not going to file well-formed issues or learn the codebase. The contributors whose judgment matters are already in the repo with a compiler; a one-click funnel serves a population that barely overlaps the useful one.
2. **The human adds near-zero differential value.** In a generate → human-approve funnel, the quality ceiling is the model's. The contributor approves blindly; the maintainer pays full review tax for model-authored code — a worse deal than the model's mistakes alone.
3. **Gate pipelines filter obvious noise, not wrongness.** An allowlist + narrow-test funnel rejects bad *submissions*, but filtering still costs time, and flash-tier code passes gates on safe paths while being wrong in ways the gates do not measure.
4. **Repo rules cap web-eligibility structurally.** Orpheus acceptance requires real-device records, hardware tests, and (for WASAPI) a Windows host — none of which can pass from a browser. The remaining web-eligible surface is SDK-core logic, the riskiest category in a realtime engine.
5. **GitHub already organically produces this spam class.** A free "AI writes your PR" button makes it systematic.

## Decision

- **Shape 1 — read-only onboarding surface, open to everyone.** The widget answers questions about the repository (architecture, routing/transport model, where to start, how to contribute) with read-only tools. Output is a *well-formed issue* or a summary — never a diff. Read-only output cannot enter the repo, so it cannot pollute it. For a niche SDK with a high contributor bar, this is the honest participation hook: "the agent is studying your issue right now."
- **Shape 2 — write-capable harness, trust-gated.** Restricted write access only for contributors with demonstrated context (merged PRs, sustained participation, maintainer vouching). Their sessions use the restricted toolset (no `bash`), their name is on the PR, and the agent assists authorship rather than replacing it. The noise population is anonymous by construction.
- **Explicitly not built:** the public anonymous write/PR funnel (Shape 3). Rationale above.

## Engineering considerations (accepted scope for Shapes 1–2)

- **Trust controls, in order of value:** tool allowlist without `bash` (`read`, `grep`, `glob`, `edit`, `write`, `lsp`) via `restrictToolNames`; read-only by default for anonymous visitors; plan-review step before any edit (human gates writes); disposable per-branch worktree per session with no host secrets mounted; least-privilege PAT whose only write path is opening a PR (robomp pattern).
- **Provider key lives server-side only**, never in the widget.
- **Abuse/budget:** per-visitor and per-account quotas, daily caps, rotatable room links. Queue semantics (steering/follow-up modes) prevent prompt collisions.
- **Identity and permission mapping:** collab's trust boundary is link possession, not accounts; the website's own auth must mint view/full links server-side. Revocation = new room.
- **Session lifecycle:** per-contribution sessions in disposable worktrees (never the maintainer's checkout; never child-app repos — shmui/fourtrack out of scope per repo rules). Session per issue/thread with `branch`/resume for forks; moderator steering for shared sessions.
- **Transport:** RPC over stdio on the server; WebSocket into browsers via the collab relay contract. HTTPS mandatory (WebCrypto requires a secure context).
- **Widget UX:** iframe of the collab-web client is the fast path (crypto/state isolation for free); needs an offline/empty host state, theming, presence, and sizing. Phase 0 can ship as live read-only transcript.
- **Disclosure floor:** prompts are visible to all participants and to the model provider; the page states "powered by <model>, contributions gated by the SDK test suite."

## Verification gates for any generated contribution

Clock the acceptance contract into the funnel rather than relying on maintainer review alone. Every candidate change must clear, in an isolated worktree:

```text
ctest --test-dir build --output-on-failure -R '^realtime_static_audit$'
ctest --test-dir build --output-on-failure -R '^cmake_find_package$'
ctest --test-dir build --output-on-failure -R '^docs_path_audit$'
python3 tools/shmui_juce_manifest.py --check
```

plus the narrow test executable exercising the changed path. Real-device, hardware, and Windows acceptance remain host-side and out of web scope; web-generated PRs must say so explicitly and must not claim unobserved verification (repo rule: do not claim verification that was not directly observed).

## Phasing

- **Phase 0 — today's pieces, zero risk:** read-only live-transcript widget. A host runs a view-only collab session for the repo; the site embeds the collab-web client. Visitors watch and read; no interactive surface, no write path, near-zero cost. Best participation hook per unit of risk.
- **Phase 1 — interactive, authenticated, restricted:** server-hosted sessions, read-only for anonymous visitors, restricted write for authenticated members, per-branch worktrees, quotas, plan-review approval step, PR-flow with least-privilege PAT.
- **Phase 2 — trusted contributor program:** per-thread sessions, role tiers (maintainers get heavier tools, vetted members get code tools), moderation, acceptance-rate telemetry per model to decide whether flash-tier editing remains enabled.

## Metrics that justify widening access

Track PR acceptance rate per model and per contributor cohort before widening any write scope. Widening is justified only when gate-passing proposals clear the review bar at a rate that measurably reduces maintainer load. If the free tier cannot clear the `realtime_static_audit` bar at acceptable rates, the widget remains a read-only/planning surface while a stronger model handles risky edits — such a change must be a new decision record, not a silent widening.

## Deferred / not-do

- Public anonymous write/PR funnel: rejected (this record).
- Self-hosted collab relay: required for any deployment; the production relay binary is not distributable — build the server-side host as part of the implementing sprint.
- WASAPI/Windows and hardware-gated acceptance via the web: structurally out of scope; host-side records only.
- Child-app (shmui/fourtrack/clip-composer) participation via this harness: out of scope per repo rules; the web harness is SDK-only.