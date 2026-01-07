# Orpheus SDK Documentation Index

> Home › Documentation › Index

## 🚀 Start Here

**New to Orpheus SDK?** Begin with these guides:

1. **[Getting Started](orp/_process/GETTING_STARTED.md)** – Install, build, and run your first session
2. **[Driver Architecture](orp/_process/DRIVER_ARCHITECTURE.md)** – Understand Service, WASM, and Native drivers
3. **[Driver Integration Guide](orp/_process/DRIVER_INTEGRATION_GUIDE.md)** – Step-by-step integration with code examples
4. **[Contract Guide](orp/_process/CONTRACT_DEVELOPMENT.md)** – Learn the command/event schema system

**For specific tasks:**

- Integrating drivers → [Driver Integration Guide](orp/_process/DRIVER_INTEGRATION_GUIDE.md)
- Adding features → [Contributor Guide](orp/_process/CONTRIBUTING.md)
- Migrating projects → [Migration Guide](MIGRATION_v0_to_v1.md)
- API reference → [API Surface Index](API_SURFACE_INDEX.md)
- SDK team handoff → [SDK Team Handoff](SDK_TEAM_HANDOFF.md) | [Sprint Summary](SDK_SPRINT_SUMMARY.md)

---

## ORP Implementation Plans & Technical Library

**📘 Current Active Plans (2025):**

| Document                                                        | Status                        | Focus                                                         | Timeline            |
| --------------------------------------------------------------- | ----------------------------- | ------------------------------------------------------------- | ------------------- |
| **[ORP068 – SDK Integration Plan v2.0](ORP/ORP068.md)**         | 🟢 Active (Phase 1 Complete)  | Driver architecture, contracts, client integration            | Months 1-6          |
| **[ORP069 – OCC-Aligned SDK Enhancements v1.0](ORP/ORP069.md)** | 🟢 Active (Planning Complete) | Platform audio drivers, routing, performance monitoring       | Months 1-6          |
| **[ORP074 – Clip Metadata Management Sprint](ORP/ORP074.md)**   | 🟡 Ready for Implementation   | TransportController trim/fade API, audio callback integration | Week 7-8 (2.5 days) |

**📂 [ORP Directory Index](ORP/README.md)** – Quick reference guide to all implementation plans

**Historical Context:**

| Document                                             | Focus                                                                                                    | Key Cross-References                                                                                                                                         |
| ---------------------------------------------------- | -------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| [ORP061 – Migration Plan](ORP/ORP061.md)             | Phase-driven roadmap for merging Shmui UI into the Orpheus SDK monorepo.                                 | Establishes Phase 0 baseline that is elaborated in ORP062 (contracts) and optimized in ORP063.                                                               |
| [ORP062 – Technical Addendum](ORP/ORP062.md)         | Normative contract, driver, and validation rules for the engine boundary.                                | Extends ORP061 Phase 0/1 requirements; informs latency and frequency limits codified in budgets.json (see orp/_process/PERFORMANCE.md) and references ORP066 refinements. |
| [ORP063 – Technical Optimization](ORP/ORP063.md)     | Operational alignment between migration strategy and contract architecture.                              | Builds on ORP061 sequencing; cites ORP062 schemas and feeds Phase 0 CI expectations referenced in ORP066/ORP068.                                             |
| [ORP066 – Implementation Refinements](ORP/ORP066.md) | Corrective refinements and guardrails for ORP065 execution, covering CI, packaging, and risk mitigation. | References ORP061/ORP063 for migration context; drives package naming rules summarized in orp/_process/PACKAGE_NAMING.md.                                                 |

### Additional Supporting References

- [ORP065 – Shmui Integration Plan](ORP/ORP065.md) – Historical baseline referenced by ORP066.
- [ORP067 – Task Numbering Appendix](ORP/ORP067.md) – Crosswalk between ORP task identifiers and repo workstreams.
- [ORP-CDX-013 – CI Revalidation](ORP-CDX-013-ci-revalidation.md) – Companion guidance for validating CI coverage post-migration.

## Migration Phase ↔ Technical Specification Map

| Migration Phase                        | Core Objectives                                                            | Authoritative Specs                                                      |
| -------------------------------------- | -------------------------------------------------------------------------- | ------------------------------------------------------------------------ |
| Phase 0 – Preparatory Repository Setup | Establish monorepo structure, parallel CI, package namespace baselines.    | ORP061 §Phase 0, ORP062 §§1–3, ORP063 §II, ORP066 §II (package hygiene). |
| Phase 1 – Tooling Normalization        | Wire Orpheus engine bindings into Shmui workflows, unify tooling.          | ORP061 §Phase 1, ORP062 §§4–5, ORP063 §III, ORP068 §II.                  |
| Phase 2 – Feature Integration          | Deliver UI experiences powered by Orpheus engine capabilities.             | ORP061 §Phase 2, ORP063 §IV, ORP068 §III.                                |
| Phase 3+ – Expansion & Governance      | Stabilize releases, enforce performance budgets, broaden platform support. | ORP068 §§IV–V, ORP063 §V, orp/_process/PERFORMANCE.md (budgets.json).                 |

## Navigation

- ▲ [Back to Repository Overview](../README.md)
- 📦 [Package Naming](orp/_process/PACKAGE_NAMING.md)
- 📈 [Performance Budgets](orp/_process/PERFORMANCE.md)
- 🧪 [CI Validation Checklist](ORP-CDX-013-ci-revalidation.md)

### Branch Protection (Manual Verification)

- Ensure **main** has required status checks: `build-cpp`, `build-ui`, `lint-cpp`.
- Require ≥1 reviewer, disallow force-push, enforce linear history.
- Record audit date in `orp/_process/GOVERNANCE.md`.
