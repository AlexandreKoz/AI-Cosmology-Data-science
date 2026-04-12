# Infrastructure Diff Review Checklist

Use this checklist for repair-scoped PRs.

- [ ] Scope is infrastructure repair only (no new physics/numerics/science features).
- [ ] `core/` dependency direction is preserved (no upward include/use into analysis/io/physics/workflow without logged ADR whitelist).
- [ ] No second config system introduced; param-style UX still maps into typed validated config path.
- [ ] No string-literal policy branching added where typed enums/contracts already exist.
- [ ] Snapshot/restart/provenance schema behavior unchanged, or change is explicit (versioning + compatibility + docs + migration notes).
- [ ] Claimed closure has command-backed test evidence, or blocker is explicitly reported with failing command.
- [ ] Public interface changes under `include/cosmosim/**` include same-patch docs updates and migration notes.
- [ ] Diff avoids cosmetic-only refactors unrelated to repair objective.
