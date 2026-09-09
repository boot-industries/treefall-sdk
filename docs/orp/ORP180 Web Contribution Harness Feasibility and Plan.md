# ORP180 Web Contribution Harness Feasibility and Plan

**Status:** Consolidated strategy and feasibility decision record; not an implementation or deployment report. The original record reported no harness prototype. This revision does not assert that the proposed documentation pipeline, website, or community integration exists.
**Created:** 2026-09-07
**Revised:** 2026-09-09
**Scope:** Orpheus SDK documentation, community participation, and optional agent-assisted contributions through a unified website. SDK-only; this does not authorize changes in shmui, fourtrack, freqfinder, or clip-composer.
**Home document:** Incorporates the documentation-automation and SDK-forum companion notes. Keep this filename stable for existing links; the strategy now extends beyond a harness widget.

## 1. Decision and strategic implications

Build a useful documentation and community front door, not an AI-generated PR funnel.

1. **Make documentation obligations part of product changes.** Generate structural reference, execute examples, require explicit docs-impact metadata, and publish against released interfaces. Use an LLM for the remaining semantic work, not as the source of truth or sole obligation classifier.
2. **Use GitHub Discussions as the initial community system of record.** A proposed Astro/Starlight website provides `/docs/*` and `/community/*`; GitHub remains directly accessible. One discussion corpus, two interfaces, not two synchronized forums.
3. **Turn community questions into evidence for documentation improvements.** Preserve page, topic, and SDK-version context. Promote useful answers through review; never silently insert community or model output into canonical docs.
4. **Keep the public assistant read-only.** It explains the repository, cites versioned sources, and helps draft questions or issues. It cannot edit code, publish documentation, or submit PRs. Posting a user-approved discussion or issue belongs to a separately authorized website action.
5. **Allow agent-assisted writes only in a vetted contributor program.** GitHub login alone is not sufficient trust. Named contributors own proposals; isolated execution, plan approval, repository gates, and maintainer review remain mandatory.
6. **Reject the public anonymous write/PR funnel.** Cheap inference does not make review, operational security, or realtime-audio correctness cheap.

**Governing principle:** Code lives in GitHub. Canonical knowledge lives in reviewed, versioned docs. Community knowledge starts in Discussions. The website unifies access. Machines supply verifiable facts; humans own explanations; the community discovers missing explanations.

This changes the original build order: documentation contracts and ordinary community participation come first. The harness is an optional supporting service, not a prerequisite for asking questions, improving docs, or contributing through normal GitHub workflows. A read-only transcript demo is no longer the whole first phase and is not described as zero-risk.

## 2. Ownership and system boundaries

| Artifact or responsibility | Authority | Website/automation role |
|---|---|---|
| Implementation, public interfaces, schemas | SDK repository at a recorded revision | Read or generate from authoritative inputs |
| Documentation source, examples, release fragments, docs manifest | Prefer the SDK repository | Build, validate, preview, publish approved versions |
| Canonical explanations, tutorials, migrations | Reviewed documentation source | Render; propose editorial changes by PR |
| Questions, answers, ideas, recipes, announcements | GitHub Discussions initially | Query and mutate the same GitHub objects under appropriate authorization |
| Reproducible bugs and implementation work | GitHub Issues and PRs | Route, prefill, and link back to originating discussion |
| Security reports | Repository's private reporting procedure | Direct users there; never publish sensitive reports to Q&A |
| Discussion-to-page associations and search/cache data | Derived integration metadata | Rebuildable index, not an independent thread store |
| Agent transcript and worktree | Isolated session host | Temporary assistance, never canonical knowledge |

Prefer documentation source beside code: a feature PR can include implementation, tests, examples, and docs atomically. Independent website deployment does not require a separate docs repository. A separate repository is justified only by concrete access-control, editorial ownership, release-cadence, or localization needs. If used, product tags and manifests remain authoritative and cross-repo documentation updates require an explicit completion/release gate.

Astro/Starlight is the proposed presentation stack from the companion notes, not a claim about the current deployment. Its plugin directory lists TypeDoc and OpenAPI integrations [1]. Those support TypeScript and HTTP surfaces respectively; they do not solve C++ reference extraction. Choose the C++ generator by inspecting the SDK's existing documentation tooling before introducing another pipeline.

## 3. Documentation as a product contract

### 3.1 Three classes, three mechanisms

| Documentation class | Mechanism | Required evidence |
|---|---|---|
| Structural/reference | Generate from code or machine-readable contracts | Output traces to the declared release inputs; drift fails the build |
| Examples/procedures | Compile, validate, or execute | Displayed example matches the tested artifact and declared environment |
| Explanation/intent | Human-authored or AI-drafted, expert-reviewed | Reviewer confirms behavior, recommendations, tradeoffs, and version applicability |

Generate signatures, defaults, CLI flags, configuration keys, feature flags, error codes, and compatibility tables from authoritative contracts wherever available. Candidate inputs include public C/C++ headers, TypeScript types where applicable, OpenAPI, JSON Schema, CLI metadata, configuration schemas, and error registries. Do not invent a web API or a TypeScript package to fit the tooling.

When validation and documentation separately encode the same configuration rules, consider a shared schema instead of another synchronization agent. Do not convert every internal conditional into a schema merely for completeness. Platform/compatibility tables may be generated from qualification records, but must retain untested/unsupported distinctions: generation cannot manufacture hardware evidence.

### 3.2 Explicit docs-impact declaration

Require a machine-readable PR form or equivalent validated metadata. Proposed logical fields:

```yaml
docs-impact: narrative # none | reference | narrative | migration
docs-owner: sdk-maintainers
public-api-change: true
breaking-change: false
```

`docs-impact` identifies the highest editorial obligation; it is not permission to skip reference generation or example validation. A migration change may require all three. `none` requires a rationale. Resolve ownership to actual maintainers/teams during implementation rather than introducing a parallel ownership list.

CI combines the declaration with deterministic public-surface changes and the docs manifest:

| Condition | Required outcome |
|---|---|
| Detected public API change with `docs-impact: none` | Fail; reconcile metadata and obligation |
| Breaking change without migration guidance | Fail |
| New/changed configuration key without reference regeneration | Fail |
| Narrative obligation without a corresponding docs update | Fail; a linked issue alone is insufficient |
| Changed example or mapped executable surface | Compile/validate and run the applicable checks |
| New public surface missing from the manifest | Require mapping and owner review |

In a same-repo workflow, the documentation update is in the product PR. In a justified cross-repo workflow, require a linked docs PR for merge and block release publication until the matching docs are merged and validated. Avoid circular dependencies by validating docs against the candidate product revision before the release tag exists.

Use an LLM only as a secondary contradiction check: “The author marked this `none`; does the change contradict that?” Its uncertainty goes to an owner. It cannot waive deterministic failures or replace an explicit declaration. Headers/schema diffs cannot detect every behavioral change, so author responsibility and review remain necessary.

### 3.3 Stable snippets and executable documentation

Do not identify snippets by line ranges such as `foo.ts:81–104`. Harmless insertions can select the wrong code even if those lines still exist. Use stable symbols where extraction is unambiguous, named examples, or paired markers such as `// docs:start create-client` and `// docs:end create-client`.

The publication guarantee is: the docs display named example X from release Y; X exists at Y; it compiles against Y; applicable execution checks passed in the recorded environment. Missing/duplicate markers and unresolved symbols fail extraction. Line numbers may be shown as navigation aids, not used as identity.

- Prefer complete example files in the product repository and import their named regions into the docs build from the pinned release.
- Compile/typecheck API examples. Extract standalone Markdown blocks only when language, dependencies, and surrounding context are declared; label partial/pseudocode snippets honestly.
- Validate documented configuration against the actual schema.
- Exercise safe CLI procedures, such as dry-run commands, and compare meaningful expected output where deterministic.
- Test HTTP examples against an isolated integration environment only for actual HTTP surfaces.
- Execute selected examples and assert meaningful outcomes; snapshots are appropriate for stable observable output, not incidental formatting.
- C++ compilation is not proof of realtime safety or device behavior. Hardware-dependent examples need host-side qualification and explicit coverage labels, not simulated claims of acceptance.

### 3.4 A small documentation manifest

Add or extend a product-owned machine-readable manifest mapping public surfaces to source artifacts, docs destinations, verification mode, and owner. Reuse an existing equivalent if present. The following is a proposed schema shape, not a declaration that these illustrative paths exist:

```yaml
version: 1
surfaces:
  - id: api.transport
    source: [include/orpheus/transport.h]
    docs: reference/transport
    mode: generated
    owner: sdk-maintainers
  - id: example.transport
    source: [examples/transport.cpp]
    docs: guides/transport
    mode: executable
    owner: sdk-maintainers
  - id: guide.routing
    source: [include/orpheus/routing.h, examples/routing.cpp]
    docs: guides/routing
    mode: narrative
    owner: sdk-maintainers
  - id: migration.next
    docs: migrations/next
    mode: human
    owner: sdk-maintainers
```

Generated surfaces rebuild; executable surfaces test; narrative/human surfaces require editorial review. Schema pointers may identify configuration subtrees. Multiple entries may point to one docs page. Validate identifiers, source resolution, docs targets, and ownership. Treat dependency mappings as maintained architecture, not a magical detector: source moves, new surfaces, and undeclared semantic changes still need review.

### 3.5 Capture intent once; reuse it

Require a release-note fragment for user-visible changes with `type`, `area`, `summary`, and, for breaking changes, `migration`. Conventional commits or Changesets may supply release mechanics, but a terse commit title alone does not necessarily explain what users should do differently. Reuse the existing release mechanism rather than creating a second changelog authority.

One reviewed fragment can drive changelog entries, release notes, upgrade-guide suggestions, docs-impact validation, and drafting context. An agent's input priority is:

1. PR description and linked issue: purpose and user problem.
2. Release-note fragment: user-visible behavior and migration action.
3. Public API/schema diff: changed contract.
4. Changed examples/tests: supported use and evidence.
5. Implementation diff as supplementary evidence, not the starting point.
6. Relevant existing docs as the editorial baseline throughout.

Provide bounded, relevant context with source revisions. Large unrestricted diffs obscure intent. The model proposes a narrative delta and cites its evidence; a subject-matter expert approves explanations and migration reasoning.

### 3.6 Version synchronization is not content synchronization

Publish a documentation product assembled from a released interface, not a continuously mirrored `main` checkout. Record product tag and resolved commit, API/schema inputs, example revision and results, generator versions, narrative-docs revision, and the docs version they serve.

A patch release may use a major/minor narrative branch while reference and examples remain pinned to the exact patch tag. That relationship must be explicit and validated; a version selector alone is not version correctness. Unreleased previews must be visibly labeled and must not replace stable docs. Documentation-only corrections can deploy independently while retaining the same product pin and a new docs revision.

The release flow is: validate change obligations in the product PR; generate/test from the release inputs; assemble approved narrative content; check version consistency and links; publish the complete validated artifact. Deterministic publishing can be automated after gates pass. Missing required migration/narrative work blocks that release's docs promotion rather than producing a deceptively complete site.

If a separate docs repository is justified, a narrowly scoped GitHub App and restricted output paths can propose updates. Keep credentials out of model context and untrusted build jobs; generation and PR creation are separate permissions. No agent receives an unrestricted cross-repo write token.

## 4. Community front door backed by GitHub

### 4.1 Route by user intent

| Website entry | Destination |
|---|---|
| Ask how to use the SDK | Community Q&A |
| Report something broken | GitHub Issue with reproduction/environment fields |
| Suggest an API or feature | Community Idea; maintainer may promote to an Issue |
| Improve the implementation | Normal GitHub PR workflow; vetted harness is optional |
| Improve these docs | Source edit/docs PR, or a documentation question when the change is unclear |

Users should not need to understand GitHub's object taxonomy before participating. Keep authorship, timestamps, answered state, and “View on GitHub” visible. Maintainers may work entirely in GitHub. Direct external contributions remain open under repository policy; trust gating applies to hosted agent write capability, not to ordinary PR eligibility.

Private security reporting is an explicit exception to the public issue path. No assistant or community form should encourage posting credentials, private logs, or vulnerability details publicly.

### 4.2 One corpus, two interfaces

Proposed website routes include `/community/questions`, `/community/ideas`, `/community/show-and-tell`, `/community/documentation`, and `/community/announcements`, with individual thread pages. A small server/API layer mediates GitHub authorization, Discussions GraphQL operations, and docs-to-discussion metadata.

GitHub documents Discussions queries and an API available to authenticated users, OAuth apps, and GitHub Apps [2]. This supports the single-corpus design; it is not evidence that a custom frontend or its permission model has been tested. The implementation spike must verify creation, replies, votes, accepted answers, edits, deletion, close/reopen, and moderation against the chosen token type and GitHub's current permissions. Existing categories, Q&A formats, polls, and moderation should be reused where suitable rather than reimplemented for parity. Poll UI is not an initial requirement.

Initial categories:

- **Q&A:** answerable SDK usage questions.
- **Ideas:** open-ended proposals.
- **Show and Tell:** integrations, recipes, and demonstrations.
- **Documentation:** answerable questions and documentation feedback.
- **Announcements:** maintainer-authored posts.

Reading public material should not require a visitor account. Writing uses GitHub identity; do not introduce another account/linking system initially. Authorize each mutation against the current user's permissions. Authentication is not moderation authority and is never contributor-harness authorization. Evaluate a GitHub App with user authorization against OAuth scopes before selection; do not assume a nominal “discussion-only” OAuth scope exists. The GitHub guide documents broader repository scopes for OAuth access [2].

The frontend needs pagination, rate-limit/backoff behavior, sanitized rendering, permission-aware controls, spam/reporting paths, and explicit failure states. Cached content is derived: propagate edits, deleted content, answer changes, and moderation restrictions; do not resurrect removed material from stale search indexes. If GitHub is unavailable, keep static docs usable and show a clear community outage/link-out state rather than accepting untracked writes.

### 4.3 Page context and durable associations

Each relevant docs page offers “Was this helpful?”, “Ask a question about this page”, and a related-discussions list with answered/unanswered state. Prefill category, stable docs-page ID and URL, SDK version, package/component, and topic; let the user inspect context before posting.

These fields are application metadata, not assumed native Discussion custom fields. The implementation must define a visible structured body convention or equivalent durable representation keyed by GitHub node ID, with a rebuildable index for page/topic lookup. Preserve associations when docs URLs change. GitHub-originated threads without metadata remain valid and can be associated during triage.

Keep canonical docs and community results visually distinct. Related answers must display version applicability and link to their source; an accepted answer is community resolution, not proof of SDK correctness or applicability to every release.

### 4.4 Promotion, not synchronization

A maintainer can promote a reproducible bug or accepted feature idea into a GitHub Issue, link the issue back to the discussion, and annotate the thread “Tracked as GitHub issue …”. Preserve authorship and the original context; check for an existing issue before creating another. Do not mirror every reply, edit, or moderation event between distinct objects.

Repeated questions, unresolved questions, feedback, and useful recipes produce documentation-gap candidates. Group by page, topic, and SDK version. A signal might say: three users on the same release asked about transport setup; two remain unresolved. Counts are triage evidence, not an automatic truth or publication threshold.

The loop is: reader question or recipe; community discussion; candidate docs gap, product bug, or feature idea; maintainer triage; linked issue/PR; validated and reviewed docs update; link the resolution back to the thread. Agents may summarize clusters and draft improvements using this context alongside product change intent. Neither a popular answer nor an LLM summary bypasses review. The long-term goal includes search-discoverable questions with canonical accepted answers and reviewed recipes, not merely chronological chat.

## 5. Forum alternatives and decision triggers

The companion notes rank a GitHub Discussions frontend first, Discourse second, and Apache Answer third for this developer SDK. Preserve the alternatives without deploying multiple forums:

| Option | When to evaluate it | Cost/decision boundary |
|---|---|---|
| GitHub Discussions + website UI | GitHub identity is acceptable; maintainers already work in GitHub | Initial choice; custom UI still has auth, API, accessibility, caching, and moderation work |
| Discourse | Community needs independent identity, richer moderation/trust, email participation, profiles, notifications, search, tags/categories, or broader non-developer participation | Evaluate hosting, operations, login/SSO, API/webhooks, and solved-question support before migration |
| Apache Answer | Searchable, Stack Overflow-style Q&A, tags, voting, and canonical answers dominate over social discussion | Evaluate Q&A fit and integration/operations rather than building a general forum |
| NodeBB | Realtime community interaction is the leading requirement | Evaluate REST API/plugin fit and operational cost |
| Flarum | Lightweight, customizable self-hosting is the leading requirement | Evaluate extension dependencies and maintenance burden |

These are retained evaluation directions from the notes, not a current product-capability audit. Discourse's GitHub login, OAuth2/SAML/SSO options and API/webhook integration were suggested in the source material; availability, hosting tiers, extensions, and exact authorization must be checked before selection. Likewise, verify Answer, NodeBB, and Flarum features at evaluation time.

A larger community might organize SDK help by language, architecture, integrations, recipes, feature ideas, showcase, and announcements. Do not manufacture language-specific sections before those SDK surfaces and audiences exist.

Move to an independent forum only when identity friction, moderation limits, community composition, or support workflows provide evidence for it. If Discourse or Answer becomes authoritative, keep community conversation there and engineering in GitHub; promote with backlinks. Do not bidirectionally synchronize Discussions and forum comments, edits, answers, or moderation. A migration needs explicit URL/identity/history handling and a declared system-of-record cutover, not a permanent dual-write bridge.

## 6. Agent assistance and harness feasibility

### 6.1 Browser UI, server execution

The original feasibility analysis identified OMP as host-native: Bun, filesystem access, bash/PTY/eval, LSP servers, browser automation, and native components. The browser can display and control a server session; it cannot host the complete harness. The reachable design is a browser transcript/composer talking to an isolated server-side session host, not a harness executing inside the website frontend.

The following are inherited feasibility findings, not reverified implementation guarantees in this consolidation:

- `packages/collab-web`: browser transcript, thinking/tool cards, subagent panel, and composer with prompt/interrupt actions. An iframe is the initial packaging candidate; theming, sizing, presence, accessibility, and disconnected/empty-host states still need work.
- Collab protocol: content-blind WebSocket relay, AES-256-GCM encryption, room secret in the URL fragment, and view-only versus full links described as a 32-byte key versus that key plus a 16-byte write token. Recheck the selected OMP revision before relying on these details.
- `omp --mode rpc`: stdio newline-delimited server control plane for prompt/steer/abort, streamed events, pagination, and host-owned tools/URI schemes.
- `createAgentSession`: in-process alternative with `restrictToolNames`/`toolNames`, session-manager choices, and explicit model/auth wiring.
- `robomp`: precedent for per-issue RPC sessions in isolated worktrees and a bot-mediated PR path, not proof that worktrees alone isolate hostile execution.
- The original record states the production relay is not distributable. Plan for a compatible self-hosted relay/host and verify packaging/licensing before deployment; do not assume the production relay can be reused.

Select RPC or in-process embedding in the implementation spike rather than maintaining both. If using collab transport, use HTTPS and a compatible WebSocket relay. An iframe can separate UI state; it is not a substitute for sandboxing or a security review.

### 6.2 Public read-only assistant

The public assistant answers architecture, routing/transport, usage, and contribution questions using allowlisted public docs and repository content. Pin retrieval to the selected SDK release; label unreleased source explicitly. Cite sources and distinguish observed behavior from inference. Approved community answers may supplement retrieval but remain labeled community knowledge.

Outputs are explanations, plans, summaries, or drafts of well-formed questions/issues, never repository diffs or automatic submissions. A user explicitly approves any subsequent posting through the website's normal authenticated action. Do not convert every chat into a Discussion or Issue.

Anonymous users may use a quota-limited read-only service once abuse controls are proven; authenticated users can receive separate quotas. A view-only live-transcript demo can precede interactive questions, but must expose only intentionally public material and clearly indicate when no host is available. Read-only prevents direct repository mutation, not misinformation, prompt injection, information disclosure, or provider abuse.

### 6.3 Vetted contributor write sessions

Eligibility requires demonstrated context, such as merged PRs, sustained participation, or maintainer vouching. The contributor reviews the plan before edits and owns the resulting PR. A logged-in forum participant does not automatically qualify.

- Use disposable per-contribution/per-branch worktrees in an actual process/container/VM isolation boundary; never the maintainer's checkout or child-app repositories.
- Anonymous sessions receive only constrained public read/search operations. Vetted sessions may receive scoped `read`, `grep`, `glob`, `edit`, `write`, and selected LSP operations; no `bash` or `eval`.
- Tool names alone are not a security boundary. Restrict paths and URI schemes, disable executable/editor side channels, control LSP behavior, remove host secrets, and restrict network egress. Treat repository and discussion text as untrusted input.
- Keep provider credentials server-side. Give the model neither user tokens nor bot credentials. A separate narrowly scoped broker creates branches/PRs only after authorization; no direct protected-branch writes or autonomous merges. Prefer short-lived GitHub App credentials; a fine-grained PAT is a fallback only with equivalent isolation and documented scope.
- Run fixed acceptance jobs in a separate disposable runner without production credentials. “No shell in the agent” does not mean “no host-side tests.” Untrusted build scripts themselves can execute code.
- Bind sessions to accounts and roles on the server. Collab link possession is not account authorization. Mint view/full capabilities only after authorization; expiry/revocation must close existing access and invalidate write capability, not merely hide a button. The inherited new-room rotation mechanism needs an exercised revocation test.
- Use per-visitor/per-account quotas, daily provider caps, concurrency limits, bounded session duration, queue semantics for steering/follow-ups, and rotatable room links. Shared sessions need moderator steering and explicit participant visibility.
- Support per-issue/thread sessions and branch/resume for forks only with authorization and isolation preserved. Delete expired worktrees and retain only explicitly governed transcript/audit data.

Disclose the model/provider, what prompts and retrieved content are sent, who can see shared transcripts, retention, and the limits of verification. Transport encryption does not hide prompts from the execution host or model provider. Do not promise “contributions validated” merely because a test suite ran.

### 6.4 Why the anonymous PR funnel remains rejected

The original record proposed GLM 5.3-flash via OrcaRouter as a free-provider route. Current availability, pricing, limits, and quality are unverified here; the strategy must not depend on permanent free inference. Even at zero token price, hosting, moderation, abuse control, and maintainer review remain costs.

A one-click agent-authored PR can increase submissions without increasing contributor understanding. This is a risk assessment, not a claim that anonymous or novice contributors have no value. In C++20 realtime audio, plausible code may introduce audio-thread allocation, ABI/export breakage, vtable changes, or worker/control ownership races. Narrow passing tests do not prove these absent. Hardware and Windows acceptance further limit what a hosted workflow can establish.

The safe participation hook is help understanding and expressing a problem, followed by ordinary contribution or a vetted assisted session. Human approval without understanding is not a meaningful quality gate. Measure reduced maintainer load, not generated PR volume.

## 7. Acceptance and verification boundaries

For generated code contributions, preserve the original minimum gate set, to be checked against the then-current repository acceptance contract during implementation:

```text
ctest --test-dir build --output-on-failure -R '^realtime_static_audit$'
ctest --test-dir build --output-on-failure -R '^cmake_find_package$'
ctest --test-dir build --output-on-failure -R '^docs_path_audit$'
python3 tools/shmui_juce_manifest.py --check
```

Run these in a correctly configured and built isolated checkout plus the narrow test executable covering the changed behavior. Require that selected tests actually exist and execute; a zero-test result is not acceptance. A manifest check does not authorize editing a child app. This list is a floor, not a replacement for current repository rules or change-specific validation.

Real-device records, hardware tests, and WASAPI/Windows qualification remain host-side obligations. A server can coordinate suitable runners, but the browser itself supplies no such proof. PRs must record exact executed commands, revision, platform/device context, results, and unverified requirements. Required qualification must be completed before acceptance or the contribution must be scoped so it does not claim it.

Documentation changes additionally need manifest/source resolution, snippet/example checks, generated-reference drift checks, version consistency, links, and editorial review as appropriate. Community integration needs an actual end-to-end GitHub identity/permission test; harness integration needs isolation, revocation, quota, and failure-state exercises. None of those implementation checks has been performed by this document-only revision.

## 8. Phased delivery and exit criteria

All phases below are planned, not completed. Implementation requires a separately approved sprint. They replace the original widget-only Phase 0–2 sequence.

| Phase | Deliverable | Exit criterion |
|---|---|---|
| 0 — Foundation and decisions | Inventory existing docs/generators, public surfaces, examples, release process, site deployment, ownership, and GitHub category/auth constraints; select one representative SDK surface | Owner-approved manifest/PR contract and release-pinning design; demonstrated feasible generator and auth choices; no dependency on a model service |
| 1 — Contract-driven docs | Same-repo docs-impact/release-fragment workflow, representative generated reference and executable example, reviewed narrative, release-pinned site build | A controlled product change demonstrates missing-docs failure, valid example execution, correct versioned publication, and separation of stable versus unreleased content; expand coverage through the manifest |
| 2 — Community front door | Five-intent routing, Discussions categories, page context, website reads and authenticated writes, GitHub fallback and moderation | One thread created through the site appears as the same object in GitHub; reply/answer and permission changes are reflected; association, outage, and deletion behavior verified |
| 3 — Knowledge feedback and read-only assistance | Triage queue for docs gaps/recipes, reviewed promotion workflow, optional version-aware public assistant; optional view-only demo first | A real or controlled question leads to a reviewed docs improvement with backlinks; assistant citations, no-write boundary, quotas, disclosure, and unavailable-provider behavior exercised |
| 4 — Vetted contribution pilot | Server-hosted isolated sessions, explicit vetting, plan approval, fixed verification runner, brokered PR creation | A named vetted contributor completes an isolated proposal and gated PR; unauthorized writes and revoked sessions fail; maintainers review recorded evidence |
| 5 — Evidence-based expansion | Evaluate models/cohorts, optional role tiers and per-thread collaboration; reconsider forum platform only if warranted | Measured reduction in maintainer effort and acceptable correctness/abuse outcomes; any wider write scope or platform migration receives a new explicit decision |

Documentation and community deliver value even if phases 3–5 never ship. Do not widen write access merely because the website can authenticate users. Maintainer-only heavier tools are a separate reviewed role, not an automatic feature of the pilot.

## 9. Measures and stop conditions

- **Docs integrity:** mapped public-surface coverage; example pass/qualification coverage; missing obligation detections; release-to-docs lag; version mismatches and broken links.
- **Community usefulness:** time to useful/accepted answer, unresolved questions by page/version/topic, duplicate questions, successful issue promotions, and reviewed docs improvements resulting from discussions.
- **Assistant usefulness:** source/version correctness, user-rated utility, successful handoff to existing knowledge, provider availability, abuse incidents, and total operating cost.
- **Contribution quality:** gate-passing PR acceptance rate by model and contributor cohort; maintainer review minutes per accepted change; rework and post-merge defects, including realtime/ABI/ownership failures.

Capture a baseline and agree thresholds before the relevant pilot; this record invents no success percentages. A high merge rate alone can hide expensive rewrites. Widen only when proposals measurably reduce maintainer load without weakening acceptance. If a cheap model cannot satisfy the realtime/static and review bar, keep it read-only/planning; any stronger-model editing or scope expansion needs an explicit recorded decision. Pause writes for isolation failures, unbounded abuse, or unmet review capacity. Reconsider independent community hosting only on demonstrated need.

## 10. Deferred work, non-goals, and open implementation decisions

**Rejected:** public anonymous AI-write/PR funnel; arbitrary forum-to-docs auto-publication; an LLM as sole docs-impact classifier; line-number snippet identity; bidirectional forum/GitHub mirroring; publishing `main` as a released interface.

**Deferred:** independent forum hosting; secondary identity/linking layer; full forum feature parity; wider contributor roles; self-hosted collab deployment until the harness phase; provider/model selection and any free-tier assumption.

**Out of scope:** browser-only claims of physical-device or Windows verification; hosted edits in child-app repositories; autonomous canonical-docs publication of unreviewed prose; private security handling in public community threads.

**Decisions required by the implementing sprint:** current C++ extraction/build conventions; exact manifest and PR-form locations; docs ownership; whether a separate docs repo is genuinely necessary; release/version URL policy; chosen GitHub authorization model and category setup; metadata representation; hosting/retention/moderation responsibility; current OMP API/relay packaging and sandbox design; baseline metrics and pilot thresholds. These are not implemented placeholders: this document defines their acceptance boundaries, and no service is authorized to ship without resolving them.

Qdrant and Whitebox are not dependencies of this strategy. This consolidation used local source notes and public documentation; Qdrant was unavailable and Whitebox offline, and neither was accessed.

## 11. Consolidation record and evidence

This revision replaces three overlapping notes with one strategy home. The original filename and index target are retained. The companion files were local, untracked notes and are retired after coverage verification; their substance and source titles are preserved below. Git history preserves the original tracked ORP180 plan, not the companion files.

| Source | Preserved substance and resolution |
|---|---|
| Original ORP180 | Browser/server feasibility, collab/RPC/SDK/robomp findings, public read-only and vetted-write distinction, rejection of anonymous PR automation, isolation/identity/transport/UX/disclosure requirements, exact gate commands, host-only qualification, metrics and exclusions: sections 6–10. Overbroad zero-risk/free-cost/security assertions are qualified. |
| ORP180-A OSS SDK Docs Automation | Nine recommendations: explicit impact contracts; stable snippets; executable docs; schema generation; manifest; intent-first agent input; release fragments; version/content separation; same-repo preference. Integrated in sections 2–3, with agent-drafted narrative plus expert review and deterministic release publishing. |
| ORP180-B SDK Forum | GitHub-backed frontend, identity tradeoff, page metadata and accepted answers, five intents/categories, community-to-docs signals and promotion, platform alternatives, migration triggers and no dual synchronization: sections 4–5, tied to the harness in sections 6–9. |

The automation note cites an Aspire anecdote: 9 of 69 generated docs PRs closed in an earlier window and 82/82 merged after classifier refinement; it also suggests agents might handle the final 10–20% of semantic work. These are unverified source-note figures, not Orpheus measurements, forecasts, or acceptance targets. The retained implication is to encode obligations and change intent explicitly rather than rely on prompt tuning. The note's related GitHub App/safe-output and bounded-context recommendations are retained without treating the anecdote as evidence of this system's reliability.

**Verification scope:** Documentation consolidation only: source-topic coverage, Markdown structure, retained gate commands, index target, and removal of live companion-file references. No SDK code, website, CI pipeline, forum, or harness was changed or exercised. Future implementation remains at Phase 0; no device or model-quality evidence is claimed.

## References

[1] Astro/Starlight, “Plugins and Integrations.” [Online]. Available: https://starlight.astro.build/resources/plugins/. Accessed: Sep. 9, 2026. Used to confirm listed TypeDoc/OpenAPI integrations, not C++ integration readiness.

[2] GitHub, “Using the GraphQL API for Discussions.” [Online]. Available: https://docs.github.com/en/graphql/guides/using-the-graphql-api-for-discussions. Accessed: Sep. 9, 2026. Used to confirm Discussions API access, query/object model, and documented OAuth scope caveat; deployment permissions still require an integration exercise.
