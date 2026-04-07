# Architecture decision log

## 2026-04-07 — ADR-REPAIR-FREEZE-001: Freeze repair evidence before touching production code

### Status
Accepted (stabilization phase)

### Context
The repository already contains significant architecture work (SoA storage, hot/cold split, sidecars, typed config, provenance, staged integration contracts). Entering emergency stabilization without a frozen baseline risks accidental churn and invalid claims of closure.

### Decision
Before any further repair implementation:

1. Capture a command-backed repair-state recap.
2. Capture a command-backed open-issues ledger.
3. Freeze the current public-interface baseline from `include/cosmosim/**`.
4. Record both default-path green status and feature-path red status explicitly.

### Consequences
- Positive: Later prompts can cite a single factual baseline instead of re-discovering repo state.
- Positive: Prevents CPU-only green path from being mistaken for HDF5 feature-path closure.
- Positive: Supports minimal, auditable fixes by tying each open issue to file+command+symptom.
- Tradeoff: Adds documentation overhead but no production-code churn.

### Evidence references
- `docs/repair_state_recap.md`
- `docs/repair_open_issues.md`
