# Developer workflow contract (Codex + human review)

This contract defines the minimum acceptable quality bar for implementation prompts and PR review in CosmoSim.

## Required response structure for implementation prompts

1. Brief design summary.
2. Exact file list with one-line rationale per file.
3. Explicit assumptions, invariants, and numerical conventions.
4. Full changed-file content.
5. Tests added/updated.
6. Benchmark or profiling hook added/updated.
7. Acceptance checklist.

## Mandatory engineering rules

- No pseudocode or TODO stubs in core logic.
- No unrelated rewrites.
- No hidden interface drift.
- No silent schema/config/provenance changes.
- No ambiguous unit/frame naming where confusion is possible.
- Do not claim completion when required preset paths are not all validated.

## Repair validation gate (required commands)

Run and report these exact paths for infrastructure repair work:

```bash
cmake --preset cpu-only-debug
cmake --build --preset build-cpu-debug
ctest --preset test-cpu-debug --output-on-failure

cmake --preset hdf5-debug
cmake --build --preset build-hdf5-debug
ctest --preset test-hdf5-debug --output-on-failure

cmake --preset pm-hdf5-fftw-debug
cmake --build --preset build-pm-hdf5-fftw-debug
ctest --preset test-pm-hdf5-fftw-debug --output-on-failure
```

If a dependency is missing or a path fails, report the exact failing command and error output and stop with a blocked status.

## Layering and ownership guardrails

- `core` (including config/state/provenance flows) must not silently gain dependencies on `analysis`, `physics`, or workflow orchestration code.
- New cross-layer coupling requires an explicit architecture decision entry in `docs/architecture/decision_log.md`.

## Reviewer checklist

- Are assumptions and limitations explicit?
- Are config/schema/restart/provenance implications documented?
- Do tests cover local invariants and a broader pipeline path?
- Is there at least one benchmark/profiling hook for costly paths?
- Are naming and ownership rules respected?
- Are all required preset paths either passing or explicitly blocked with exact command evidence?

## Documentation coupling rules

Any PR that changes behavior must update docs in-repo in the same patch:

- user-facing workflow updates (`README.md`, `docs/build_instructions.md`)
- configuration keys or validation semantics (`docs/configuration.md`)
- snapshot/restart/provenance schema behavior (`docs/output_schema.md`)
- validation expectations (`docs/validation_plan.md`)
- profiling/benchmark usage (`docs/profiling.md`)
- architecture decisions (`docs/architecture/decision_log.md`)
