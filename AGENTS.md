# Codex repair control plane (CosmoSim)

Scope: entire repository.

## Purpose
Keep infrastructure repair work consistent across Codex sessions. Follow this file first, then task-specific instructions.

## Required validation paths (run exact commands)

### 1) CPU-only debug path
```bash
cmake --preset cpu-only-debug
cmake --build --preset build-cpu-debug
ctest --preset test-cpu-debug --output-on-failure
```

### 2) HDF5 debug path
```bash
cmake --preset hdf5-debug
cmake --build --preset build-hdf5-debug
ctest --preset test-hdf5-debug --output-on-failure
```

### 3) PM + HDF5 + FFTW debug path
```bash
cmake --preset pm-hdf5-fftw-debug
cmake --build --preset build-pm-hdf5-fftw-debug
ctest --preset test-pm-hdf5-fftw-debug --output-on-failure
```

## Non-negotiable rules
- **Do not claim completion if a required preset path cannot run.** Report the exact failing command and terminal error, then stop and mark blocked.
- **No unrelated rewrites.** Change only files needed for the scoped task.
- **Layer boundary guard:** `core`/config/state/provenance code may not silently gain dependencies on `analysis`, `physics`, or workflow-only code.
- **Docs coupling:** any change touching config schema, snapshot/restart schema, provenance behavior, or architecture boundaries must update the matching docs in the same patch (`docs/configuration.md`, `docs/output_schema.md`, `docs/architecture/overview.md`, `docs/architecture/decision_log.md`, and/or `docs/architecture/developer_workflow_contract.md`).

## Reporting format for repair tasks
- Provide exact commands run, pass/fail per path, and blocker details (if any).
- If blocked, update a recap using `docs/repair_current_state_template.md`.
