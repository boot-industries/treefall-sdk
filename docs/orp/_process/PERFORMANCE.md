# Performance Budgets

Orpheus SDK enforces performance guardrails through the version-controlled [`budgets.json`](../budgets.json) file. The thresholds
reflect the latency, size, and event cadence expectations defined in ORP062 §1.0 and ORP066 §X.

## Budget Categories

- **Bundle Size** – Maximum and warning thresholds for distributable artifacts such as `@orpheus/shmui` and `@orpheus/engine-wasm`.
- **Command Latency** – 95th/99th percentile limits for critical contract commands (e.g., `LoadSession`, `RenderClick`).
- **Event Frequency** – Upper bounds on emitted events to preserve transport stability (`TransportTick`, `RenderProgress`).
- **Degradation Tolerance** – Allowed regression window before escalation.

## Governance

1. Proposed changes to `budgets.json` **must be reviewed** in a pull request with clear justification tying back to ORP objectives.
2. CI gates (Phase 1+) will consume this file to block regressions when thresholds are exceeded.
3. Performance reports should link to the relevant budget entry when exceptions are requested.

## Validation

To inspect individual thresholds:

```bash
jq '.budgets.bundleSize."@orpheus/shmui".max' budgets.json
```

This should return `1572864`, matching the 1.5 MB bundle ceiling.
