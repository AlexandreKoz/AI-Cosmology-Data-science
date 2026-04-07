# Contributing

## Development workflow

1. Start from a clean branch.
2. Pick the **smallest preset** that exercises your change:
   - `cpu-only-debug` for default development.
   - `hdf5-debug` for snapshot/restart/provenance I/O work.
   - `pm-hdf5-fftw-debug` for PM/TreePM validation.
3. Configure, build, and run tests using presets.
4. Include the exact commands and outcomes in your PR description.

## Emergency repair mode rules

When a prompt is scoped as a narrow repair:

- Make minimal, auditable changes.
- Do not rewrite unrelated files.
- Do not land architecture churn as part of a narrow fix.
- Preserve established architecture discipline (SoA state, hot/cold separation, sidecar usage, typed config, provenance, stage-based integrator contracts).
- Do not claim closure unless the intended feature path compiles and the intended tests pass.
- Do not silently downgrade to CPU-only if an explicitly enabled dependency is missing.

## Interface drift control

- Public header/API changes require explicit migration notes in the PR.
- Configuration or preset behavior changes must update docs in the same patch.
- Schema/provenance-affecting I/O changes must document schema version and compatibility impact.

## Build and test quickstart

```bash
cmake --preset cpu-only-debug
cmake --build --preset build-cpu-debug
ctest --preset test-cpu-debug
```

See `docs/build_instructions.md` for dependency-enabled paths and troubleshooting.


## Repository hygiene guardrails

- Keep repository-root entries automation-safe (`[A-Za-z0-9._-]` only).
- Root-level non-code/binary artifacts are prohibited unless explicitly required by build tooling.
- Quarantine exceptional legacy artifacts under `docs/quarantine/` with written rationale.
- Before claiming a repair prompt is closed, run `./scripts/ci/check_repo_hygiene.sh` and the relevant preset guard path(s).

## Required repair verification commands

For emergency stabilization prompts, include command output for both baseline and intended feature path:

```bash
./scripts/ci/check_repo_hygiene.sh
./scripts/ci/guard_feature_paths.sh
```

If your change only affects one path, you may run a narrower subset, but you must state why in the PR.
